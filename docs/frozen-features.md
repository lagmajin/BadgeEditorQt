# Frozen Features

This document records features that are intentionally frozen rather than deleted.

## Sticker Track

Status: frozen

Scope:

- `ProductMode::Sticker`
- non-circle `GuideShape` values used for sticker-like workflows
- sticker-specific editor UX
- sticker-specific layout/export polish
- print-shop additions tied to sticker output, such as white-ink underlay and named cut-line spot colors

Rules:

- Keep load/save compatibility for existing documents that already contain sticker-related fields.
- Do not remove `ProductMode`, `GuideShape`, or related JSON fields unless a migration plan is prepared first.
- Do not add new Sticker-specific UI, workflow, or export behavior while this feature is frozen.
- Shared rendering/layout code may stay in place if removing it would risk regressions in badge behavior.

Practical meaning:

- Existing code paths may still mention `Sticker` for compatibility.
- New work should treat `Badge` as the only active product direction.
- If Sticker work is resumed later, reopen it explicitly instead of continuing implicitly from dormant code.
