#include "projectsync.h"
#include "designerwidget.h"
#include "layoutworkspacewidget.h"
#include "layoutengine.h"

#include <utility>

import badge.document;
import badge.qtbridge;

namespace projectsync {

namespace {

QList<BadgeItem> collectBadges(const DesignerWidget& designer) {
    QList<BadgeItem> list;
    for (auto* gi : designer.graphicItems()) {
        list.append(gi->badge());
    }
    return list;
}

PaperConfig currentPaperConfig(const QComboBox& paperSize, const QCheckBox& landscape) {
    PaperConfig cfg;
    cfg.widthMm = paperSize.currentIndex() == 1 ? 182 : 210;
    cfg.heightMm = paperSize.currentIndex() == 1 ? 257 : 297;
    if (landscape.isChecked()) {
        std::swap(cfg.widthMm, cfg.heightMm);
    }
    return cfg;
}

}

badge::DocumentData currentDocument(const DesignerWidget& designer,
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
        .badges = badge::qt::toCoreBadges(collectBadges(designer)),
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
        const int paperIndex = isLandscape ? 1 : 0;
        paperSize.setCurrentIndex(paperIndex);
        landscape.setChecked(isLandscape);
    }

    marginMm.setValue(document.paper.marginMm);
    spacingMm.setValue(document.paper.spacingMm);

    layoutWorkspace.setDocument(document);
}

}
