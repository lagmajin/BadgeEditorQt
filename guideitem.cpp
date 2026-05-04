#include "guideitem.h"
#include <QPainter>

GuideItem::GuideItem(QGraphicsItem* parent) : QGraphicsEllipseItem(parent) {
    setFlag(ItemIgnoresTransformations, true);
    setFlag(ItemIsSelectable, false);
    setZValue(100);
}

void GuideItem::update(double radiusPx, bool visible) {
    setVisible(visible);
    if (!visible) return;
    QRectF r(-radiusPx, -radiusPx, radiusPx * 2, radiusPx * 2);
    setRect(r);
    setPos(0, 0);
}

void GuideItem::setStyle(const QColor& color, double thickness, const QVector<qreal>& dash) {
    QPen pen(color, thickness);
    if (!dash.isEmpty()) pen.setDashPattern(dash);
    setPen(pen);
    setBrush(Qt::NoBrush);
}

void GuideItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawEllipse(rect());
}
