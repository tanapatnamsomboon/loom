#include "scene_properties_panel.h"
#include "editor/file_dialog.h"
#include <imgui.h>
#include <cstring>

namespace Weaver {

    void ScenePropertiesPanel::OnImGuiRender(bool* p_open) {
        if (!ImGui::Begin("Scene Properties", p_open)) {
            ImGui::End();
            return;
        }

        if (!mContext.ActiveScene) {
            ImGui::TextDisabled("No scene loaded.");
            ImGui::End();
            return;
        }

        RenderSkyboxSection();

        ImGui::End();
    }

    void ScenePropertiesPanel::RenderSkyboxSection() {
        ImGui::TextDisabled("SKYBOX");
        ImGui::Separator();

        // Project-relative HDR path; empty = no skybox at runtime (the editor
        // falls back to its default while authoring). Stored on Scene and
        // persisted via SceneSerializer.
        std::string sky = mContext.ActiveScene->GetSkyboxPath();
        char buf[256] = {};
        std::strncpy(buf, sky.c_str(), sizeof(buf) - 1);

        constexpr float browse_w = 28.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputText("##SkyboxPath", buf, sizeof(buf))) {
            mContext.ActiveScene->SetSkyboxPath(buf);
            mContext.SceneDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("...##SkyboxBrowse", { browse_w, 0.0f })) {
            FileDialog::Open("BrowseSkybox", "Choose HDR Environment", ".hdr",
                [scene = mContext.ActiveScene, dirty = &mContext.SceneDirty](const std::string& abs_path) {
                    if (!scene) return;
                    scene->SetSkyboxPath(FileDialog::MakeAssetRelative(abs_path));
                    *dirty = true;
                });
        }

        if (sky.empty()) {
            ImGui::TextDisabled("Empty — editor uses fallback HDR;\nplay mode renders the clear color.");
        } else {
            ImGui::TextDisabled("HDR (.hdr / Radiance RGBE)");
        }
    }

} // namespace Weaver
