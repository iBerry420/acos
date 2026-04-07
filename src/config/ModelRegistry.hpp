#pragma once

#include "core/Types.hpp"
#include <string>
#include <vector>

namespace avacli {

class ModelRegistry {
public:
    static ModelConfig get(const std::string& id);
    static std::vector<std::string> listIds();
    static const std::vector<ModelConfig>& listAll();

    /// Fetch models from xAI API and cache locally. Call once at startup.
    static void fetchFromApi(const std::string& apiKey);

    /// Clear cached models and re-fetch from the API.
    static void refreshModels(const std::string& apiKey);

    /// Output all models as JSON to stdout (for --list-models-json).
    static std::string listModelsJson();
};

} // namespace avacli
