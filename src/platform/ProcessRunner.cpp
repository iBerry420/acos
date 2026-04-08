#include "platform/ProcessRunner.hpp"

#include <chrono>
#include <mutex>
#include <thread>

#if defined(_WIN32)

#define NOMINMAX
#include <windows.h>

#include <vector>

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

} // namespace

ProcessRunResult runShellCommand(const std::string& cwd, const std::string& userCommand, int timeoutSeconds) {
    ProcessRunResult result;
    if (timeoutSeconds < 1)
        timeoutSeconds = 1;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result.exitCode = -1;
        return result;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = (hNul != INVALID_HANDLE_VALUE) ? hNul : GetStdHandle(STD_INPUT_HANDLE);

    std::wstring wcmd = L"cmd.exe /c " + utf8ToWide(userCommand);
    std::vector<wchar_t> cmdLine(wcmd.begin(), wcmd.end());
    cmdLine.push_back(L'\0');

    std::wstring wcwd = utf8ToWide(cwd);
    const wchar_t* cwdPtr = cwd.empty() ? nullptr : wcwd.c_str();

    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, cwdPtr, &si, &pi);
    CloseHandle(hWrite);
    hWrite = nullptr;
    if (hNul != INVALID_HANDLE_VALUE)
        CloseHandle(hNul);

    if (!created) {
        CloseHandle(hRead);
        result.exitCode = -1;
        return result;
    }
    CloseHandle(pi.hThread);

    std::mutex mu;
    std::string collected;
    std::thread reader([&] {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
            std::lock_guard<std::mutex> lock(mu);
            collected.append(buf, n);
        }
    });

    DWORD waitMs = static_cast<DWORD>(timeoutSeconds) * 1000u;
    DWORD w = WaitForSingleObject(pi.hProcess, waitMs);
    if (w == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timedOut = true;
        result.exitCode = -1;
    } else {
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        result.exitCode = static_cast<int>(code);
    }

    CloseHandle(pi.hProcess);
    reader.join();
    CloseHandle(hRead);

    {
        std::lock_guard<std::mutex> lock(mu);
        result.capturedOutput = std::move(collected);
    }
    return result;
}

} // namespace avacli

#else

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

namespace avacli {

ProcessRunResult runShellCommand(const std::string& cwd, const std::string& userCommand, int timeoutSeconds) {
    ProcessRunResult result;
    if (timeoutSeconds < 1)
        timeoutSeconds = 1;

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.exitCode = -1;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.exitCode = -1;
        return result;
    }

    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(127);
        if (dup2(pipefd[1], STDERR_FILENO) < 0)
            _exit(127);
        close(pipefd[1]);

        int nullIn = open("/dev/null", O_RDONLY);
        if (nullIn >= 0) {
            dup2(nullIn, STDIN_FILENO);
            close(nullIn);
        }

        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0)
                _exit(127);
        }

        execl("/bin/sh", "sh", "-c", userCommand.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipefd[1]);
    const int readFd = pipefd[0];

    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::seconds(timeoutSeconds);

    std::string& out = result.capturedOutput;
    char buf[4096];

    while (true) {
        const auto now = clock::now();
        if (now >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            while (true) {
                ssize_t n = read(readFd, buf, sizeof(buf));
                if (n <= 0)
                    break;
                out.append(buf, static_cast<size_t>(n));
            }
            close(readFd);
            result.timedOut = true;
            result.exitCode = -1;
            return result;
        }

        int ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        if (ms > 500)
            ms = 500;
        if (ms < 1)
            ms = 1;

        struct pollfd pfd {};
        pfd.fd = readFd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, ms);
        if (pr < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (pr > 0) {
            if (pfd.revents & POLLIN) {
                ssize_t n = read(readFd, buf, sizeof(buf));
                if (n > 0)
                    out.append(buf, static_cast<size_t>(n));
            }
        }

        int st = 0;
        pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) {
            while (true) {
                ssize_t n = read(readFd, buf, sizeof(buf));
                if (n <= 0)
                    break;
                out.append(buf, static_cast<size_t>(n));
            }
            close(readFd);
            if (WIFEXITED(st))
                result.exitCode = WEXITSTATUS(st);
            else
                result.exitCode = -1;
            return result;
        }
    }

    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    close(readFd);
    result.exitCode = -1;
    return result;
}

} // namespace avacli

#endif
