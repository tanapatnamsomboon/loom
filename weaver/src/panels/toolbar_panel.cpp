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
        float top_y          = mContext.ViewportBounds[0].y + 15.0f;

        ImGui::SetNextWindowPos(ImVec2(center_x, top_y), ImGuiCond_Always, ImVec2(0.5f, 0.0f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration    | ImGuiWindowFlags_NoDocking       |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav         |
                                 ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.3f,  0.3f,  0.3f,  0.5f));

        ImGui::Begin("##Toolbar", nullptr, flags);

        constexpr float kButtonHeight = 28.0f;
        constexpr float kButtonWidth  = 80.0f;

        if (mContext.SceneState == SceneState::Edit) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

            if (ImGui::RadioButton("Select", mContext.GizmoType == -1))
                mContext.GizmoType = -1;
            ImGui::SameLine();
            if (ImGui::RadioButton("Move", mContext.GizmoType == ImGuizmo::OPERATION::TRANSLATE))
                mContext.GizmoType = ImGuizmo::OPERATION::TRANSLATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate", mContext.GizmoType == ImGuizmo::OPERATION::ROTATE))
                mContext.GizmoType = ImGuizmo::OPERATION::ROTATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale", mContext.GizmoType == ImGuizmo::OPERATION::SCALE))
                mContext.GizmoType = ImGuizmo::OPERATION::SCALE;

            ImGui::SameLine(0, 15.0f);
            const char* modes[] = { "Local", "World" };
            if (ImGui::Button(modes[mContext.GizmoMode], ImVec2(60.0f, kButtonHeight)))
                mContext.GizmoMode = mContext.GizmoMode == 0 ? 1 : 0;

            ImGui::PopStyleVar();
            ImGui::SameLine(0, 25.0f);
        }

        bool has_project = Loom::Project::GetActive() != nullptr;
        if (!has_project) ImGui::BeginDisabled();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        if (mContext.SceneState == SceneState::Edit) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
            if (ImGui::Button("Play", ImVec2(kButtonWidth, kButtonHeight)) && mOnPlayPressed)
                mOnPlayPressed();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("Stop", ImVec2(kButtonWidth, kButtonHeight)) && mOnStopPressed)
                mOnStopPressed();
            ImGui::PopStyleColor(3);
        }

        if (!has_project) ImGui::EndDisabled();

        ImGui::SameLine(0, 25.0f);
        if (ImGui::Button("Settings", ImVec2(90.0f, kButtonHeight)))
            ImGui::OpenPopup("EditorSettingsPopup");

        ImGui::PopStyleVar();

        RenderSettingsPopup();

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
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
