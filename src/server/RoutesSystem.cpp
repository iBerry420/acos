#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/DefaultSystemPrompt.hpp"
#include "client/XAIClient.hpp"
#include "config/ModelRegistry.hpp"
#include "tools/ToolRegistry.hpp"
#include "platform/Paths.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace avacli {

namespace fs = std::filesystem;

static constexpr const char* kDefaultBlueprintId = "default";

static std::string blueprintsDir() {
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "blueprints").string();
}

static std::string activePromptPath(const std::string& workspace) {
    return (fs::path(workspace) / "GROK_SYSTEM_PROMPT.md").string();
}

static int64_t nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Write (or overwrite) the canonical default blueprint on disk. Always
/// keyed as id="default" so the UI can render a "Default" badge and so
/// POST /api/system/blueprints/reset always targets the same file.
static bool writeDefaultBlueprint(std::string* errOut = nullptr) {
    try {
        auto dir = blueprintsDir();
        fs::create_directories(dir);

        auto bp = nlohmann::json::parse(getDefaultBlueprintJson());
        bp["is_default"] = true;
        bp["updated_at"] = nowEpoch();
        if (!bp.contains("created_at")) bp["created_at"] = nowEpoch();

        auto path = (fs::path(dir) / (std::string(kDefaultBlueprintId) + ".json")).string();
        std::ofstream ofs(path);
        if (!ofs) {
            if (errOut) *errOut = "cannot write " + path;
            return false;
        }
        ofs << bp.dump(2);
        return true;
    } catch (const std::exception& e) {
        if (errOut) *errOut = e.what();
        return false;
    }
}

/// Run once per process: seed `~/.avacli/blueprints/default.json` if missing
/// so fresh installs have something selectable on the System page. Idempotent;
/// cheap on subsequent calls (fs::exists check only).
static void seedDefaultsOnceIfMissing() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        try {
            auto path = (fs::path(blueprintsDir()) /
                         (std::string(kDefaultBlueprintId) + ".json")).string();
            if (!fs::exists(path)) {
                std::string err;
                if (writeDefaultBlueprint(&err)) {
                    spdlog::info("Seeded default blueprint at {}", path);
                } else {
                    spdlog::warn("Failed to seed default blueprint: {}", err);
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("seedDefaultsOnceIfMissing: {}", e.what());
        }
    });
}

void registerSystemRoutes(httplib::Server& svr, ServerContext ctx) {

    // Seed ~/.avacli/blueprints/default.json at server startup so fresh
    // installs have the Default Coding Agent already in the list.
    // Idempotent; only writes when the file doesn't already exist.
    seedDefaultsOnceIfMissing();

    // Get active system prompt. If the workspace file doesn't exist
    // (e.g. fresh install, user hasn't saved yet), return the embedded
    // default so the UI has something to show instead of an empty textarea.
    // `is_default=true` lets the UI indicate "you're viewing the shipped default".
    svr.Get("/api/system/prompt", [ctx](const httplib::Request&, httplib::Response& res) {
        auto path = activePromptPath(ctx.config->workspace);
        std::string content;
        bool isDefault = false;
        if (fs::exists(path)) {
            std::ifstream ifs(path);
            if (ifs) content.assign(std::istreambuf_iterator<char>(ifs),
                                     std::istreambuf_iterator<char>());
        }
        if (content.empty()) {
            content = getDefaultSystemPrompt();
            isDefault = true;
        }
        nlohmann::json j;
        j["prompt"] = content;
        j["path"] = path;
        j["is_default"] = isDefault;
        res.set_content(j.dump(), "application/json");
    });

    // Preview the embedded default prompt without touching disk.
    svr.Get("/api/system/prompt/default", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["prompt"] = getDefaultSystemPrompt();
        j["is_default"] = true;
        res.set_content(j.dump(), "application/json");
    });

    // Reset the workspace system prompt to the embedded default.
    // Overwrites GROK_SYSTEM_PROMPT.md in the workspace with the baked-in
    // copy from GROK_SYSTEM_PROMPT.md at build time.
    svr.Post("/api/system/prompt/reset", [ctx](const httplib::Request&, httplib::Response& res) {
        auto path = activePromptPath(ctx.config->workspace);
        auto content = getDefaultSystemPrompt();
        std::ofstream ofs(path);
        if (!ofs) {
            res.status = 500;
            res.set_content(R"({"error":"cannot write prompt file"})", "application/json");
            return;
        }
        ofs << content;
        ofs.close();
        nlohmann::json j;
        j["reset"] = true;
        j["prompt"] = content;
        j["path"] = path;
        j["is_default"] = true;
        res.set_content(j.dump(), "application/json");
    });

    // Update active system prompt
    svr.Post("/api/system/prompt", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        auto path = activePromptPath(ctx.config->workspace);
        std::string content = body.value("prompt", "");
        std::ofstream ofs(path);
        if (!ofs) {
            res.status = 500;
            res.set_content(R"({"error":"cannot write prompt file"})", "application/json");
            return;
        }
        ofs << content;
        ofs.close();
        res.set_content(R"({"saved":true})", "application/json");
    });

    // Restore the canonical "Default Coding Agent" blueprint. Overwrites
    // ~/.avacli/blueprints/default.json with the baked-in copy so the user
    // always has a reset target if they've edited or deleted it.
    // Registered before the /api/system/blueprints/:id handlers so the
    // literal "reset" path wins over the regex matcher.
    svr.Post("/api/system/blueprints/reset", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        if (!writeDefaultBlueprint(&err)) {
            res.status = 500;
            nlohmann::json j;
            j["error"] = err.empty() ? "failed to write default blueprint" : err;
            res.set_content(j.dump(), "application/json");
            return;
        }
        auto path = (fs::path(blueprintsDir()) /
                     (std::string(kDefaultBlueprintId) + ".json")).string();
        std::ifstream ifs(path);
        auto bp = nlohmann::json::parse(ifs);
        bp["id"] = kDefaultBlueprintId;
        res.set_content(bp.dump(), "application/json");
    });

    // List blueprints. Auto-seeds the default blueprint if the user has
    // wiped their blueprints dir since the last seed attempt this process;
    // keeps the "no blueprints yet" empty state from ever showing on a
    // fresh install.
    svr.Get("/api/system/blueprints", [](const httplib::Request&, httplib::Response& res) {
        auto dir = blueprintsDir();
        auto defPath = (fs::path(dir) / (std::string(kDefaultBlueprintId) + ".json")).string();
        if (!fs::exists(defPath)) {
            writeDefaultBlueprint();
        }
        nlohmann::json arr = nlohmann::json::array();
        if (fs::exists(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.path().extension() != ".json") continue;
                try {
                    std::ifstream ifs(entry.path());
                    auto bp = nlohmann::json::parse(ifs);
                    bp["id"] = entry.path().stem().string();
                    arr.push_back(bp);
                } catch (...) {}
            }
        }
        std::sort(arr.begin(), arr.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            // Default pinned to top, then newest first.
            bool aDef = a.value("id", "") == kDefaultBlueprintId;
            bool bDef = b.value("id", "") == kDefaultBlueprintId;
            if (aDef != bDef) return aDef;
            return a.value("updated_at", 0) > b.value("updated_at", 0);
        });
        res.set_content(arr.dump(), "application/json");
    });

    // Get single blueprint
    svr.Get(R"(/api/system/blueprints/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        auto path = (fs::path(blueprintsDir()) / (id + ".json")).string();
        if (!fs::exists(path)) {
            res.status = 404;
            res.set_content(R"({"error":"blueprint not found"})", "application/json");
            return;
        }
        std::ifstream ifs(path);
        auto bp = nlohmann::json::parse(ifs);
        bp["id"] = id;
        res.set_content(bp.dump(), "application/json");
    });

    // Create or update blueprint
    svr.Post("/api/system/blueprints", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        auto dir = blueprintsDir();
        fs::create_directories(dir);

        std::string id = body.value("id", "");
        bool isNew = id.empty();
        if (isNew) {
            id = "bp_" + std::to_string(nowEpoch()) + "_" +
                 std::to_string(std::hash<std::string>{}(body.value("name", "")) % 10000);
        }

        nlohmann::json bp;
        bp["name"] = body.value("name", "Untitled");
        bp["description"] = body.value("description", "");
        bp["prompt"] = body.value("prompt", "");
        bp["nodes"] = body.value("nodes", nlohmann::json::array());
        bp["edges"] = body.value("edges", nlohmann::json::array());
        bp["mermaid_src"] = body.value("mermaid_src", "");
        bp["tools"] = body.value("tools", nlohmann::json::array());
        bp["model_assignments"] = body.value("model_assignments", nlohmann::json::object());
        bp["is_default"] = body.value("is_default", false);
        bp["updated_at"] = nowEpoch();
        if (isNew) bp["created_at"] = nowEpoch();
        else {
            auto path = (fs::path(dir) / (id + ".json")).string();
            if (fs::exists(path)) {
                try {
                    std::ifstream ifs(path);
                    auto existing = nlohmann::json::parse(ifs);
                    bp["created_at"] = existing.value("created_at", nowEpoch());
                } catch (...) { bp["created_at"] = nowEpoch(); }
            } else bp["created_at"] = nowEpoch();
        }

        auto outPath = (fs::path(dir) / (id + ".json")).string();
        std::ofstream ofs(outPath);
        ofs << bp.dump(2);
        ofs.close();

        bp["id"] = id;
        res.set_content(bp.dump(), "application/json");
    });

    // Delete blueprint
    svr.Delete(R"(/api/system/blueprints/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        auto path = (fs::path(blueprintsDir()) / (id + ".json")).string();
        if (fs::exists(path)) {
            fs::remove(path);
            res.set_content(R"({"deleted":true})", "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
        }
    });

    // Deploy blueprint as active system prompt
    svr.Post(R"(/api/system/blueprints/([^/]+)/deploy)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        auto bpPath = (fs::path(blueprintsDir()) / (id + ".json")).string();
        if (!fs::exists(bpPath)) {
            res.status = 404;
            res.set_content(R"({"error":"blueprint not found"})", "application/json");
            return;
        }
        std::ifstream ifs(bpPath);
        auto bp = nlohmann::json::parse(ifs);
        ifs.close();

        std::string prompt = bp.value("prompt", "");
        auto promptPath = activePromptPath(ctx.config->workspace);
        std::ofstream ofs(promptPath);
        if (!ofs) {
            res.status = 500;
            res.set_content(R"({"error":"cannot write prompt file"})", "application/json");
            return;
        }
        ofs << prompt;
        ofs.close();

        nlohmann::json j;
        j["deployed"] = true;
        j["blueprint_id"] = id;
        j["prompt_path"] = promptPath;
        res.set_content(j.dump(), "application/json");
    });

    // List available tools for the agent builder
    svr.Get("/api/system/tools", [](const httplib::Request&, httplib::Response& res) {
        ToolRegistry::registerAll();
        auto allTools = ToolRegistry::getToolsForMode(Mode::Agent);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : allTools) {
            if (!t.contains("function")) continue;
            auto& fn = t["function"];
            arr.push_back({
                {"name", fn.value("name", "")},
                {"description", fn.value("description", "")},
                {"category", "built-in"}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // Agent Builder AI - generate a blueprint from natural language
    svr.Post("/api/system/agent-builder", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string description = body.value("description", "");
        if (description.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"description required"})", "application/json");
            return;
        }

        auto selectedTools = body.value("tools", nlohmann::json::array());
        auto selectedModels = body.value("models", nlohmann::json::object());

        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"no API key configured"})", "application/json");
            return;
        }

        ToolRegistry::registerAll();
        auto allTools = ToolRegistry::getToolsForMode(Mode::Agent);
        std::string toolList;
        for (const auto& t : allTools) {
            if (!t.contains("function")) continue;
            auto& fn = t["function"];
            toolList += "- `" + fn.value("name", "") + "` - " + fn.value("description", "") + "\n";
        }

        const auto& models = ModelRegistry::listAll();
        std::string modelList;
        for (const auto& m : models) {
            modelList += "- `" + m.id + "` (context: " + std::to_string(m.contextWindow) +
                         ", type: " + m.type + (m.reasoning ? ", reasoning" : "") + ")\n";
        }

        std::string selectedToolsStr;
        if (!selectedTools.empty()) {
            selectedToolsStr = "\n\nThe user has pre-selected these tools: ";
            for (const auto& t : selectedTools) {
                selectedToolsStr += "`" + t.get<std::string>() + "`, ";
            }
        }

        std::string selectedModelsStr;
        if (!selectedModels.empty()) {
            selectedModelsStr = "\n\nThe user has pre-assigned these models: ";
            for (auto& [step, model] : selectedModels.items()) {
                selectedModelsStr += step + " -> `" + model.get<std::string>() + "`, ";
            }
        }

        std::string metaPrompt = R"(You are an expert agent architect. Given a description of an agent's desired behavior, generate a complete agent blueprint.

## Available Tools
)" + toolList + R"(

## Available Models
)" + modelList + R"(

## Instructions
Generate a JSON object with these fields:
1. `name` - Short name for the blueprint
2. `description` - One-line description
3. `prompt` - A complete system prompt (markdown) that instructs an LLM to behave as described. Be thorough but token-efficient. Include tool usage instructions.
4. `tools` - Array of tool names the agent should use
5. `model_assignments` - Object mapping step names to model IDs. Use reasoning models for complex analysis, fast models for simple processing, image models for image tasks, video models for video tasks. Keys should be descriptive step names.
6. `flow_steps` - Array of objects with {name, description, model, type} where type is one of: process, decision, tool, io
7. `flow_edges` - Array of objects with {from, to, label} connecting the flow steps by name

The system prompt should:
- Define the agent's identity and purpose clearly
- Reference specific tools by name with usage instructions
- Specify which model to use for which processing step (using model routing comments)
- Be optimized for minimal tokens with maximum context
- Include safety constraints and guardrails appropriate to the use case

)" + selectedToolsStr + selectedModelsStr + R"(

IMPORTANT: Return ONLY valid JSON, no markdown fences, no explanation text. Just the raw JSON object.)";

        XAIClient client(apiKey, chatUrl);

        std::string model = ctx.config->model;
        if (model.empty()) model = "grok-3-mini";

        std::vector<Message> messages;
        messages.push_back({"system", metaPrompt});
        messages.push_back({"user", description});

        ChatOptions opts;
        opts.model = model;
        opts.maxTokens = 16384;
        opts.stream = true;

        std::string fullResponse;
        bool ok = client.chatStream(messages, opts, {
            .onContent = [&](const std::string& delta) { fullResponse += delta; },
            .onToolCalls = [](const std::vector<ToolCall>&) {},
            .onUsage = [](const Usage&) {},
            .onDone = [] {}
        });

        if (!ok) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = "LLM generation failed: " + client.lastError();
            res.set_content(err.dump(), "application/json");
            return;
        }

        auto jsonStart = fullResponse.find('{');
        auto jsonEnd = fullResponse.rfind('}');
        if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = "Failed to parse LLM response as JSON";
            err["raw"] = fullResponse.substr(0, 2000);
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string jsonStr = fullResponse.substr(jsonStart, jsonEnd - jsonStart + 1);
        nlohmann::json result;
        try {
            result = nlohmann::json::parse(jsonStr);
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("JSON parse error: ") + e.what();
            err["raw"] = jsonStr.substr(0, 2000);
            res.set_content(err.dump(), "application/json");
            return;
        }

        nlohmann::json nodes = nlohmann::json::array();
        nlohmann::json edges = nlohmann::json::array();
        if (result.contains("flow_steps") && result["flow_steps"].is_array()) {
            for (const auto& step : result["flow_steps"]) {
                std::string name = step.value("name", "step");
                std::string cleanId = name;
                for (char& c : cleanId) {
                    if (!std::isalnum(c) && c != '_') c = '_';
                }
                nodes.push_back({
                    {"id", cleanId},
                    {"label", name + (step.contains("model") ? "\n[" + step.value("model", "") + "]" : "")},
                    {"type", step.value("type", "process")},
                    {"model", step.value("model", "")},
                    {"description", step.value("description", "")}
                });
            }
        }
        if (result.contains("flow_edges") && result["flow_edges"].is_array()) {
            for (const auto& edge : result["flow_edges"]) {
                std::string from = edge.value("from", "");
                std::string to = edge.value("to", "");
                for (char& c : from) { if (!std::isalnum(c) && c != '_') c = '_'; }
                for (char& c : to) { if (!std::isalnum(c) && c != '_') c = '_'; }
                edges.push_back({
                    {"from", from},
                    {"to", to},
                    {"label", edge.value("label", "")}
                });
            }
        }

        nlohmann::json blueprint;
        blueprint["name"] = result.value("name", "AI-Generated Blueprint");
        blueprint["description"] = result.value("description", description.substr(0, 100));
        blueprint["prompt"] = result.value("prompt", "");
        blueprint["tools"] = result.value("tools", nlohmann::json::array());
        blueprint["model_assignments"] = result.value("model_assignments", nlohmann::json::object());
        blueprint["nodes"] = nodes;
        blueprint["edges"] = edges;

        res.set_content(blueprint.dump(), "application/json");
    });

    // Generate Mermaid flowchart from prompt text
    svr.Post("/api/system/prompt/mermaid", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string prompt = body.value("prompt", "");
        auto nodes = body.value("nodes", nlohmann::json::array());
        auto edges = body.value("edges", nlohmann::json::array());

        std::string mmd = "flowchart TD\n";
        mmd += "    classDef core fill:#7c3aed,stroke:#a78bfa,color:#fff\n";
        mmd += "    classDef tool fill:#1e1b4b,stroke:#6366f1,color:#c7d2fe\n";
        mmd += "    classDef decision fill:#312e81,stroke:#818cf8,color:#e0e7ff\n";
        mmd += "    classDef io fill:#0c4a6e,stroke:#38bdf8,color:#bae6fd\n";

        if (nodes.empty()) {
            mmd += "    USER([\"User Input\"]):::io\n";
            mmd += "    PROMPT[\"System Prompt\"]:::core\n";
            mmd += "    THINK{\"Reason & Plan\"}:::decision\n";
            mmd += "    TOOLS[\"Execute Tools\"]:::tool\n";
            mmd += "    VERIFY{\"Verify Results\"}:::decision\n";
            mmd += "    RESPOND([\"Response\"]):::io\n";
            mmd += "    USER --> PROMPT --> THINK\n";
            mmd += "    THINK -->|\"needs tools\"| TOOLS\n";
            mmd += "    TOOLS --> VERIFY\n";
            mmd += "    VERIFY -->|\"more work\"| THINK\n";
            mmd += "    VERIFY -->|\"done\"| RESPOND\n";
            mmd += "    THINK -->|\"direct answer\"| RESPOND\n";
        } else {
            for (const auto& n : nodes) {
                std::string nid = n.value("id", "N");
                std::string label = n.value("label", "Node");
                std::string type = n.value("type", "process");
                std::string cls = "core";
                if (type == "tool") cls = "tool";
                else if (type == "decision") cls = "decision";
                else if (type == "io") cls = "io";

                if (type == "io")
                    mmd += "    " + nid + "([\"" + label + "\"]):::" + cls + "\n";
                else if (type == "decision")
                    mmd += "    " + nid + "{\"" + label + "\"}:::" + cls + "\n";
                else
                    mmd += "    " + nid + "[\"" + label + "\"]:::" + cls + "\n";
            }
            for (const auto& e : edges) {
                std::string from = e.value("from", "");
                std::string to = e.value("to", "");
                std::string label = e.value("label", "");
                if (from.empty() || to.empty()) continue;
                if (label.empty())
                    mmd += "    " + from + " --> " + to + "\n";
                else
                    mmd += "    " + from + " -->|\"" + label + "\"| " + to + "\n";
            }
        }

        nlohmann::json j;
        j["mermaid"] = mmd;
        res.set_content(j.dump(), "application/json");
    });
}

} // namespace avacli
