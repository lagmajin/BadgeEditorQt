#include "layoutengine.h"

#include <algorithm>
#include <limits>

#include <QPainterPath>

namespace {

double bleedMm(const BadgeItem& badge) {
    return std::max(0.0, badge.guide.bleedMm);
}

GuideShape packingShape(const BadgeItem& badge) {
    return badge.clipToCircle ? GuideShape::Circle : badge.guide.shape;
}

double contentWidth(const BadgeItem& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) : badge.widthMm;
}

double contentHeight(const BadgeItem& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) : badge.heightMm;
}

double footprintWidth(const BadgeItem& badge) {
    return contentWidth(badge) + bleedMm(badge) * 2.0;
}

double footprintHeight(const BadgeItem& badge) {
    return contentHeight(badge) + bleedMm(badge) * 2.0;
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

QPointF contentPositionFromFootprint(const BadgeItem& badge, const QPointF& footprintPos) {
    const double bleed = bleedMm(badge);
    return footprintPos + QPointF(bleed, bleed);
}

QRectF footprintRect(const BadgeItem& badge, const QPointF& contentPos) {
    const double bleed = bleedMm(badge);
    return QRectF(contentPos.x() - bleed,
                  contentPos.y() - bleed,
                  footprintWidth(badge),
                  footprintHeight(badge));
}

QPainterPath footprintPath(const BadgeItem& badge, const QPointF& contentPos) {
    const QRectF rect = footprintRect(badge, contentPos);
    QPainterPath path;
    switch (packingShape(badge)) {
    case GuideShape::Rectangle:
        path.addRect(rect);
        break;
    case GuideShape::RoundedRectangle: {
        const double radius = std::clamp(badge.guide.cornerRadiusMm + bleedMm(badge),
                                         0.0,
                                         std::min(rect.width(), rect.height()) * 0.5);
        path.addRoundedRect(rect, radius, radius);
        break;
    }
    case GuideShape::Oval:
        path.addEllipse(rect);
        break;
    case GuideShape::Circle:
    default:
        path.addEllipse(rect);
        break;
    }
    return path;
}

bool canPlaceWithoutOverlap(const BadgeItem& badge,
                            const QPointF& contentPos,
                            const QList<QPainterPath>& placedPaths,
                            const QList<QRectF>& placedBounds) {
    const QPainterPath candidate = footprintPath(badge, contentPos);
    const QRectF bounds = candidate.boundingRect();
    for (int i = 0; i < placedPaths.size(); ++i) {
        if (!bounds.intersects(placedBounds[i])) {
            continue;
        }
        if (candidate.intersects(placedPaths[i])) {
            return false;
        }
    }
    return true;
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

} // namespace

QList<BadgeItem> LayoutEngine::autoLayout(const QList<BadgeItem>& templates, const PaperConfig& config) {
    QList<BadgeItem> result;
    result.reserve(templates.size());

    double curX = config.marginMm;
    double curY = config.marginMm;
    double rowHeight = 0.0;
    QList<QPainterPath> placedPaths;
    QList<QRectF> placedBounds;
    bool pageFull = false;

    for (const auto& tpl : templates) {
        const double tw = footprintWidth(tpl);
        const double th = footprintHeight(tpl);
        const double stepX = tw + config.spacingMm;
        const double stepY = th + config.spacingMm;
        const QPointF contentOffset = contentPositionFromFootprint(tpl, QPointF(0.0, 0.0));
        bool placed = false;

        for (int attempt = 0; attempt < 2 && !placed; ++attempt) {
            if (curX + tw > config.widthMm - config.marginMm) {
                curX = config.marginMm;
                curY += rowHeight;
                rowHeight = 0.0;
            }
            if (curY + th > config.heightMm - config.marginMm) {
                pageFull = true;
                break;
            }

            const QPointF candidatePos(curX + contentOffset.x(), curY + contentOffset.y());
            if (!canPlaceWithoutOverlap(tpl, candidatePos, placedPaths, placedBounds)) {
                curX = config.marginMm;
                curY += rowHeight;
                rowHeight = 0.0;
                continue;
            }

            BadgeItem item = tpl;
            item.xMm = candidatePos.x();
            item.yMm = candidatePos.y();
            const QPainterPath path = footprintPath(item, QPointF(item.xMm, item.yMm));
            result.append(item);
            placedPaths.append(path);
            placedBounds.append(path.boundingRect());
            placed = true;
        }

        if (!placed) {
            if (pageFull) {
                break;
            }
            continue;
        }

        curX += stepX;
        rowHeight = std::max(rowHeight, stepY);
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
    QList<QPainterPath> placedPaths;
    QList<QRectF> placedBounds;

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

        const auto chosen = freeRects[bestIndex];
        const QPointF candidatePos = contentPositionFromFootprint(tpl, QPointF(chosen.x, chosen.y));
        if (!canPlaceWithoutOverlap(tpl, candidatePos, placedPaths, placedBounds)) {
            freeRects.removeAt(bestIndex);
            continue;
        }

        freeRects.takeAt(bestIndex);
        BadgeItem item = tpl;
        item.xMm = candidatePos.x();
        item.yMm = candidatePos.y();
        const QPainterPath path = footprintPath(item, QPointF(item.xMm, item.yMm));
        result.append(item);
        placedPaths.append(path);
        placedBounds.append(path.boundingRect());

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
                || a.guide.shape != b.guide.shape
                || a.guide.bleedMm != b.guide.bleedMm
                || a.guide.safeInsetMm != b.guide.safeInsetMm
                || a.guide.cornerRadiusMm != b.guide.cornerRadiusMm
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
    QList<BadgeItem> result;
    const double tw = footprintWidth(template_);
    const double th = footprintHeight(template_);
    const double stepX = tw + config.spacingMm;
    const double stepY = th + config.spacingMm;
    const QPointF contentOffset = contentPositionFromFootprint(template_, QPointF(0.0, 0.0));

    for (double y = config.marginMm; y + th <= config.heightMm - config.marginMm; y += stepY) {
        for (double x = config.marginMm; x + tw <= config.widthMm - config.marginMm; x += stepX) {
            BadgeItem item = template_;
            item.xMm = x + contentOffset.x();
            item.yMm = y + contentOffset.y();
            result.append(item);
        }
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
        const QPointF contentOffset = contentPositionFromFootprint(tpl, QPointF(0.0, 0.0));

        if (curX + tw > config.widthMm - config.marginMm) {
            curX = config.marginMm;
            curY += rowHeight;
            rowHeight = 0.0;
        }
        if (curY + th > config.heightMm - config.marginMm) {
            break;
        }

        BadgeItem item = tpl;
        item.xMm = curX + contentOffset.x();
        item.yMm = curY + contentOffset.y();
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
    const QPointF contentOffset = contentPositionFromFootprint(template_, QPointF(0.0, 0.0));

    for (double y = config.marginMm; y + th <= config.heightMm - config.marginMm; y += stepY) {
        for (double x = config.marginMm; x + tw <= config.widthMm - config.marginMm; x += stepX) {
            BadgeItem item = template_;
            item.xMm = x + contentOffset.x();
            item.yMm = y + contentOffset.y();
            result.append(item);
        }
    }

    return result;
}
