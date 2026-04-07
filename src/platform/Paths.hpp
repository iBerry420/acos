#pragma once

#include <string>

namespace avacli {

/// User profile directory (HOME / USERPROFILE / HOMEDRIVE+HOMEPATH). May be empty.
std::string userHomeDirectory();

/// Like userHomeDirectory(), but never empty (TEMP, /tmp, or ".").
std::string userHomeDirectoryOrFallback();

bool pathIsAbsolute(const std::string& path);

/// If path is empty, returns workspace. Absolute path unchanged; else workspace-relative.
std::string resolveWorkspacePath(const std::string& workspace, const std::string& path);

} // namespace avacli
