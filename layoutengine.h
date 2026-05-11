#ifndef LAYOUTENGINE_H
#define LAYOUTENGINE_H

import badge.paper;

#include <QList>
#include "badgeitem.h"

class LayoutEngine {
public:
    static QList<BadgeItem> autoLayout(const QList<BadgeItem>& templates, const PaperConfig& config);
    static QList<BadgeItem> packMixed(const QList<BadgeItem>& templates, const PaperConfig& config);
    static QList<QList<BadgeItem>> packMixedPages(const QList<BadgeItem>& templates, const PaperConfig& config);
    static QList<BadgeItem> fillPage(const BadgeItem& template_, const PaperConfig& config);
    static QList<BadgeItem> autoLayoutGrid(const QList<BadgeItem>& templates, const PaperConfig& config);
    static QList<BadgeItem> fillPageGrid(const BadgeItem& template_, const PaperConfig& config);
};

#endif
