#include "layoutengine.h"

#include <algorithm>
#include <limits>

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

struct FreeRect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

double footprintArea(const BadgeItem& badge) {
    return footprintWidth(badge) * footprintHeight(badge);
}

bool rectContains(const FreeRect& outer, const FreeRect& inner) {
    return inner.x >= outer.x
        && inner.y >= outer.y
        && inner.x + inner.w <= outer.x + outer.w
        && inner.y + inner.h <= outer.y + outer.h;
}

void pruneFreeRects(QList<FreeRect>& rects) {
    for (int i = 0; i < rects.size(); ++i) {
        const auto& a = rects[i];
        if (a.w <= 0.0 || a.h <= 0.0) {
            rects.removeAt(i--);
            continue;
        }
        for (int j = rects.size() - 1; j > i; --j) {
            const auto& b = rects[j];
            if (rectContains(a, b)) {
                rects.removeAt(j);
            } else if (rectContains(b, a)) {
                rects.removeAt(i--);
                break;
            }
        }
    }
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

QList<BadgeItem> LayoutEngine::packMixed(const QList<BadgeItem>& templates, const PaperConfig& config) {
    QList<BadgeItem> sorted = templates;
    std::stable_sort(sorted.begin(), sorted.end(), [](const BadgeItem& a, const BadgeItem& b) {
        const double areaA = footprintArea(a);
        const double areaB = footprintArea(b);
        if (!qFuzzyCompare(areaA, areaB)) {
            return areaA > areaB;
        }
        const double sideA = std::max(footprintWidth(a), footprintHeight(a));
        const double sideB = std::max(footprintWidth(b), footprintHeight(b));
        if (!qFuzzyCompare(sideA, sideB)) {
            return sideA > sideB;
        }
        if (!qFuzzyCompare(a.widthMm, b.widthMm)) {
            return a.widthMm > b.widthMm;
        }
        return a.heightMm > b.heightMm;
    });

    const double innerWidth = std::max(0.0, config.widthMm - config.marginMm * 2.0);
    const double innerHeight = std::max(0.0, config.heightMm - config.marginMm * 2.0);
    QList<FreeRect> freeRects;
    freeRects.append({config.marginMm, config.marginMm, innerWidth, innerHeight});

    QList<BadgeItem> result;
    result.reserve(sorted.size());

    for (const auto& tpl : sorted) {
        const double tw = footprintWidth(tpl);
        const double th = footprintHeight(tpl);
        const double occupiedW = tw + config.spacingMm;
        const double occupiedH = th + config.spacingMm;

        int bestIndex = -1;
        double bestWaste = std::numeric_limits<double>::infinity();
        double bestShortSide = std::numeric_limits<double>::infinity();
        double bestTop = std::numeric_limits<double>::infinity();
        double bestLeft = std::numeric_limits<double>::infinity();

        for (int i = 0; i < freeRects.size(); ++i) {
            const auto& rect = freeRects[i];
            if (occupiedW > rect.w || occupiedH > rect.h) {
                continue;
            }
            const double waste = rect.w * rect.h - occupiedW * occupiedH;
            const double shortSide = std::min(rect.w - occupiedW, rect.h - occupiedH);
            if (waste < bestWaste
                || (qFuzzyCompare(waste, bestWaste) && shortSide < bestShortSide)
                || (qFuzzyCompare(waste, bestWaste) && qFuzzyCompare(shortSide, bestShortSide) && rect.y < bestTop)
                || (qFuzzyCompare(waste, bestWaste) && qFuzzyCompare(shortSide, bestShortSide) && qFuzzyCompare(rect.y, bestTop) && rect.x < bestLeft)) {
                bestIndex = i;
                bestWaste = waste;
                bestShortSide = shortSide;
                bestTop = rect.y;
                bestLeft = rect.x;
            }
        }

        if (bestIndex < 0) {
            continue;
        }

        const auto chosen = freeRects.takeAt(bestIndex);
        BadgeItem item = tpl;
        item.xMm = chosen.x;
        item.yMm = chosen.y;
        result.append(item);

        const FreeRect right{chosen.x + occupiedW, chosen.y, chosen.w - occupiedW, chosen.h};
        const FreeRect bottom{chosen.x, chosen.y + occupiedH, chosen.w, chosen.h - occupiedH};
        if (right.w > 0.0 && right.h > 0.0) {
            freeRects.append(right);
        }
        if (bottom.w > 0.0 && bottom.h > 0.0) {
            freeRects.append(bottom);
        }
        pruneFreeRects(freeRects);
    }

    return result;
}

QList<QList<BadgeItem>> LayoutEngine::packMixedPages(const QList<BadgeItem>& templates, const PaperConfig& config) {
    QList<QList<BadgeItem>> pages;
    QList<BadgeItem> remaining = templates;

    while (!remaining.isEmpty()) {
        const int beforeCount = remaining.size();
        const QList<BadgeItem> page = packMixed(remaining, config);
        if (page.isEmpty()) {
            break;
        }

        pages.append(page);

        auto sameTemplate = [](const BadgeItem& a, const BadgeItem& b) {
            if (a.productMode != b.productMode
                || a.clipToCircle != b.clipToCircle
                || a.widthMm != b.widthMm
                || a.heightMm != b.heightMm
                || a.imageScale != b.imageScale
                || a.materialPreset != b.materialPreset
                || a.specularStrength != b.specularStrength
                || a.envReflectionStrength != b.envReflectionStrength
                || a.glitterStrength != b.glitterStrength
                || a.rotation != b.rotation
                || a.label != b.label
                || a.imagePath != b.imagePath
                || a.displayText != b.displayText
                || a.brightness != b.brightness
                || a.contrast != b.contrast
                || a.saturation != b.saturation
                || a.flattenedForLayoutTransfer != b.flattenedForLayoutTransfer
                || a.layers.size() != b.layers.size()) {
                return false;
            }
            for (int i = 0; i < a.layers.size(); ++i) {
                const auto& la = a.layers[i];
                const auto& lb = b.layers[i];
                if (la.imagePath != lb.imagePath
                    || la.name != lb.name
                    || la.opacity != lb.opacity
                    || la.visible != lb.visible
                    || la.offsetX != lb.offsetX
                    || la.offsetY != lb.offsetY
                    || la.blendMode != lb.blendMode) {
                    return false;
                }
            }
            return true;
        };

        for (const auto& placed : page) {
            for (int i = 0; i < remaining.size(); ++i) {
                const auto& candidate = remaining[i];
                if (sameTemplate(candidate, placed)) {
                    remaining.removeAt(i);
                    break;
                }
            }
        }

        if (remaining.size() == beforeCount) {
            break;
        }
    }

    return pages;
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
