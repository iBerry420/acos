#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace avacli {

class VultrClient {
public:
    explicit VultrClient(const std::string& apiKey);

    struct HttpResult {
        long status = 0;
        std::string body;
        bool ok() const { return status >= 200 && status < 300; }
    };

    nlohmann::json listPlans();
    nlohmann::json listRegions();
    nlohmann::json listInstances();
    nlohmann::json getInstance(const std::string& instanceId);
    nlohmann::json createInstance(const std::string& region, const std::string& plan,
                                  const std::string& osId, const std::string& label = "",
                                  const std::string& startupScriptId = "",
                                  const std::string& userDataPlain = "");
    bool destroyInstance(const std::string& instanceId);
    nlohmann::json listOSImages();
    nlohmann::json getAccount();
    nlohmann::json getAccountBandwidth();
    nlohmann::json listPendingCharges();

private:
    HttpResult get(const std::string& path);
    HttpResult post(const std::string& path, const nlohmann::json& body);
    HttpResult del(const std::string& path);

    std::string apiKey_;
    static constexpr const char* BASE_URL = "https://api.vultr.com/v2";
};

} // namespace avacli
