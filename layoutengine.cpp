#include "layoutengine.h"

import badge.layout;
import badge.model;
import badge.qtbridge;

QList<BadgeItem> LayoutEngine::autoLayout(const QList<BadgeItem>& templates, const PaperConfig& config) {
    std::vector<badge::BadgeData> coreTemplates;
    coreTemplates.reserve(templates.size());
    for (const auto& tpl : templates) {
        coreTemplates.push_back(badge::qt::toCoreBadge(tpl));
    }

    const auto coreResult = badge::auto_layout(coreTemplates, config);

    QList<BadgeItem> result;
    result.reserve(static_cast<qsizetype>(coreResult.size()));
    for (const auto& item : coreResult) {
        result.append(badge::qt::fromCoreBadge(item));
    }
    return result;
}

QList<BadgeItem> LayoutEngine::fillPage(const BadgeItem& template_, const PaperConfig& config) {
    const auto coreResult = badge::fill_page(badge::qt::toCoreBadge(template_), config);

    QList<BadgeItem> result;
    result.reserve(static_cast<qsizetype>(coreResult.size()));
    for (const auto& item : coreResult) {
        result.append(badge::qt::fromCoreBadge(item));
    }
    return result;
}
