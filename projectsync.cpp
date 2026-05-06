#include "projectsync.h"
#include "designerwidget.h"
#include "layoutworkspacewidget.h"
#include "layoutengine.h"

#include <iterator>
#include <utility>

import badge.document;
import badge.qtbridge;

namespace projectsync {

namespace {
struct PaperPreset {
    int widthMm;
    int heightMm;
};

constexpr PaperPreset kPaperPresets[] = {
    {297, 420}, // A3
    {210, 297}, // A4
    {148, 210}, // A5
    {257, 364}, // B4
    {182, 257}, // B5
    {216, 279}, // Letter
    {216, 356}, // Legal
};

PaperConfig currentPaperConfig(const QComboBox& paperSize, const QCheckBox& landscape) {
    PaperConfig cfg;
    const int index = paperSize.currentIndex();
    if (index >= 0 && index < static_cast<int>(std::size(kPaperPresets))) {
        cfg.widthMm = kPaperPresets[index].widthMm;
        cfg.heightMm = kPaperPresets[index].heightMm;
    }
    if (landscape.isChecked()) {
        std::swap(cfg.widthMm, cfg.heightMm);
    }
    return cfg;
}

int paperPresetIndexFor(const PaperConfig& paper) {
    for (int i = 0; i < static_cast<int>(std::size(kPaperPresets)); ++i) {
        const auto& preset = kPaperPresets[i];
        if ((paper.widthMm == preset.widthMm && paper.heightMm == preset.heightMm) ||
            (paper.widthMm == preset.heightMm && paper.heightMm == preset.widthMm)) {
            return i;
        }
    }
    return 1; // A4
}

}

badge::DocumentData currentDocument(const QList<BadgeItem>& badges,
                                   const QComboBox& paperSize,
                                   const QCheckBox& landscape,
                                   const QDoubleSpinBox& marginMm,
                                   const QDoubleSpinBox& spacingMm,
                                   const QString& title) {
    PaperConfig paper = currentPaperConfig(paperSize, landscape);
    paper.marginMm = marginMm.value();
    paper.spacingMm = spacingMm.value();
    return badge::DocumentData{
        .title = title.toStdString(),
        .paper = paper,
        .badges = badge::qt::toCoreBadges(badges),
    };
}

void applyDocument(DesignerWidget& designer,
                   LayoutWorkspaceWidget& layoutWorkspace,
                   QComboBox& paperSize,
                   QCheckBox& landscape,
                   QDoubleSpinBox& marginMm,
                   QDoubleSpinBox& spacingMm,
                   const badge::DocumentData& document) {
    designer.clearBadges();
    for (const auto& badge : document.badges) {
        designer.addBadge(badge::qt::fromCoreBadge(badge));
    }

    if (document.paper.widthMm > 0 && document.paper.heightMm > 0) {
        const bool isLandscape = document.paper.widthMm > document.paper.heightMm;
        const int paperIndex = paperPresetIndexFor(document.paper);
        paperSize.setCurrentIndex(paperIndex);
        landscape.setChecked(isLandscape);
    }

    marginMm.setValue(document.paper.marginMm);
    spacingMm.setValue(document.paper.spacingMm);

    layoutWorkspace.setDocument(document);
}

}
