#pragma once
#include "server/HttpServer.hpp"
#include "auth/MasterKeyManager.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace avacli {

std::string findPythonCmd();
std::string getPyCmd();
int findAvailablePort(int startPort, int maxAttempts = 20);
std::string usageFilePath();
void appendUsageRecord(const nlohmann::json& record);
nlohmann::json loadUsageHistory(int days, int hours = 0);
std::vector<std::string> autoDetectCapabilities(const std::string& workspace);
std::string generateChatSessionName();
void mergeServeDiskIntoConfig(ServeConfig& sc);
void resolveXaiAuth(const ServeConfig& sc, std::string& apiKey, std::string& chatUrl);
std::string getVultrApiKey();

/// Extract Bearer token from request, return the username associated with the session
std::string getRequestUser(const httplib::Request& req, MasterKeyManager* mgr);

/// Check if the requesting user has admin privileges
bool isAdminRequest(const httplib::Request& req, MasterKeyManager* mgr);

} // namespace avacli
