#include "toolbar_panel.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h>
// clang-format on
#include <loom/project/project.h>

namespace Weaver {

    ToolbarPanel::ToolbarPanel(EditorContext& ctx)
        : mContext(ctx) {}

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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(12.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.35f, 0.35f, 0.35f, 0.55f));

        ImGui::Begin("##Toolbar", nullptr, k_flags);

        constexpr float kH = 26.0f; // uniform item height

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        // ── Gizmo tools (edit mode only) ──────────────────────────────────
        if (mContext.SceneState == SceneState::Edit) {
            static const char* k_labels[] = { "Select (Q)", "Move   (W)", "Rotate (E)", "Scale  (R)" };
            static const int   kValues[] = { -1, ImGuizmo::OPERATION::TRANSLATE,
                                                 ImGuizmo::OPERATION::ROTATE,
                                                 ImGuizmo::OPERATION::SCALE };

            int current = 0;
            for (int i = 0; i < 4; i++) {
                if (kValues[i] == mContext.GizmoType) { current = i; break; }
            }

            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::BeginCombo("##GizmoTool", k_labels[current])) {
                for (int i = 0; i < 4; i++) {
                    bool sel = (i == current);
                    if (ImGui::Selectable(k_labels[i], sel))
                        mContext.GizmoType = kValues[i];
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Gizmo tool  (Q / W / E / R)");

            ImGui::SameLine(0, 6.0f);

            // Local / World toggle — blue tint when World is active
            bool is_world = (mContext.GizmoMode == 1);
            if (is_world) {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.44f, 0.78f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.52f, 0.86f, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.38f, 0.68f, 1.00f));
            }
            if (ImGui::Button(is_world ? "World" : "Local", { 58.0f, kH }))
                mContext.GizmoMode ^= 1;
            if (is_world) ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle transform space");

            // Vertical separator via draw list (no imgui_internal.h required)
            ImGui::SameLine(0, 14.0f);
            {
                ImVec2 p = ImGui::GetCursorScreenPos();
                float  h = ImGui::GetFrameHeight();
                ImGui::GetWindowDrawList()->AddLine(
                    { p.x, p.y + 3.0f }, { p.x, p.y + h - 3.0f },
                    IM_COL32(140, 140, 140, 90), 1.0f);
                ImGui::Dummy({ 1.0f, h });
            }
            ImGui::SameLine(0, 14.0f);
        }

        // ── Play / Stop ────────────────────────────────────────────────────
        bool has_project = Loom::Project::GetActive() != nullptr;
        if (!has_project) ImGui::BeginDisabled();

        if (mContext.SceneState == SceneState::Edit) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.65f, 0.20f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.75f, 0.28f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.55f, 0.14f, 1.00f));
            if (ImGui::Button("Play", { 72.0f, kH }) && mOnPlayPressed)
                mOnPlayPressed();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.18f, 0.18f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.28f, 0.28f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.12f, 0.12f, 1.00f));
            if (ImGui::Button("Stop", { 72.0f, kH }) && mOnStopPressed)
                mOnStopPressed();
            ImGui::PopStyleColor(3);
        }

        if (!has_project) ImGui::EndDisabled();

        ImGui::SameLine(0, 8.0f);

        // ── Settings ──────────────────────────────────────────────────────
        if (ImGui::Button("Settings", { 80.0f, kH }))
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

        ImGui::EndPopup();
    }

} // namespace Weaver
