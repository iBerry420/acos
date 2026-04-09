#pragma once

#include <optional>
#include <string>
#include <utility>

namespace avacli {

class UIFileServer {
public:
    /// Default UI directory: ~/.avacli/ui/
    static std::string defaultUiDir();

    /// Resolve the active UI directory (custom override > default)
    static std::string resolveUiDir(const std::string& overridePath);

    /// Check if a directory has a valid UI (contains index.html)
    static bool isValidUiDir(const std::string& dir);

    /// Serve a file from the UI directory.
    /// Returns {content, mimeType} or nullopt if file not found.
    static std::optional<std::pair<std::string, std::string>>
    serveFile(const std::string& uiDir, const std::string& requestPath);

    /// Get index.html from disk UI dir, with optional theme injection.
    /// Returns nullopt if the file doesn't exist.
    static std::optional<std::string>
    getIndexFromDisk(const std::string& uiDir, const std::string& theme = "");

    /// Extract the compiled-in embedded UI to a target directory.
    static bool extractEmbeddedUI(const std::string& targetDir);

    /// MIME type from file extension
    static const char* mimeForExtension(const std::string& ext);
};

} // namespace avacli
