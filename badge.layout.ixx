module;

#include "badge.paper.h"
#include "badge.model.h"

#include <algorithm>
#include <utility>
#include <vector>

export module badge.layout;


namespace badge_layout_detail {
constexpr double kCirclePackFactor = 0.8660254037844386; // sqrt(3) / 2

double bleedMm(const badge::BadgeData& badge) {
    return std::max(0.0, badge.guide.bleedMm);
}

double contentWidth(const badge::BadgeData& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) : badge.widthMm;
}

double contentHeight(const badge::BadgeData& badge) {
    return badge.clipToCircle ? std::max(badge.widthMm, badge.heightMm) : badge.heightMm;
}

double footprintWidth(const badge::BadgeData& badge) {
    return contentWidth(badge) + bleedMm(badge) * 2.0;
}

double footprintHeight(const badge::BadgeData& badge) {
    return contentHeight(badge) + bleedMm(badge) * 2.0;
}

bool isCircleLike(const badge::BadgeData& badge) {
    return badge.clipToCircle || badge.guide.shape == badge::GuideShape::Circle;
}

std::pair<double, double> contentOffset(const badge::BadgeData& badge) {
    const double bleed = bleedMm(badge);
    return {bleed, bleed};
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
        const bool circle = badge_layout_detail::isCircleLike(tpl);
        const double pitch = tw + config.spacingMm;
        const double bw = pitch;
        const double bh = circle ? pitch * badge_layout_detail::kCirclePackFactor : th + config.spacingMm;
        const auto [ox, oy] = badge_layout_detail::contentOffset(tpl);
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
        item.xMm = curX + xOff + ox;
        item.yMm = curY + oy;
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
    const bool circle = badge_layout_detail::isCircleLike(template_);
    const double pitch = tw + config.spacingMm;
    const double bw = pitch;
    const double vStep = circle ? pitch * badge_layout_detail::kCirclePackFactor : th + config.spacingMm;
    const auto [ox, oy] = badge_layout_detail::contentOffset(template_);

    for (double y = config.marginMm; y + th < config.heightMm - config.marginMm; y += vStep) {
        const bool shift = (static_cast<int>((y - config.marginMm) / vStep) % 2) == 1;
        for (double x = config.marginMm + (shift ? pitch / 2.0 : 0.0);
             x + tw < config.widthMm - config.marginMm;
             x += bw) {
            BadgeData item = template_;
            item.xMm = x + ox;
            item.yMm = y + oy;
            result.push_back(std::move(item));
        }
    }

    return result;
}

}
