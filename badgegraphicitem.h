#ifndef BADGEGRAPHICITEM_H
#define BADGEGRAPHICITEM_H

#include <QGraphicsItem>
#include <QPixmap>
#include <QPainter>
#include "badgeitem.h"

class LightingEffect;

class BadgeGraphicItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent = nullptr);
    
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    BadgeItem& badge() { return m_badge; }
    const BadgeItem& badge() const { return m_badge; }
    void syncFromBadge();
    void applyColorCorrection();
    void updateHandles();
    void setLightingEnabled(bool on);
    void setLightAngle(int degrees);
    void setLightIntensity(double val);

signals:
    void badgeClicked(BadgeGraphicItem* item);
    void badgeDoubleClicked(BadgeGraphicItem* item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    BadgeItem m_badge;
    QPixmap m_thumbnail;
    QPixmap m_processed;
    QList<QGraphicsRectItem*> m_handles;
    LightingEffect* m_lighting = nullptr;
    void loadImage();
    void renderCore(QPainter* painter, const QRectF& rect);
    void createHandles();
};

#endif
