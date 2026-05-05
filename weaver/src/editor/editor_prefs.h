#pragma once

#include <string>
#include <vector>

namespace Weaver {

    struct EditorPrefs {
        std::vector<std::string> RecentProjects;
        static constexpr size_t kMaxRecent = 10;
    };

    class EditorPrefsSerializer {
    public:
        static EditorPrefs Load();
        static void        Save(const EditorPrefs& prefs);
    };

} // namespace Weaver
