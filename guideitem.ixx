module;

#include <QGraphicsEllipseItem>
#include <QColor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QPen>
#include <QBrush>
#include <QVector>

export module guideitem;

export class GuideItem : public QGraphicsEllipseItem {
public:
    GuideItem(QGraphicsItem* parent = nullptr);
    void update(double radiusPx, bool visible);
    void setStyle(const QColor& color, double thickness, const QVector<qreal>& dash = {});

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
