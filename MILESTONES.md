# Milestones

## Phase 1: Surface Separation
- Split Designer and Layout into separate widgets.
- Keep MainWindow as the wiring layer only.
- Preserve current actions, menus, and shortcuts.

## Phase 2: Document Boundary
- Introduce a shared badge document model.
- Make save/load operate only through the document boundary.
- Reduce direct item inspection from MainWindow.

## Phase 3: Qt-Free Core
- Move pure data and layout logic into C++ modules.
- Keep Qt-specific conversion at the edges.
- Prepare the codebase for unit tests without GUI dependencies.

## Phase 3.5: Color Management Baseline
- Normalize common input images to sRGB at load time.
- Preserve embedded color profiles when Qt can read them.
- Keep the correction pipeline operating in a predictable display space.

## Phase 4: Editing Robustness
- Add undo/redo commands for move, resize, image changes, and delete.
- Make selection and inspector updates fully consistent.

## Phase 5: Export and Polish
- Implement PNG and PDF export.
- Align layout preview with export output.
- Tighten theming, empty states, and error handling.
