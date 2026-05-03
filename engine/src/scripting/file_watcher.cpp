#include "scripting/file_watcher.h"

namespace Loom {

    FileWatcher::FileWatcher(std::chrono::milliseconds poll_interval)
        : mPollInterval(poll_interval) {}

    FileWatcher::~FileWatcher() {
        mRunning = false;
        if (mThread.joinable())
            mThread.join();
    }

    void FileWatcher::Watch(const std::string& path) {
        {
            std::lock_guard lock(mMutex);
            try {
                mWatchedFiles[path] = std::filesystem::last_write_time(path);
            } catch (...) {
                mWatchedFiles[path] = {};
            }
        }

        if (!mRunning.exchange(true))
            mThread = std::thread(&FileWatcher::ThreadProc, this);
    }

    void FileWatcher::Clear() {
        std::lock_guard lock(mMutex);
        mWatchedFiles.clear();
        mPendingChanges.clear();
    }

    std::vector<std::string> FileWatcher::FlushChanges() {
        std::lock_guard lock(mMutex);
        return std::move(mPendingChanges);
    }

    void FileWatcher::ThreadProc() {
        while (mRunning) {
            std::this_thread::sleep_for(mPollInterval);
            if (!mRunning)
                break;

            // Snapshot watched paths without holding the lock during filesystem calls.
            std::vector<std::pair<std::string, std::filesystem::file_time_type>> snapshot;
            {
                std::lock_guard lock(mMutex);
                snapshot.assign(mWatchedFiles.begin(), mWatchedFiles.end());
            }

            for (auto& [path, known_time] : snapshot) {
                std::filesystem::file_time_type current_time;
                try {
                    current_time = std::filesystem::last_write_time(path);
                } catch (...) {
                    // File temporarily unavailable (e.g. editor is mid-save).
                    continue;
                }

                if (current_time != known_time) {
                    std::lock_guard lock(mMutex);
                    auto it = mWatchedFiles.find(path);
                    // Guard against the file being unwatched between snapshot and lock.
                    if (it != mWatchedFiles.end() && it->second == known_time) {
                        it->second = current_time;
                        mPendingChanges.push_back(path);
                    }
                }
            }
        }
    }

} // namespace Loom
