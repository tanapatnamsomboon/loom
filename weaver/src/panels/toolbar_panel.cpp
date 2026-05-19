#include "toolbar_panel.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <loom/asset/asset_manager.h>
#include <loom/project/project.h>
#include <loom/renderer/renderer_3d.h>

namespace Weaver {

    ToolbarPanel::ToolbarPanel(EditorContext& ctx)
        : mContext(ctx) {}

    void ToolbarPanel::Init() {
        auto pb_path    = Loom::Project::GetEngineAssetFileSystemPath("icons/play_button_icon_version2.png");
        mPlayButtonIcon = Loom::AssetManager::GetTexture(pb_path.generic_string());

        auto sb_path    = Loom::Project::GetEngineAssetFileSystemPath("icons/stop_button_icon.png");
        mStopButtonIcon = Loom::AssetManager::GetTexture(sb_path.generic_string());
    }

    void ToolbarPanel::OnImGuiRender() {
        float viewport_width = mContext.ViewportBounds[1].x - mContext.ViewportBounds[0].x;
        float center_x       = mContext.ViewportBounds[0].x + (viewport_width * 0.5f);
        float top_y          = mContext.ViewportBounds[0].y + 14.0f;

        ImGui::SetNextWindowPos(ImVec2(center_x, top_y), ImGuiCond_Always, ImVec2(0.5f, 0.0f));

        constexpr ImGuiWindowFlags k_flags =
            ImGuiWindowFlags_NoDecoration       | ImGuiWindowFlags_NoDocking         |
            ImGuiWindowFlags_AlwaysAutoResize   | ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav             |
            ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(12.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.88f, 0.88f, 0.88f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.35f, 0.35f, 0.35f, 0.55f));

        ImGui::Begin("##Toolbar", nullptr, k_flags);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        // ── Play / Stop ────────────────────────────────────────────────────
        bool has_project = Loom::Project::GetActive() != nullptr;
        if (!has_project) ImGui::BeginDisabled();

        if (mContext.SceneState == SceneState::Edit) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0, 0, 0, 0));
            if (ImGui::ImageButton(mPlayButtonIcon->GetPath().c_str(), mPlayButtonIcon->GetRendererID(), { 25.0f, 25.0f }) && mOnPlayPressed)
                mOnPlayPressed();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0, 0, 0, 0));
            if (ImGui::ImageButton(mStopButtonIcon->GetPath().c_str(), mStopButtonIcon->GetRendererID(), { 20.0f, 20.0f }) && mOnStopPressed)
                mOnStopPressed();
            ImGui::PopStyleColor(3);
        }

        if (!has_project) ImGui::EndDisabled();

        ImGui::SameLine(0, 12.0f);

        // ── Gizmo operation (W/E/R + Q for None) ──────────────────────────
        auto gizmo_btn = [&](const char* label, GizmoOperation op) {
            bool active = (mContext.GizmoOp == op);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.55f, 0.90f, 1.0f));
            if (ImGui::Button(label, { 32.0f, 26.0f })) mContext.GizmoOp = op;
            if (active) ImGui::PopStyleColor();
        };
        gizmo_btn("T", GizmoOperation::Translate); ImGui::SameLine(0, 2.0f);
        gizmo_btn("R", GizmoOperation::Rotate);    ImGui::SameLine(0, 2.0f);
        gizmo_btn("S", GizmoOperation::Scale);     ImGui::SameLine(0, 2.0f);
        gizmo_btn("-", GizmoOperation::None);

        ImGui::SameLine(0, 8.0f);

        // ── World / Local toggle ──────────────────────────────────────────
        {
            const char* label = (mContext.GizmoMode == GizmoSpace::Local) ? "Local" : "World";
            if (ImGui::Button(label, { 60.0f, 26.0f }))
                mContext.GizmoMode = (mContext.GizmoMode == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
        }

        ImGui::SameLine(0, 12.0f);

        // ── Settings ──────────────────────────────────────────────────────
        if (ImGui::Button("Settings", { 80.0f, 26.0f }))
            ImGui::OpenPopup("EditorSettingsPopup");

        ImGui::PopStyleVar(); // FrameRounding

        RenderSettingsPopup();

        ImGui::End();
        ImGui::PopStyleColor(2); // WindowBg, Border
        ImGui::PopStyleVar(3);   // WindowRounding, WindowBorderSize, WindowPadding
    }

    void ToolbarPanel::RenderSettingsPopup() {
        if (!ImGui::BeginPopup("EditorSettingsPopup"))
            return;

        ImGui::TextDisabled("CAMERA");
        ImGui::Separator();
        float speed = mContext.EditorCamera.GetCameraSpeed();
        if (ImGui::DragFloat("Fly Speed", &speed, 0.1f, 0.1f, 100.0f))
            mContext.EditorCamera.SetCameraSpeed(speed);

        ImGui::Spacing();
        ImGui::TextDisabled("EDITOR GRID");
        ImGui::Separator();
        auto& gs = mContext.Grid;
        ImGui::DragFloat("Minor Scale",  &gs.MinorScale,    0.1f,  0.1f,  10.0f);
        ImGui::DragFloat("Major Scale",  &gs.MajorScale,    0.1f,  1.0f,  100.0f);
        ImGui::DragFloat("Thickness",    &gs.LineThickness, 0.05f, 0.1f,  5.0f);
        ImGui::ColorEdit4("Minor Color", glm::value_ptr(gs.MinorColor));
        ImGui::ColorEdit4("Major Color", glm::value_ptr(gs.MajorColor));

        ImGui::Spacing();
        ImGui::TextDisabled("PHYSICS");
        ImGui::Separator();
        bool show_colliders = mContext.ActiveScene->IsShowingPhysicsColliders();
        if (ImGui::Checkbox("Show Colliders", &show_colliders)) {
            mContext.EditorScene->SetShowPhysicsColliders(show_colliders);
            if (mContext.ActiveScene != mContext.EditorScene)
                mContext.ActiveScene->SetShowPhysicsColliders(show_colliders);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("DEBUG VIZ");
        ImGui::Separator();
        {
            // Render the irradiance map as the skybox to inspect what the B.2
            // convolution actually produced. The irradiance cubemap should
            // look like a very low-frequency smoothed version of the
            // environment — if it has any visible high-frequency structure,
            // the convolution is broken.
            using SkyboxSource = Loom::Scene::SkyboxSource;
            int source = (int)mContext.ActiveScene->GetSkyboxSource();
            const char* labels[] = { "Environment (B.1)", "Irradiance (B.2 debug)" };
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##SkyboxSource", &source, labels, IM_ARRAYSIZE(labels))) {
                mContext.ActiveScene->SetSkyboxSource((SkyboxSource)source);
            }
            ImGui::TextDisabled("Skybox Source — what the viewport renders");

            // Debug viz for mesh shading. Bypasses PBR and outputs raw
            // intermediates so we can isolate where the IBL pipeline is
            // misbehaving. Reset to PBR (0) on next session.
            static int debug_viz = 0;
            const char* viz_labels[] = {
                "PBR (default)",
                "Irradiance sample",
                "World normal",
                "NdotL (light 0)",
                "NdotV",
                "Albedo only",
            };
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##DebugViz", &debug_viz, viz_labels, IM_ARRAYSIZE(viz_labels))) {
                Loom::Renderer3D::SetDebugViz(debug_viz);
            }
            ImGui::TextDisabled("Mesh Debug — what the sphere fragment outputs");
        }

        ImGui::EndPopup();
    }

} // namespace Weaver
