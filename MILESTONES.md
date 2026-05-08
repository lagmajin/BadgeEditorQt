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

## Phase 6: Product Modes
- Introduce a product mode concept without breaking existing badge documents.
- Keep Badge as the default mode.
- Add Sticker as an experimental mode with separate guide rules.

## Phase 7: Sticker Guide Model
- Replace circle-only badge guides with reusable guide geometry.
- Support sticker finish line, bleed line, and safe area.
- Start with circle, rectangle, rounded rectangle, and oval presets.

## Phase 8: Sticker Editing Workflow
- Add shape preset controls to the Designer inspector.
- Keep image layers, color correction, transforms, and layout transfer shared.
- Make non-square sticker resizing update artwork without changing guide intent.

## Phase 9: Sticker Layout and Export
- Reuse the existing Layout workspace for sticker sheet placement.
- Add sticker-specific cut and bleed previews in Layout.
- Export sticker sheets to PDF and PNG with preview output matching Layout.

## Phase 10: Print-Shop Features
- Add automatic border/outline generation from transparent artwork.
- Add optional white-ink underlay support for transparent stickers.
- Prepare PDF cut lines for print-shop workflows, including named spot-color paths.
