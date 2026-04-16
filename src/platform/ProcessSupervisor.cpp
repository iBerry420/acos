#include "platform/ProcessSupervisor.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)

#define NOMINMAX
#include <windows.h>

#include <cstring>
#include <sstream>

namespace avacli {

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0)
        return L"";
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// Quote a single argv element per CommandLineToArgvW rules.
// See https://docs.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
std::wstring quoteArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        return arg;
    std::wstring out;
    out.push_back(L'"');
    for (size_t i = 0; i < arg.size(); ++i) {
        size_t bs = 0;
        while (i < arg.size() && arg[i] == L'\\') { ++bs; ++i; }
        if (i == arg.size()) {
            out.append(bs * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            out.append(bs * 2 + 1, L'\\');
        } else {
            out.append(bs, L'\\');
        }
        out.push_back(arg[i]);
    }
    out.push_back(L'"');
    return out;
}

// Build a single CommandLine string from an executable + args, properly quoted.
std::wstring buildCommandLine(const std::wstring& exe, const std::vector<std::wstring>& args) {
    std::wstring out = quoteArg(exe);
    for (const auto& a : args) {
        out.push_back(L' ');
        out.append(quoteArg(a));
    }
    return out;
}

// Build a UTF-16 double-null-terminated environment block, merging parent env.
std::wstring buildEnvBlock(const std::map<std::string, std::string>& overlay, bool inheritEnv) {
    std::map<std::wstring, std::wstring> merged;

    if (inheritEnv) {
        LPWCH envStrs = GetEnvironmentStringsW();
        if (envStrs) {
            for (LPWCH p = envStrs; *p; ) {
                std::wstring entry(p);
                auto eq = entry.find(L'=');
                if (eq != std::wstring::npos && eq > 0)
                    merged[entry.substr(0, eq)] = entry.substr(eq + 1);
                p += entry.size() + 1;
            }
            FreeEnvironmentStringsW(envStrs);
        }
    }
    for (const auto& [k, v] : overlay) {
        merged[utf8ToWide(k)] = utf8ToWide(v);
    }

    std::wstring block;
    for (const auto& [k, v] : merged) {
        block.append(k);
        block.push_back(L'=');
        block.append(v);
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

class Win32ChildProcess : public ChildProcess {
public:
    Win32ChildProcess(HANDLE hProc, HANDLE hReadPipe, DWORD pid, HANDLE hJob, bool termViaBreak)
        : hProc_(hProc), hReadPipe_(hReadPipe), hJob_(hJob), pid_(pid), termViaBreak_(termViaBreak) {}

    ~Win32ChildProcess() override {
        // Best-effort cleanup: closing the job handle with
        // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE terminates surviving children.
        if (hReadPipe_) CloseHandle(hReadPipe_);
        if (hProc_)    CloseHandle(hProc_);
        if (hJob_)     CloseHandle(hJob_);
    }

    bool alive() override {
        if (!hProc_) return false;
        DWORD code = 0;
        if (!GetExitCodeProcess(hProc_, &code)) return false;
        if (code == STILL_ACTIVE) return true;
        exitCode_ = static_cast<int>(code);
        return false;
    }

    int exitCode() const override { return exitCode_; }

    void sendTerm() override {
        if (!hProc_) return;
        if (termViaBreak_) {
            // CTRL_BREAK_EVENT is the only reliable "polite" signal for a
            // detached process group on Windows — CTRL_C_EVENT is explicitly
            // disabled by CREATE_NEW_PROCESS_GROUP.
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid_);
        } else {
            // No process group — fall back to WM_CLOSE then TerminateProcess.
            // Most console apps without a console will still exit, but the
            // supervisor will force-kill after its grace period.
            TerminateProcess(hProc_, 0xF291);
        }
    }

    void kill() override {
        if (hJob_)      TerminateJobObject(hJob_, 0xF292);
        else if (hProc_) TerminateProcess(hProc_, 0xF292);
    }

    long pid() const override { return static_cast<long>(pid_); }

    size_t readAvailable(std::string& out, size_t maxBytes = 4096) override {
        if (!hReadPipe_ || maxBytes == 0) return 0;
        DWORD avail = 0;
        if (!PeekNamedPipe(hReadPipe_, nullptr, 0, nullptr, &avail, nullptr))
            return 0;
        if (avail == 0) return 0;

        DWORD want = avail;
        if (static_cast<size_t>(want) > maxBytes) want = static_cast<DWORD>(maxBytes);

        std::string buf(want, '\0');
        DWORD got = 0;
        if (!ReadFile(hReadPipe_, buf.data(), want, &got, nullptr) || got == 0)
            return 0;
        out.append(buf.data(), got);
        return got;
    }

private:
    HANDLE hProc_ = nullptr;
    HANDLE hReadPipe_ = nullptr;
    HANDLE hJob_ = nullptr;
    DWORD  pid_ = 0;
    bool   termViaBreak_ = false;
    int    exitCode_ = -1;
};

} // namespace

std::unique_ptr<ChildProcess> spawnChild(const SpawnSpec& spec) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 1 << 16)) {
        throw std::runtime_error("CreatePipe failed");
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &sa,
                              OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = spec.mergeStderr ? hWrite : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = (hNul != INVALID_HANDLE_VALUE) ? hNul : GetStdHandle(STD_INPUT_HANDLE);

    std::wstring wExe = utf8ToWide(spec.executable);
    std::vector<std::wstring> wArgs;
    wArgs.reserve(spec.args.size());
    for (const auto& a : spec.args) wArgs.push_back(utf8ToWide(a));

    std::wstring cmdLine = buildCommandLine(wExe, wArgs);
    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    std::wstring wCwd = utf8ToWide(spec.workingDir);
    const wchar_t* cwdPtr = spec.workingDir.empty() ? nullptr : wCwd.c_str();

    std::wstring envBlock = buildEnvBlock(spec.env, spec.inheritEnv);

    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    if (spec.newProcessGroup) flags |= CREATE_NEW_PROCESS_GROUP;
    // CREATE_SUSPENDED so we can assign the process to a Job Object atomically
    // before it starts running and spawns grandchildren.
    flags |= CREATE_SUSPENDED;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmdLineBuf.data(), nullptr, nullptr, TRUE,
                             flags, envBlock.data(), cwdPtr, &si, &pi);

    CloseHandle(hWrite);
    if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

    if (!ok) {
        DWORD err = GetLastError();
        CloseHandle(hRead);
        throw std::runtime_error("CreateProcessW failed (error " + std::to_string(err) + ")");
    }

    HANDLE hJob = nullptr;
    if (spec.newProcessGroup) {
        hJob = CreateJobObjectW(nullptr, nullptr);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                    &limits, sizeof(limits));
            if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
                // Non-fatal: if we can't assign the job, we still have the
                // process handle and can TerminateProcess it directly.
                CloseHandle(hJob);
                hJob = nullptr;
            }
        }
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    return std::make_unique<Win32ChildProcess>(pi.hProcess, hRead, pi.dwProcessId, hJob, spec.newProcessGroup);
}

} // namespace avacli

#else // POSIX

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace avacli {

namespace {

// Locate an executable via PATH when it contains no slashes.
// Returns an absolute path, or the input unchanged if we can't resolve.
std::string resolveExecutable(const std::string& exe) {
    if (exe.empty() || exe.find('/') != std::string::npos)
        return exe;
    const char* path = std::getenv("PATH");
    if (!path || !*path) return exe;

    std::string p(path);
    size_t i = 0;
    while (i < p.size()) {
        size_t j = p.find(':', i);
        if (j == std::string::npos) j = p.size();
        std::string dir = p.substr(i, j - i);
        if (!dir.empty()) {
            std::string cand = dir + "/" + exe;
            if (access(cand.c_str(), X_OK) == 0)
                return cand;
        }
        i = (j == p.size()) ? j : (j + 1);
    }
    return exe;
}

// Render a C string "KEY=VALUE" from a pair. Caller owns the returned buffer.
char* dupKeyValue(const std::string& key, const std::string& value) {
    std::string joined = key + "=" + value;
    char* out = static_cast<char*>(std::malloc(joined.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, joined.c_str(), joined.size() + 1);
    return out;
}

class PosixChildProcess : public ChildProcess {
public:
    PosixChildProcess(pid_t pid, int readFd, bool ownPgid)
        : pid_(pid), readFd_(readFd), ownPgid_(ownPgid) {
        // Make the read end non-blocking so readAvailable() never stalls.
        int flags = fcntl(readFd_, F_GETFL, 0);
        if (flags >= 0) fcntl(readFd_, F_SETFL, flags | O_NONBLOCK);
    }

    ~PosixChildProcess() override {
        if (readFd_ >= 0) ::close(readFd_);
        // Reap if the caller forgot — avoid zombies. Non-blocking; if still
        // running at destruction we deliberately detach (supervisor should
        // always stopService() first).
        if (pid_ > 0 && !reaped_) {
            int st = 0;
            waitpid(pid_, &st, WNOHANG);
        }
    }

    bool alive() override {
        if (pid_ <= 0 || reaped_) return false;
        int st = 0;
        pid_t w = waitpid(pid_, &st, WNOHANG);
        if (w == 0) return true;
        if (w == pid_) {
            reaped_ = true;
            if (WIFEXITED(st))      exitCode_ = WEXITSTATUS(st);
            else if (WIFSIGNALED(st)) exitCode_ = 128 + WTERMSIG(st);
            else                      exitCode_ = -1;
            return false;
        }
        // w < 0: ECHILD typically means already reaped elsewhere.
        reaped_ = true;
        return false;
    }

    int exitCode() const override { return exitCode_; }

    void sendTerm() override {
        if (pid_ <= 0 || reaped_) return;
        if (ownPgid_) ::kill(-pid_, SIGTERM);
        else          ::kill(pid_,  SIGTERM);
    }

    void kill() override {
        if (pid_ <= 0 || reaped_) return;
        if (ownPgid_) ::kill(-pid_, SIGKILL);
        else          ::kill(pid_,  SIGKILL);
    }

    long pid() const override { return static_cast<long>(pid_); }

    size_t readAvailable(std::string& out, size_t maxBytes = 4096) override {
        if (readFd_ < 0 || maxBytes == 0) return 0;
        std::string buf(maxBytes, '\0');
        ssize_t n = ::read(readFd_, buf.data(), maxBytes);
        if (n > 0) {
            out.append(buf.data(), static_cast<size_t>(n));
            return static_cast<size_t>(n);
        }
        // n==0 → EOF (pipe closed). n<0 with EAGAIN → nothing right now.
        return 0;
    }

private:
    pid_t pid_ = -1;
    int   readFd_ = -1;
    bool  ownPgid_ = false;
    bool  reaped_ = false;
    int   exitCode_ = -1;
};

} // namespace

std::unique_ptr<ChildProcess> spawnChild(const SpawnSpec& spec) {
    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) != 0)
        throw std::runtime_error(std::string("pipe() failed: ") + std::strerror(errno));

    std::string resolved = resolveExecutable(spec.executable);

    // Assemble argv in the parent so the child doesn't allocate after fork.
    std::vector<std::string> argvStore;
    argvStore.reserve(spec.args.size() + 1);
    argvStore.push_back(resolved);
    for (const auto& a : spec.args) argvStore.push_back(a);

    std::vector<char*> argv;
    argv.reserve(argvStore.size() + 1);
    for (auto& s : argvStore) argv.push_back(s.data());
    argv.push_back(nullptr);

    // Build the child's env block. We always pass an explicit envp so the
    // child's env is deterministic (inheritEnv = merge parent first).
    std::vector<std::string> envStore;
    std::map<std::string, std::string> merged;
    if (spec.inheritEnv) {
        for (char** e = environ; e && *e; ++e) {
            std::string entry(*e);
            auto eq = entry.find('=');
            if (eq != std::string::npos && eq > 0)
                merged[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
    }
    for (const auto& [k, v] : spec.env) merged[k] = v;

    envStore.reserve(merged.size());
    std::vector<char*> envp;
    envp.reserve(merged.size() + 1);
    for (const auto& [k, v] : merged) {
        envStore.push_back(k + "=" + v);
        envp.push_back(envStore.back().data());
    }
    envp.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        int err = errno;
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        throw std::runtime_error(std::string("fork() failed: ") + std::strerror(err));
    }

    if (pid == 0) {
        // Child. Keep this short and allocation-free.
        ::close(pipefd[0]);

        if (spec.newProcessGroup) {
            // New process group: we can kill the entire tree with kill(-pgid).
            if (setpgid(0, 0) < 0) _exit(127);
        }

        if (!spec.workingDir.empty()) {
            if (chdir(spec.workingDir.c_str()) != 0) _exit(127);
        }

        // stdin: /dev/null so rogue readline() calls don't hang.
        int nullIn = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (nullIn < 0) _exit(127);
        if (dup2(nullIn, STDIN_FILENO) < 0) _exit(127);
        ::close(nullIn);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
        if (spec.mergeStderr) {
            if (dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
        }
        ::close(pipefd[1]);

        // Default signal disposition: unblock everything the parent blocked.
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, nullptr);
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        for (int s : {SIGINT, SIGTERM, SIGQUIT, SIGHUP, SIGPIPE}) {
            sigaction(s, &sa, nullptr);
        }

        execve(resolved.c_str(), argv.data(), envp.data());
        // If we get here, exec failed. Write a terse marker and exit — the
        // parent's readAvailable() will surface it as a log line.
        const char* msg = "avacli-supervisor: execve failed\n";
        [[maybe_unused]] ssize_t _w = ::write(STDOUT_FILENO, msg, std::strlen(msg));
        _exit(127);
    }

    // Parent.
    ::close(pipefd[1]);

    // Also setpgid from the parent to avoid the race where we deliver a signal
    // before the child has called setpgid in its branch.
    if (spec.newProcessGroup) {
        if (setpgid(pid, pid) < 0 && errno != EACCES && errno != ESRCH) {
            // EACCES = child already exec'd, which means it already set its own pgid — fine.
            spdlog::debug("setpgid(parent) for pid {}: {}", pid, std::strerror(errno));
        }
    }

    return std::make_unique<PosixChildProcess>(pid, pipefd[0], spec.newProcessGroup);
}

} // namespace avacli

#endif
