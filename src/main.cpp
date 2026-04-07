#include "auth/MasterKeyManager.hpp"
#include "config/ApiKeyStorage.hpp"
#include "config/Config.hpp"
#include "config/ModelRegistry.hpp"
#include "core/Application.hpp"
#include "core/Types.hpp"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <iomanip>

using namespace avacli;

static void printModels(bool json) {
    std::string apiKey;
    const char* env = std::getenv("XAI_API_KEY");
    if (env && env[0] != '\0') apiKey = env;
    else apiKey = ApiKeyStorage::load();
    ModelRegistry::fetchFromApi(apiKey);

    if (json) {
        std::cout << ModelRegistry::listModelsJson() << "\n";
        return;
    }

    std::cout << "Available models:\n\n";
    std::cout << std::left << std::setw(50) << "MODEL ID"
              << std::setw(10) << "CONTEXT"
              << std::setw(12) << "MAX OUTPUT"
              << "REASONING" << "\n";
    std::cout << std::string(82, '-') << "\n";
    for (const auto& m : ModelRegistry::listAll()) {
        std::cout << std::left << std::setw(50) << m.id
                  << std::setw(10) << (std::to_string(m.contextWindow / 1024) + "k")
                  << std::setw(12) << (std::to_string(m.defaultMaxTokens / 1024) + "k")
                  << (m.reasoning ? "yes" : "no") << "\n";
    }
    std::cout << "\nDefault: grok-4-1-reasoning\n";
}

int main(int argc, char** argv) {
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("avacli", stderr_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    MasterKeyManager::migrateFromLegacy();

    avacli::Config config;
    std::string modeStr = "question";
    std::string setApiKey;
    bool listModels = false;
    bool listModelsJson = false;

    CLI::App app{"avacli - Autonomous AI agent for xAI Grok models"};
    app.require_subcommand(0, 1);

    app.add_option("--model", config.model, "Model ID")
        ->default_val("grok-4-1-reasoning");
    app.add_option("--mode", modeStr, "Mode: question, plan, or agent")
        ->default_val("question");
    app.add_option("--session,-s", config.session, "Session name for save/resume");
    app.add_option("--workspace,--dir,-d", config.workspace, "Working directory");
    app.add_option("--task,-t", config.task, "Task (for non-interactive mode)");
    app.add_flag("--non-interactive", config.nonInteractive, "Run without interactive UI");
    app.add_option("--format", config.outputFormat, "Output format (text, json, or stream-json)");
    app.add_option("--set-api-key", setApiKey, "Save API key to ~/.avacli/config.json and exit");
    app.add_flag("--list-models", listModels, "List available models and exit");
    app.add_flag("--list-models-json", listModelsJson, "Output available models as JSON and exit");

    std::string setMasterKey;
    bool generateMasterKey = false;
    app.add_option("--set-master-key", setMasterKey, "Set password for the default admin account");
    app.add_flag("--generate-master-key", generateMasterKey, "Generate a random password for the default admin account");

    auto* serve_cmd = app.add_subcommand("serve", "Start embedded HTTP server with web UI");
    serve_cmd->add_option("--port,-p", config.servePort, "Server port")
        ->default_val(8080);
    serve_cmd->add_option("--host", config.serveHost, "Bind host")
        ->default_val("0.0.0.0");

    std::string chatMessage;
    auto* chat_cmd = app.add_subcommand("chat", "Send a one-shot chat message");
    chat_cmd->add_option("message", chatMessage, "Message to send")->required();

    auto* login_cmd = app.add_subcommand("login", "Authenticate with master key");
    auto* logout_cmd = app.add_subcommand("logout", "Clear authentication session");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (listModels || listModelsJson) {
        printModels(listModelsJson);
        return 0;
    }

    if (!setApiKey.empty()) {
        if (ApiKeyStorage::save(setApiKey)) {
            std::cout << "API key saved. You can now run avacli without setting XAI_API_KEY.\n";
            return 0;
        }
        std::cerr << "Failed to save API key.\n";
        return 1;
    }

    if (generateMasterKey) {
        std::string key = MasterKeyManager::generateMasterKey();
        if (key.empty()) {
            std::cerr << "Failed to generate admin password.\n";
            return 1;
        }
        std::cout << "Admin account created (username: admin). Save this password:\n\n"
                  << "  " << key << "\n\n"
                  << "Use these credentials to log in to the web UI.\n";
        return 0;
    }

    if (!setMasterKey.empty()) {
        if (MasterKeyManager::setMasterKey(setMasterKey)) {
            std::cout << "Admin account configured (username: admin). Use these credentials to log in.\n";
            return 0;
        }
        std::cerr << "Failed to set admin password (minimum 8 characters).\n";
        return 1;
    }

    // Handle login subcommand
    if (login_cmd->parsed()) {
        if (!MasterKeyManager::isConfigured()) {
            std::cerr << "No accounts configured. Run: avacli --set-master-key <password> or avacli --generate-master-key\n";
            return 1;
        }
        std::cout << "Enter password: ";
        std::string key;
        std::getline(std::cin, key);
        if (MasterKeyManager::verify(key)) {
            std::cout << "Login successful.\n";
            return 0;
        }
        std::cerr << "Invalid credentials.\n";
        return 1;
    }

    // Handle logout subcommand
    if (logout_cmd->parsed()) {
        std::cout << "Logged out.\n";
        return 0;
    }

    if (chat_cmd->parsed()) {
        config.nonInteractive = true;
        config.task = chatMessage;
    }

    bool hasExplicitAction = chat_cmd->parsed()
                          || login_cmd->parsed()
                          || logout_cmd->parsed()
                          || !config.task.empty()
                          || listModels || listModelsJson
                          || !setApiKey.empty()
                          || !setMasterKey.empty()
                          || generateMasterKey;

    if (serve_cmd->parsed() || !hasExplicitAction) {
        config.serveMode = true;
    }

    config.mode = modeFromString(modeStr);
    if (config.workspace.empty())
        config.workspace = ".";

    if (config.nonInteractive)
        spdlog::set_level(spdlog::level::warn);

    {
        std::string apiKey = config.xaiApiKey();
        ModelRegistry::fetchFromApi(apiKey);
    }

    Application application(config);
    return application.run();
}
