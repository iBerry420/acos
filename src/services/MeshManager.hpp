#pragma once

#include <atomic>
#include <thread>

namespace avacli {

class MeshManager {
public:
    static MeshManager& instance();

    void start();
    void stop();
    bool running() const { return running_; }

    void healthCheckAll();
    void syncWithPeers();
    void checkProvisioningNodes();

private:
    MeshManager() = default;
    ~MeshManager();
    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    void runLoop();

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
};

} // namespace avacli
