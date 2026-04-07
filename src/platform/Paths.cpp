#include "platform/Paths.hpp"
#include <cstdlib>
#include <filesystem>

namespace avacli {

namespace fs = std::filesystem;

std::string userHomeDirectory() {
#if defined(_WIN32)
    const char* profile = std::getenv("USERPROFILE");
    if (profile && profile[0] != '\0')
        return std::string(profile);
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path = std::getenv("HOMEPATH");
    if (drive && path)
        return std::string(drive) + path;
    return {};
#else
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : std::string{};
#endif
}

std::string userHomeDirectoryOrFallback() {
    std::string h = userHomeDirectory();
    if (!h.empty())
        return h;
#if defined(_WIN32)
    const char* t = std::getenv("TEMP");
    if (t && t[0] != '\0')
        return std::string(t);
    t = std::getenv("TMP");
    if (t && t[0] != '\0')
        return std::string(t);
    return ".";
#else
    const char* td = std::getenv("TMPDIR");
    if (td && td[0] != '\0') return std::string(td);
    return "/tmp";
#endif
}

bool pathIsAbsolute(const std::string& path) {
    if (path.empty())
        return false;
#if defined(_WIN32)
    if (path.size() >= 2) {
        unsigned char c0 = static_cast<unsigned char>(path[0]);
        unsigned char c1 = static_cast<unsigned char>(path[1]);
        if (((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')) && c1 == ':')
            return true;
    }
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
        return true;
    return false;
#else
    return path[0] == '/';
#endif
}

std::string resolveWorkspacePath(const std::string& workspace, const std::string& path) {
    if (path.empty())
        return workspace;
    if (pathIsAbsolute(path))
        return fs::path(path).lexically_normal().string();
    return (fs::path(workspace) / path).lexically_normal().string();
}

} // namespace avacli
