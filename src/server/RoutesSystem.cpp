#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "platform/Paths.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

namespace avacli {

namespace fs = std::filesystem;

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

void registerSystemRoutes(httplib::Server& svr, ServerContext ctx) {

    // Get active system prompt
    svr.Get("/api/system/prompt", [ctx](const httplib::Request&, httplib::Response& res) {
        auto path = activePromptPath(ctx.config->workspace);
        std::string content;
        if (fs::exists(path)) {
            std::ifstream ifs(path);
            if (ifs) content.assign(std::istreambuf_iterator<char>(ifs),
                                     std::istreambuf_iterator<char>());
        }
        nlohmann::json j;
        j["prompt"] = content;
        j["path"] = path;
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

    // List blueprints
    svr.Get("/api/system/blueprints", [](const httplib::Request&, httplib::Response& res) {
        auto dir = blueprintsDir();
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
