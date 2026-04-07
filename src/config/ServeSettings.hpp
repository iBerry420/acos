#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace avacli {

/// Path to ~/.avacli/settings.json (serve-mode UI persistence).
std::string serveSettingsFilePath();

/// Parse settings.json or return empty object.
nlohmann::json loadServeSettingsJson();

} // namespace avacli
