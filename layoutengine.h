#ifndef LAYOUTENGINE_H
#define LAYOUTENGINE_H

#include <QList>
#include "badgeitem.h"

struct PaperConfig {
    double widthMm = 210.0;
    double heightMm = 297.0;
    double marginMm = 10.0;
    double spacingMm = 1.0;
};

class LayoutEngine {
public:
    static QList<BadgeItem> autoLayout(const QList<BadgeItem>& templates, const PaperConfig& config);
    static QList<BadgeItem> fillPage(const BadgeItem& template_, const PaperConfig& config);
};

#endif
