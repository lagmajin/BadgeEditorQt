module;

#include <algorithm>
#include <vector>
#include "layoutengine.h"

export module badge.layout;

export import badge.paper;
export import badge.model;

namespace badge_layout_detail {
constexpr double kCirclePackFactor = 0.8660254037844386; // sqrt(3) / 2
constexpr double kCircleBleedMm = 3.0;

double footprintWidth(const badge::BadgeData& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) + kCircleBleedMm
                              : badge.widthMm;
}

double footprintHeight(const badge::BadgeData& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) + kCircleBleedMm
                              : badge.heightMm;
}
}

export namespace badge {

export std::vector<BadgeData> auto_layout(const std::vector<BadgeData>& templates, const PaperConfig& config) {
    std::vector<BadgeData> result;
    result.reserve(templates.size());

    double curX = config.marginMm;
    double curY = config.marginMm;
    double rowHeight = 0.0;
    bool shift = false;

    for (const auto& tpl : templates) {
        const double tw = badge_layout_detail::footprintWidth(tpl);
        const double th = badge_layout_detail::footprintHeight(tpl);
        const bool circle = tpl.clipToCircle;
        const double pitch = tw + config.spacingMm;
        const double bw = pitch;
        const double bh = circle ? pitch * badge_layout_detail::kCirclePackFactor : th + config.spacingMm;
        double xOff = shift ? pitch / 2.0 : 0.0;

        if (curX + bw + xOff > config.widthMm - config.marginMm) {
            curX = config.marginMm;
            curY += rowHeight;
            rowHeight = 0.0;
            shift = !shift;
            xOff = shift ? pitch / 2.0 : 0.0;
        }
        if (curY + bh > config.heightMm - config.marginMm) {
            break;
        }

        BadgeData item = tpl;
        item.xMm = curX + xOff;
        item.yMm = curY;
        result.push_back(std::move(item));
        rowHeight = std::max(rowHeight, bh);
        curX += bw;
    }

    return result;
}

export std::vector<BadgeData> fill_page(const BadgeData& template_, const PaperConfig& config) {
    std::vector<BadgeData> result;
    const double tw = badge_layout_detail::footprintWidth(template_);
    const double th = badge_layout_detail::footprintHeight(template_);
    const bool circle = template_.clipToCircle;
    const double pitch = tw + config.spacingMm;
    const double bw = pitch;
    const double vStep = circle ? pitch * badge_layout_detail::kCirclePackFactor : th + config.spacingMm;

    for (double y = config.marginMm; y + th < config.heightMm - config.marginMm; y += vStep) {
        const bool shift = (static_cast<int>((y - config.marginMm) / vStep) % 2) == 1;
        for (double x = config.marginMm + (shift ? pitch / 2.0 : 0.0);
             x + tw < config.widthMm - config.marginMm;
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
