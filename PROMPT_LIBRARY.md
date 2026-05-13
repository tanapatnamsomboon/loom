# Loom Engine - AI Prompt Library

Use these prompts to guide Claude Code efficiently while maintaining project rules.

## 1. Resume / Context Refresh (Use after `/clear` or starting a new day)
```text
Context: We are resuming work on the Loom Engine. 
Please read `CLAUDE.md` and `PROJECT_LOG.md` to restore your context. 
Remember our workflow: I handle ImGui/UI, you handle Engine/Backend logic.
Briefly summarize our current status and propose the next step from the roadmap.
```

## 2. Start a New Roadmap Feature
```text
Context: We are ready to start a new feature from the Roadmap in `CLAUDE.md`.
Please review the pending roadmap items. Propose the best candidate to implement next and briefly explain your technical approach (especially regarding `engine/` architecture).
Hold off on writing code or adding submodules until I approve the proposal.
```

## 3. Debugging / Bug Fixing
```text
Context: We have an issue in the engine backend. 
[Describe the bug here, e.g., "The Texture2D reload is crashing the sandbox"].
Please analyze the relevant files in `engine/`. Provide a brief root cause analysis.
Fix the issue while strictly adhering to `CLAUDE.md` rules (e.g., no raw OpenGL in core engine). Provide a quick snippet for `sandbox/` so I can verify the fix.
```

## 4. Finalize & Commit (Use when a feature is done and tested)
```text
Context: I have tested your recent changes and everything is working perfectly.
Please perform the final wrap-up:
1. Generate the exact `git commit` command using Conventional Commits as per `CLAUDE.md`.
2. Provide the text to update `PROJECT_LOG.md` with this new completed step.
3. If this completes a major roadmap Phase, suggest updates for `README.md`.
```

## 5. Add a New Vendor Library
```text
Context: We need to add a new third-party library: [Library Name].
As per `CLAUDE.md` rules, do NOT execute git submodule commands yourself. 
Provide me with the exact `git submodule add` command. Once I confirm it's done, you may proceed to update `cmake/vendors.cmake`.
```

## 6. Review & Adjust Internal Docs (`CLAUDE.md` & `PROJECT_LOG.md`)
```text
Context: I want to perform periodic maintenance on our internal project documentation.
Please review the current state of the codebase against `CLAUDE.md` and `PROJECT_LOG.md`.
1. **CLAUDE.md:** Check if there are any outdated rules, completed roadmap items, or new architectural patterns we recently introduced that need to be documented.
2. **PROJECT_LOG.md:** Ensure the history logs and roadmap status accurately reflect our recent commits and current development phase.
3. **Action:** Propose specific adjustments to keep both files clean, highly scannable, and perfectly aligned. Do not write the changes until I approve the proposal.
```

## 7. Review & Update Public Docs (`README.md`)
```text
Context: We have recently completed a major roadmap milestone.
As per the documentation rules in `CLAUDE.md`, it is time to update our public-facing `README.md`.
1. Review `README.md` against our recent progress in `PROJECT_LOG.md` and the codebase.
2. Propose updates to the "Features", "Current State", and "Dependencies" sections to accurately reflect Loom Engine's new capabilities.
3. Ensure the instructions (like build steps or dependencies) are still correct.
4. Show me the proposed Markdown changes for review before applying them. (Remember: Do NOT propose updates for minor bug fixes or micro-steps).
```

---