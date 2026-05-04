#include "layoutengine.h"
#include <cmath>

QList<BadgeItem> LayoutEngine::autoLayout(const QList<BadgeItem>& templates, const PaperConfig& config) {
    QList<BadgeItem> result;
    double curX = config.marginMm, curY = config.marginMm, rowHeight = 0;
    bool shift = false;

    for (const auto& tpl : templates) {
        double bw = tpl.widthMm + config.spacingMm;
        double bh = tpl.heightMm + config.spacingMm;
        double xOff = shift ? tpl.widthMm / 2.0 : 0.0;

        if (curX + bw + xOff > config.widthMm - config.marginMm) {
            curX = config.marginMm;
            curY += rowHeight;
            rowHeight = 0;
            shift = !shift;
            xOff = shift ? tpl.widthMm / 2.0 : 0.0;
        }
        if (curY + bh > config.heightMm - config.marginMm) break;

        BadgeItem item = tpl;
        item.xMm = curX + xOff;
        item.yMm = curY;
        result.append(item);
        rowHeight = std::max(rowHeight, tpl.heightMm * 0.88);
        curX += bw;
    }
    return result;
}

QList<BadgeItem> LayoutEngine::fillPage(const BadgeItem& template_, const PaperConfig& config) {
    QList<BadgeItem> result;
    double bw = template_.widthMm + config.spacingMm;
    double vStep = template_.heightMm * 0.88;

    for (double y = config.marginMm; y + template_.heightMm < config.heightMm - config.marginMm; y += vStep) {
        bool shift = (int((y - config.marginMm) / vStep) % 2 == 1);
        for (double x = config.marginMm + (shift ? bw / 2.0 : 0.0);
             x + template_.widthMm < config.widthMm - config.marginMm; x += bw) {
            BadgeItem item = template_;
            item.xMm = x;
            item.yMm = y;
            result.append(item);
        }
    }
    return result;
}
