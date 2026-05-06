# Layout Transfer Spec

This document describes the data that moves from the Designer view to the Layout view.

## Goal

- The Designer is for visual editing and preview.
- The Layout view is for placement and export.
- Transfer must use raw badge content, not Designer preview effects.

## What Must Be Sent

- Badge position, size, and rotation.
- Raw image layers and their order.
- Layer visibility, opacity, and offsets.
- Badge text content if present.
- The badge geometry needed for layout placement.

## What Must Not Be Sent

- Designer lighting preview.
- Glitter preview.
- Surface gloss / reflection preview.
- Any guide overlays.
- Selection overlays or crop-mark visuals from the Designer UI.

## Central Guide Rule

- Each badge is rendered in Designer-space first, using its actual scene position.
- The transfer should prefer the live `BadgeGraphicItem` scene transform from the Designer view, rather than reconstructing placement from a copied data object.
- The transfer source is then clipped against the Designer's red dotted circular guide.
- The full area inside that red dotted circle must be preserved as a fixed-size transfer image.
- The transfer must not trim transparent margins after clipping, because that would destroy the original placement inside the guide.
- The layout transfer source is the clipped result, not the full preview badge.
- The implementation may render a temporary one-badge scene to produce that crop.
- The Layout side should not receive Designer preview shading or other presentation-only effects.

## Rendering Rule

- Layout rendering should draw raw badge content only.
- The transfer image is a raster crop of the Designer content inside the red dotted guide, not a fixed center crop.
- The transfer image must keep the guide-relative position of the artwork intact.
- Circular badge transfer uses the Designer safe area as the clip target, and the area outside the safe clip stays transparent before layout rendering.
- The Layout preview must not recreate the Designer's gloss, glitter, or lighting simulation.

## Notes for Future Changes

- If a new Designer visual effect is added, assume it must stay out of layout transfer unless this document says otherwise.
- If a new layout-only property is added, keep it separate from Designer preview state.
