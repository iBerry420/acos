#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "client/XAIClient.hpp"
#include "tools/ToolRegistry.hpp"
#include "platform/Paths.hpp"
#include "platform/ProcessRunner.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

void registerToolRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/tools", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        ToolRegistry::registerAll();
        auto allTools = ToolRegistry::getAllTools();

        std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();

        nlohmann::json result = nlohmann::json::array();
        for (const auto& t : allTools) {
            nlohmann::json entry;
            std::string name = t["function"]["name"];
            entry["name"] = name;
            entry["description"] = t["function"].value("description", "");
            entry["parameters"] = t["function"].value("parameters", nlohmann::json::object());
            entry["is_custom"] = ToolRegistry::isCustomTool(name);
            entry["enabled"] = true;

            if (!ToolRegistry::isCustomTool(name)) {
                entry["category"] = "builtin";
            } else {
                std::string metaPath = (fs::path(toolsDir) / (name + ".json")).string();
                std::string createdBy = "user";
                if (fs::exists(metaPath)) {
                    try {
                        std::ifstream ifs(metaPath);
                        auto meta = nlohmann::json::parse(ifs);
                        createdBy = meta.value("created_by", "user");
                    } catch (...) {}
                }
                entry["category"] = (createdBy == "ai") ? "ai_created" : "user_created";
            }
            result.push_back(entry);
        }
        res.set_content(result.dump(), "application/json");
    });

    svr.Post("/api/tools/generate", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string description = body.value("description", "");
        std::string toolName = body.value("name", "");
        std::string defaultExtraModel = ctx.config->extraModel.empty() ? "grok-4-1-non-reasoning" : ctx.config->extraModel;
        std::string model = body.value("model", defaultExtraModel);
        if (description.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"description required"})", "application/json");
            return;
        }
        if (toolName.empty()) {
            std::string lower;
            for (unsigned char c : description.substr(0, 50))
                lower += (c == ' ' ? '_' : static_cast<char>(std::tolower(c)));
            lower.erase(std::remove_if(lower.begin(), lower.end(),
                [](char c) { return !std::isalnum(c) && c != '_'; }), lower.end());
            toolName = "custom_" + lower;
        }
        if (toolName.find("custom_") != 0) toolName = "custom_" + toolName;

        std::string apiKey;
        std::string chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"no API key configured"})", "application/json");
            return;
        }

        std::string prompt = R"(Generate a Python tool script. Requirements:
1. The script reads a JSON input file path from sys.argv[1], loads it, and processes the data.
2. It must output ONLY valid JSON to stdout (no other text, logs, etc).
3. The output JSON must have a "result" key with the tool's output.
4. Include proper error handling — on error output {"error": "description"}.
5. The script should be self-contained (can import standard library + common pip packages).

Tool name: )" + toolName + R"(
Tool description: )" + description + R"(

Also generate a JSON object with:
- "parameters": an OpenAI function-calling style JSON Schema for the input parameters
- "description": a one-line description for the tool
- "script": the full Python script content

Respond with ONLY a JSON object containing "parameters", "description", and "script" keys. No markdown fences.)";

        XAIClient client(apiKey, chatUrl);
        std::vector<Message> msgs;
        msgs.push_back({"user", prompt});
        std::string fullResponse;
        ChatOptions opts;
        opts.model = model;
        opts.stream = true;
        XAIClient::StreamCallbacks cb;
        cb.onContent = [&fullResponse](const std::string& delta) { fullResponse += delta; };
        bool ok = client.chatStream(msgs, opts, cb);

        if (!ok || fullResponse.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"LLM generation failed"})", "application/json");
            return;
        }

        std::string jsonStr = fullResponse;
        auto startPos = jsonStr.find('{');
        auto endPos = jsonStr.rfind('}');
        if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos)
            jsonStr = jsonStr.substr(startPos, endPos - startPos + 1);

        nlohmann::json generated;
        try { generated = nlohmann::json::parse(jsonStr); } catch (...) {
            nlohmann::json ej;
            ej["error"] = "Failed to parse LLM response as JSON";
            ej["raw"] = fullResponse;
            res.status = 500;
            res.set_content(ej.dump(), "application/json");
            return;
        }

        std::string script = generated.value("script", "");
        std::string desc = generated.value("description", description);
        nlohmann::json params = generated.value("parameters", nlohmann::json::object());

        if (script.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"LLM did not generate a script"})", "application/json");
            return;
        }

        std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
        fs::create_directories(toolsDir);

        std::string scriptPath = (fs::path(toolsDir) / (toolName + ".py")).string();
        { std::ofstream ofs(scriptPath); ofs << script; }

        nlohmann::json meta;
        meta["name"] = toolName;
        meta["description"] = desc;
        meta["parameters"] = params;
        meta["script_path"] = scriptPath;
        meta["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::string metaPath = (fs::path(toolsDir) / (toolName + ".json")).string();
        { std::ofstream ofs(metaPath); ofs << meta.dump(2); }

        nlohmann::json toolDef;
        toolDef["type"] = "function";
        toolDef["function"]["name"] = toolName;
        toolDef["function"]["description"] = desc;
        toolDef["function"]["parameters"] = params;
        ToolRegistry::registerCustomTool(toolDef);

        nlohmann::json response;
        response["name"] = toolName;
        response["description"] = desc;
        response["parameters"] = params;
        response["script"] = script;
        response["script_path"] = scriptPath;
        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/tools/test", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string toolName = body.value("name", "");
        nlohmann::json input = body.value("input", nlohmann::json::object());

        std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
        std::string scriptPath = (fs::path(toolsDir) / (toolName + ".py")).string();

        if (!fs::exists(scriptPath)) {
            res.status = 404;
            res.set_content(R"({"error":"tool script not found"})", "application/json");
            return;
        }

        std::string inputPath = (fs::path(toolsDir) / (toolName + "_test_input.json")).string();
        { std::ofstream ofs(inputPath); ofs << input.dump(); }

        std::string cmd = getPyCmd() + " \"" + scriptPath + "\" \"" + inputPath + "\"";
        auto pr = runShellCommand(toolsDir, cmd, 60);
        try { fs::remove(inputPath); } catch (...) {}

        nlohmann::json result;
        result["exit_code"] = pr.exitCode;
        result["timed_out"] = pr.timedOut;
        if (pr.exitCode == 0) {
            try { result["output"] = nlohmann::json::parse(pr.capturedOutput); }
            catch (...) { result["output"] = pr.capturedOutput; }
        } else {
            result["error"] = pr.capturedOutput;
        }
        res.set_content(result.dump(), "application/json");
    });

    svr.Post("/api/tools/mock", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string toolName = body.value("name", "");
        std::string desc = body.value("description", "");
        nlohmann::json params = body.value("parameters", nlohmann::json::object());
        std::string defaultExtraModel2 = ctx.config->extraModel.empty() ? "grok-4-1-non-reasoning" : ctx.config->extraModel;
        std::string model = body.value("model", defaultExtraModel2);

        std::string apiKey;
        std::string chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"no API key"})", "application/json");
            return;
        }

        std::string prompt = "Generate realistic mock/test input data for this tool.\n"
            "Tool name: " + toolName + "\nDescription: " + desc +
            "\nParameter schema: " + params.dump() +
            "\n\nRespond with ONLY a JSON object matching the parameter schema. No markdown fences.";

        XAIClient client(apiKey, chatUrl);
        std::vector<Message> msgs;
        msgs.push_back({"user", prompt});
        std::string fullResp;
        ChatOptions mockOpts;
        mockOpts.model = model;
        mockOpts.stream = true;
        XAIClient::StreamCallbacks mockCb;
        mockCb.onContent = [&fullResp](const std::string& d) { fullResp += d; };
        client.chatStream(msgs, mockOpts, mockCb);

        std::string js = fullResp;
        auto s = js.find('{');
        auto e = js.rfind('}');
        if (s != std::string::npos && e != std::string::npos && e > s) js = js.substr(s, e - s + 1);

        try {
            auto parsed = nlohmann::json::parse(js);
            res.set_content(parsed.dump(), "application/json");
        } catch (...) {
            res.set_content(R"({"error":"failed to parse mock data","raw":")" + fullResp + R"("})", "application/json");
        }
    });

    svr.Delete(R"(/api/tools/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string toolName = req.matches[1];

        if (!ToolRegistry::isCustomTool(toolName)) {
            res.status = 400;
            res.set_content(R"({"error":"cannot delete built-in tool"})", "application/json");
            return;
        }

        ToolRegistry::removeCustomTool(toolName);

        std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
        try { fs::remove(fs::path(toolsDir) / (toolName + ".py")); } catch (...) {}
        try { fs::remove(fs::path(toolsDir) / (toolName + ".json")); } catch (...) {}

        res.set_content(R"({"ok":true})", "application/json");
    });
}

} // namespace avacli
