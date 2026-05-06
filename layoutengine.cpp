#include "layoutengine.h"

#include <algorithm>

import badge.layout;
import badge.model;
import badge.qtbridge;

namespace {
constexpr double kCircleBleedMm = 3.0;

double footprintWidth(const BadgeItem& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) + kCircleBleedMm
                              : badge.widthMm;
}

double footprintHeight(const BadgeItem& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) + kCircleBleedMm
                              : badge.heightMm;
}
}

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

QList<BadgeItem> LayoutEngine::autoLayoutGrid(const QList<BadgeItem>& templates, const PaperConfig& config) {
    QList<BadgeItem> result;
    result.reserve(templates.size());

    double curX = config.marginMm;
    double curY = config.marginMm;
    double rowHeight = 0.0;

    for (const auto& tpl : templates) {
        const double tw = footprintWidth(tpl);
        const double th = footprintHeight(tpl);
        const double stepX = tw + config.spacingMm;
        const double stepY = th + config.spacingMm;

        if (curX + tw > config.widthMm - config.marginMm) {
            curX = config.marginMm;
            curY += rowHeight;
            rowHeight = 0.0;
        }
        if (curY + th > config.heightMm - config.marginMm) {
            break;
        }

        BadgeItem item = tpl;
        item.xMm = curX;
        item.yMm = curY;
        result.append(item);
        curX += stepX;
        rowHeight = std::max(rowHeight, stepY);
    }

    return result;
}

QList<BadgeItem> LayoutEngine::fillPageGrid(const BadgeItem& template_, const PaperConfig& config) {
    QList<BadgeItem> result;
    const double tw = footprintWidth(template_);
    const double th = footprintHeight(template_);
    const double stepX = tw + config.spacingMm;
    const double stepY = th + config.spacingMm;

    for (double y = config.marginMm; y + th <= config.heightMm - config.marginMm; y += stepY) {
        for (double x = config.marginMm; x + tw <= config.widthMm - config.marginMm; x += stepX) {
            BadgeItem item = template_;
            item.xMm = x;
            item.yMm = y;
            result.append(item);
        }
    }

    return result;
}
