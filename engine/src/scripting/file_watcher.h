#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace Loom {

    // Background-thread file watcher using std::filesystem::last_write_time polling.
    // Call Watch() to register paths, FlushChanges() on the main thread each frame
    // to get the list of paths that'd changed since the last flush.
    class FileWatcher {
    public:
        explicit FileWatcher(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250));
        ~FileWatcher();

        void Watch(const std::string& path);
        void Clear();

        // Returns paths that'd changed since the last call. Call on the main thread only.
        std::vector<std::string> FlushChanges();

    private:
        void ThreadProc();

    private:
        std::chrono::milliseconds mPollInterval;
        std::unordered_map<std::string, std::filesystem::file_time_type> mWatchedFiles;
        std::vector<std::string> mPendingChanges;
        std::mutex mMutex;
        std::thread mThread;
        std::atomic<bool> mRunning{ false };
    };

} // namespace Loom
