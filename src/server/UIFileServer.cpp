#include "server/UIFileServer.hpp"
#include "server/EmbeddedAssets.hpp"
#include "platform/Paths.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unordered_map>

namespace avacli {

namespace fs = std::filesystem;

std::string UIFileServer::defaultUiDir() {
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "ui").string();
}

std::string UIFileServer::resolveUiDir(const std::string& overridePath) {
    if (!overridePath.empty()) {
        try {
            return fs::canonical(overridePath).string();
        } catch (...) {
            return fs::absolute(overridePath).string();
        }
    }
    return defaultUiDir();
}

bool UIFileServer::isValidUiDir(const std::string& dir) {
    return fs::exists(fs::path(dir) / "index.html");
}

std::optional<std::pair<std::string, std::string>>
UIFileServer::serveFile(const std::string& uiDir, const std::string& requestPath) {
    if (uiDir.empty() || !fs::is_directory(uiDir))
        return std::nullopt;

    // Sanitize: strip leading /ui/ prefix to get relative path
    std::string rel = requestPath;
    if (rel.rfind("/ui/", 0) == 0)
        rel = rel.substr(4);
    else if (rel.rfind("/", 0) == 0)
        rel = rel.substr(1);

    // Block path traversal
    if (rel.find("..") != std::string::npos)
        return std::nullopt;

    fs::path filePath = fs::path(uiDir) / rel;

    if (!fs::exists(filePath) || !fs::is_regular_file(filePath))
        return std::nullopt;

    // Verify the resolved path is still under uiDir
    try {
        auto canonical = fs::canonical(filePath).string();
        auto canonicalDir = fs::canonical(uiDir).string();
        if (canonical.rfind(canonicalDir, 0) != 0)
            return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs)
        return std::nullopt;

    std::ostringstream ss;
    ss << ifs.rdbuf();

    std::string ext = filePath.extension().string();
    return std::make_pair(ss.str(), std::string(mimeForExtension(ext)));
}

std::optional<std::string>
UIFileServer::getIndexFromDisk(const std::string& uiDir, const std::string& theme) {
    fs::path indexPath = fs::path(uiDir) / "index.html";
    if (!fs::exists(indexPath))
        return std::nullopt;

    std::ifstream ifs(indexPath);
    if (!ifs)
        return std::nullopt;

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string html = ss.str();

    // If a theme is specified, update the theme stylesheet link
    if (!theme.empty() && theme != "default") {
        std::string themeHref = "/ui/themes/" + theme + ".css";
        // Replace the theme-css link href
        std::regex themeRe(R"(href="/ui/themes/[^"]*\.css")");
        html = std::regex_replace(html, themeRe, "href=\"" + themeHref + "\"");
    }

    return html;
}

bool UIFileServer::extractEmbeddedUI(const std::string& targetDir) {
    try {
        fs::create_directories(targetDir);
        fs::create_directories(fs::path(targetDir) / "css");
        fs::create_directories(fs::path(targetDir) / "js");
        fs::create_directories(fs::path(targetDir) / "themes");

        std::string fullHtml = getIndexHtml();

        // Extract CSS variables (:root{...})
        std::string cssVars;
        auto rootStart = fullHtml.find(":root{");
        if (rootStart != std::string::npos) {
            auto rootEnd = fullHtml.find('}', rootStart);
            if (rootEnd != std::string::npos)
                cssVars = fullHtml.substr(rootStart, rootEnd - rootStart + 1);
        }

        // Extract full CSS (between <style> and </style>)
        std::string fullCss;
        auto styleStart = fullHtml.find("<style>");
        auto styleEnd = fullHtml.find("</style>");
        if (styleStart != std::string::npos && styleEnd != std::string::npos) {
            styleStart += 7; // skip <style>
            fullCss = fullHtml.substr(styleStart, styleEnd - styleStart);
        }

        // The rest of CSS after :root block
        std::string restCss = fullCss;
        if (!cssVars.empty()) {
            auto pos = restCss.find(cssVars);
            if (pos != std::string::npos) {
                restCss.erase(pos, cssVars.size());
                // trim leading whitespace
                auto firstChar = restCss.find_first_not_of(" \t\n\r");
                if (firstChar != std::string::npos)
                    restCss = restCss.substr(firstChar);
            }
        }

        // Extract JS (between the app <script> tags in <body>)
        std::string appJs;
        auto scriptStart = fullHtml.find("<script>\n(function(){");
        if (scriptStart == std::string::npos)
            scriptStart = fullHtml.find("<script>(function(){");
        if (scriptStart != std::string::npos) {
            scriptStart += 8; // skip <script> or <script>\n
            if (fullHtml[scriptStart] == '\n') scriptStart++;
            auto scriptEnd = fullHtml.rfind("</script>");
            if (scriptEnd != std::string::npos)
                appJs = fullHtml.substr(scriptStart, scriptEnd - scriptStart);
        }

        // Extract body HTML (between <body> and the app <script>)
        std::string bodyHtml;
        auto bodyStart = fullHtml.find("<body>");
        if (bodyStart != std::string::npos) {
            bodyStart += 6;
            auto bodyScriptStart = fullHtml.find("<script>\n(function(){");
            if (bodyScriptStart == std::string::npos)
                bodyScriptStart = fullHtml.find("<script>(function(){");
            if (bodyScriptStart != std::string::npos)
                bodyHtml = fullHtml.substr(bodyStart, bodyScriptStart - bodyStart);
        }

        // Trim
        auto trim = [](std::string& s) {
            auto a = s.find_first_not_of(" \t\n\r");
            auto b = s.find_last_not_of(" \t\n\r");
            if (a != std::string::npos && b != std::string::npos)
                s = s.substr(a, b - a + 1);
        };
        trim(bodyHtml);

        // Write variables.css
        {
            std::ofstream f(fs::path(targetDir) / "css" / "variables.css");
            f << "/* Avacli Theme Variables */\n" << cssVars << "\n";
        }

        // Write style.css
        {
            std::ofstream f(fs::path(targetDir) / "css" / "style.css");
            f << "/* Avacli UI Styles */\n" << restCss << "\n";
        }

        // Write app.js
        {
            std::ofstream f(fs::path(targetDir) / "js" / "app.js");
            f << appJs << "\n";
        }

        // Write default theme
        {
            std::ofstream f(fs::path(targetDir) / "themes" / "default.css");
            f << "/* Default theme — no overrides */\n";
        }

        // Find CDN scripts
        std::string cdnScripts;
        std::regex cdnRe(R"re(<script\s+src="(https?://[^"]+)"></script>)re");
        std::sregex_iterator it(fullHtml.begin(), fullHtml.end(), cdnRe);
        std::sregex_iterator end;
        for (; it != end; ++it)
            cdnScripts += it->str() + "\n";

        // Write index.html
        {
            std::ofstream f(fs::path(targetDir) / "index.html");
            f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
              << "<meta charset=\"UTF-8\">\n"
              << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,"
                 "maximum-scale=1,interactive-widget=resizes-content,viewport-fit=cover\">\n"
              << "<title>Avacli</title>\n"
              << "<link rel=\"stylesheet\" href=\"/ui/css/variables.css\">\n"
              << "<link rel=\"stylesheet\" href=\"/ui/css/style.css\">\n"
              << "<link id=\"theme-css\" rel=\"stylesheet\" href=\"/ui/themes/default.css\">\n"
              << cdnScripts
              << "</head>\n<body>\n"
              << bodyHtml << "\n"
              << "<script src=\"/ui/js/app.js\"></script>\n"
              << "</body>\n</html>\n";
        }

        spdlog::info("Extracted embedded UI to {}", targetDir);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to extract UI: {}", e.what());
        return false;
    }
}

const char* UIFileServer::mimeForExtension(const std::string& ext) {
    static const std::unordered_map<std::string, const char*> mimeMap = {
        {".html", "text/html"},
        {".htm",  "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".mjs",  "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".webp", "image/webp"},
        {".woff", "font/woff"},
        {".woff2","font/woff2"},
        {".ttf",  "font/ttf"},
        {".otf",  "font/otf"},
        {".map",  "application/json"},
        {".txt",  "text/plain"},
        {".xml",  "application/xml"},
        {".webm", "video/webm"},
        {".mp4",  "video/mp4"},
        {".mp3",  "audio/mpeg"},
        {".wav",  "audio/wav"},
        {".ogg",  "audio/ogg"},
    };

    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = mimeMap.find(lower);
    if (it != mimeMap.end())
        return it->second;
    return "application/octet-stream";
}

} // namespace avacli
