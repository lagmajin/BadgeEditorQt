#ifndef GUIDEITEM_H
#define GUIDEITEM_H

#include <QBrush>
#include <QColor>
#include <QGraphicsEllipseItem>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>
#include <QVector>
#include <QWidget>

class GuideItem : public QGraphicsEllipseItem {
public:
    GuideItem(QGraphicsItem* parent = nullptr);
    void update(double radiusPx, bool visible);
    void setStyle(const QColor& color, double thickness, const QVector<qreal>& dash = {});

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

#endif
