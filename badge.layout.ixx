module;

#include <algorithm>
#include <vector>
#include "layoutengine.h"

export module badge.layout;

export import badge.paper;
export import badge.model;

export namespace badge {

export std::vector<BadgeData> auto_layout(const std::vector<BadgeData>& templates, const PaperConfig& config) {
    std::vector<BadgeData> result;
    result.reserve(templates.size());

    double curX = config.marginMm;
    double curY = config.marginMm;
    double rowHeight = 0.0;
    bool shift = false;

    for (const auto& tpl : templates) {
        const double bw = tpl.widthMm + config.spacingMm;
        const double bh = tpl.heightMm + config.spacingMm;
        double xOff = shift ? tpl.widthMm / 2.0 : 0.0;

        if (curX + bw + xOff > config.widthMm - config.marginMm) {
            curX = config.marginMm;
            curY += rowHeight;
            rowHeight = 0.0;
            shift = !shift;
            xOff = shift ? tpl.widthMm / 2.0 : 0.0;
        }
        if (curY + bh > config.heightMm - config.marginMm) {
            break;
        }

        BadgeData item = tpl;
        item.xMm = curX + xOff;
        item.yMm = curY;
        result.push_back(std::move(item));
        rowHeight = std::max(rowHeight, tpl.heightMm * 0.88);
        curX += bw;
    }

    return result;
}

export std::vector<BadgeData> fill_page(const BadgeData& template_, const PaperConfig& config) {
    std::vector<BadgeData> result;
    const double bw = template_.widthMm + config.spacingMm;
    const double vStep = template_.heightMm * 0.88;

    for (double y = config.marginMm; y + template_.heightMm < config.heightMm - config.marginMm; y += vStep) {
        const bool shift = (static_cast<int>((y - config.marginMm) / vStep) % 2) == 1;
        for (double x = config.marginMm + (shift ? bw / 2.0 : 0.0);
             x + template_.widthMm < config.widthMm - config.marginMm;
             x += bw) {
            BadgeData item = template_;
            item.xMm = x;
            item.yMm = y;
            result.push_back(std::move(item));
        }
    }

    return result;
}

}
