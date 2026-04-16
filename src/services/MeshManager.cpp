#include "services/MeshManager.hpp"
#include "client/NodeClient.hpp"
#include "db/Database.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace avacli {

static int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

MeshManager& MeshManager::instance() {
    static MeshManager mm;
    return mm;
}

MeshManager::~MeshManager() {
    stop();
}

void MeshManager::start() {
    if (running_) return;
    stopFlag_ = false;
    running_ = true;
    thread_ = std::thread(&MeshManager::runLoop, this);
    spdlog::info("MeshManager started");
}

void MeshManager::stop() {
    if (!running_) return;
    stopFlag_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
    spdlog::info("MeshManager stopped");
}

void MeshManager::runLoop() {
    // Initial delay to let the server fully start
    for (int i = 0; i < 10 && !stopFlag_; i++)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    int cycle = 0;
    while (!stopFlag_) {
        try {
            // Health check every 30 seconds
            healthCheckAll();

            // Mesh sync with peers every 60 seconds
            if (cycle % 2 == 0)
                syncWithPeers();

            // Check provisioning nodes every cycle
            checkProvisioningNodes();
        } catch (const std::exception& ex) {
            spdlog::warn("MeshManager cycle error: {}", ex.what());
        }
        cycle++;

        // Sleep 30s in 1s increments so we can respond to stop quickly
        for (int i = 0; i < 30 && !stopFlag_; i++)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void MeshManager::healthCheckAll() {
    nlohmann::json rows;
    try {
        rows = Database::instance().query("SELECT * FROM nodes WHERE status != 'provisioning'");
    } catch (...) { return; }

    for (const auto& row : rows) {
        if (stopFlag_) return;

        NodeInfo ni;
        ni.id = row.value("id", "");
        ni.ip = row.value("ip", "");
        ni.port = row.value("port", 8080);
        ni.domain = row.value("domain", "");
        ni.label = row.value("label", "");
        ni.password = row.value("auth_token", "");

        if (ni.ip.empty() || ni.ip == "0.0.0.0") continue;

        NodeClient client(ni);
        auto hr = client.checkHealth();

        std::string status = "unreachable";
        if (hr.reachable && hr.isAvacli && hr.authOk) status = "connected";
        else if (hr.reachable && !hr.authOk) status = "auth_failed";
        else if (hr.reachable && !hr.isAvacli) status = "not_avacli";

        try {
            Database::instance().execute(
                "UPDATE nodes SET status = ?1, latency_ms = ?2, version = ?3, workspace = ?4, last_seen = ?5 "
                "WHERE id = ?6",
                {status, std::to_string(hr.latencyMs), hr.version, hr.workspace,
                 std::to_string(nowSec()), ni.id});
        } catch (...) {}
    }
}

void MeshManager::syncWithPeers() {
    nlohmann::json localNodes;
    try {
        localNodes = Database::instance().query(
            "SELECT id, ip, port, domain, label, added_at FROM nodes WHERE status = 'connected'");
    } catch (...) { return; }

    nlohmann::json connectedNodes;
    try {
        connectedNodes = Database::instance().query(
            "SELECT * FROM nodes WHERE status = 'connected'");
    } catch (...) { return; }

    for (const auto& row : connectedNodes) {
        if (stopFlag_) return;

        NodeInfo ni;
        ni.id = row.value("id", "");
        ni.ip = row.value("ip", "");
        ni.port = row.value("port", 8080);
        ni.domain = row.value("domain", "");
        ni.password = row.value("auth_token", "");

        NodeClient client(ni);
        nlohmann::json syncBody;
        syncBody["source_id"] = "local";
        syncBody["nodes"] = localNodes;
        syncBody["timestamp"] = nowSec();

        auto result = client.proxyPost("/api/mesh/sync", syncBody);
        if (result.contains("nodes")) {
            auto remoteNodes = result["nodes"];
            for (const auto& rn : remoteNodes) {
                std::string id = rn.value("id", "");
                std::string ip = rn.value("ip", "");
                if (id.empty() || ip.empty()) continue;

                auto existing = Database::instance().queryOne(
                    "SELECT id FROM nodes WHERE id = ?1", {id});
                if (existing.is_null()) {
                    try {
                        auto removed = Database::instance().queryOne(
                            "SELECT COUNT(*) AS c FROM mesh_events WHERE event_type = 'node_removed' "
                            "AND node_id = ?1",
                            {id});
                        int removedCount = removed.is_null() ? 0 : removed.value("c", 0);
                        if (removedCount > 0) {
                            spdlog::debug("Mesh sync: skip re-adding user-removed node {}", id);
                            continue;
                        }
                        Database::instance().execute(
                            "INSERT INTO nodes (id, ip, port, domain, label, status, added_at) "
                            "VALUES (?1, ?2, ?3, ?4, ?5, 'unknown', ?6)",
                            {id, ip, std::to_string(rn.value("port", 8080)),
                             rn.value("domain", ""), rn.value("label", ip),
                             std::to_string(rn.value("added_at", nowSec()))});
                        spdlog::info("Mesh sync: discovered new node {} ({})", rn.value("label", ""), ip);
                    } catch (...) {}
                }
            }
        }
    }
}

void MeshManager::checkProvisioningNodes() {
    nlohmann::json rows;
    try {
        rows = Database::instance().query(
            "SELECT * FROM nodes WHERE status = 'provisioning' AND vultr_instance_id != ''");
    } catch (...) { return; }

    for (const auto& row : rows) {
        if (stopFlag_) return;

        std::string ip = row.value("ip", "");
        if (ip.empty() || ip == "0.0.0.0") continue;

        NodeInfo ni;
        ni.id = row.value("id", "");
        ni.ip = ip;
        ni.port = row.value("port", 8080);
        ni.password = row.value("auth_token", "");

        NodeClient client(ni);
        auto hr = client.checkHealth();

        if (hr.reachable && hr.isAvacli) {
            try {
                Database::instance().execute(
                    "UPDATE nodes SET status = 'connected', latency_ms = ?1, version = ?2, "
                    "workspace = ?3, last_seen = ?4 WHERE id = ?5",
                    {std::to_string(hr.latencyMs), hr.version, hr.workspace,
                     std::to_string(nowSec()), ni.id});
                Database::instance().execute(
                    "INSERT INTO mesh_events (event_type, node_id, payload, created_at) "
                    "VALUES ('node_online', ?1, ?2, ?3)",
                    {ni.id, nlohmann::json({{"version", hr.version}, {"latency_ms", hr.latencyMs}}).dump(),
                     std::to_string(nowSec())});
                spdlog::info("Provisioned node {} is now online", row.value("label", ""));
            } catch (...) {}
        }
    }
}

} // namespace avacli
