#pragma once

#include <string>

namespace avacli {

class ApiKeyStorage {
public:
    static std::string configPath();
    static std::string load();
    static bool save(const std::string& apiKey);
};

} // namespace avacli
