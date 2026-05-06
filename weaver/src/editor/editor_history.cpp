#include "editor_history.h"

namespace Weaver {

    void EditorHistory::Push(std::unique_ptr<IEditorCommand> cmd) {
        cmd->Execute();

        // If the save point sits inside the redo chain that is about to be
        // discarded, it is no longer reachable — mark permanently dirty.
        if (!mRedoStack.empty() && mSaveDepth > (int)mUndoStack.size())
            mSaveDepth = -1;
        mRedoStack.clear();

        mUndoStack.push_back(std::move(cmd));

        // Enforce the history cap. If the oldest entry falls off, adjust the
        // save-point depth accordingly (it may become unreachable).
        if ((int)mUndoStack.size() > k_MaxHistory) {
            mUndoStack.pop_front();
            if (mSaveDepth > 0) --mSaveDepth;
            else                mSaveDepth = -1;
        }
    }

    bool EditorHistory::Undo() {
        if (mUndoStack.empty()) return false;
        mUndoStack.back()->Undo();
        mRedoStack.push_back(std::move(mUndoStack.back()));
        mUndoStack.pop_back();
        return true;
    }

    bool EditorHistory::Redo() {
        if (mRedoStack.empty()) return false;
        mRedoStack.back()->Execute();
        mUndoStack.push_back(std::move(mRedoStack.back()));
        mRedoStack.pop_back();
        return true;
    }

    void EditorHistory::Clear() {
        mUndoStack.clear();
        mRedoStack.clear();
        mSaveDepth = 0;
    }

} // namespace Weaver
