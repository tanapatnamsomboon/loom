#pragma once
#include "editor_command.h"
#include <deque>
#include <memory>

namespace Weaver {

    // Fixed-depth (50-step) undo/redo stack.
    // Push() executes the command immediately and owns it.
    // IsDirty() compares the current stack depth against the depth recorded
    // at the last MarkSavePoint() call, enabling history-driven dirty tracking.
    class EditorHistory {
    public:
        static constexpr int k_MaxHistory = 50;

        // Execute cmd and push it onto the undo stack. Clears the redo stack.
        void Push(std::unique_ptr<IEditorCommand> cmd);

        bool Undo();
        bool Redo();

        bool CanUndo() const { return !mUndoStack.empty(); }
        bool CanRedo() const { return !mRedoStack.empty(); }

        // Call after a successful scene save to record the clean state.
        void MarkSavePoint() { mSaveDepth = (int)mUndoStack.size(); }

        // True when the current state differs from the recorded save point.
        bool IsDirty() const { return mSaveDepth != (int)mUndoStack.size(); }

        // Reset all history — call when a new scene is loaded.
        void Clear();

    private:
        std::deque<std::unique_ptr<IEditorCommand>> mUndoStack;
        std::deque<std::unique_ptr<IEditorCommand>> mRedoStack;

        // Undo-stack size at the last save; -1 means the save point is no longer reachable
        // (either it was in the redo chain that got discarded, or it scrolled off the cap).
        int mSaveDepth = 0;
    };

} // namespace Weaver
