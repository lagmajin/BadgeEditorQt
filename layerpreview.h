#ifndef LAYERPREVIEW_H
#define LAYERPREVIEW_H

#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include "badgeitem.h"

QPixmap applyLayerFillColor(QPixmap pixmap, const QColor& fillColor);
QPixmap correctedPixmapForBadge(const BadgeItem& badge, const QString& path);
QPixmap renderedLayerPixmap(const BadgeItem& badge, const LayerItem& layer);
QPainter::CompositionMode compositionModeForLayer(LayerBlendMode mode);
QString layerBlendModeText(LayerBlendMode mode);
QString layerItemSummary(const LayerItem& layer);
QRectF fitRectInside(const QRectF& target, const QSizeF& sourceSize);
QRectF badgeContentRectPx(const BadgeItem& badge);
QRectF badgePrimaryImageRectPx(const BadgeItem& badge, const QSizeF& sourceSize);

#endif
