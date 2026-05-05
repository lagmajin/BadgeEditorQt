#ifndef BADGEGRAPHICITEM_H
#define BADGEGRAPHICITEM_H

#include <QGraphicsItem>
#include <QPixmap>
#include <QPainter>
#include <wobjectdefs.h>
#include "badgeitem.h"

class BadgeGraphicItem : public QGraphicsObject {
    W_OBJECT(BadgeGraphicItem)
public:
    explicit BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent = nullptr);
    
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    BadgeItem& badge() { return m_badge; }
    const BadgeItem& badge() const { return m_badge; }
    QString colorSpaceLabel() const { return m_colorSpaceLabel; }
    void syncFromBadge();
    void applyColorCorrection();
    void updateHandles();

Q_SIGNALS:
    void badgeClicked(BadgeGraphicItem* item) W_SIGNAL(badgeClicked, item);
    void badgeDoubleClicked(BadgeGraphicItem* item) W_SIGNAL(badgeDoubleClicked, item);
    void badgeMoved(BadgeGraphicItem* item) W_SIGNAL(badgeMoved, item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    BadgeItem m_badge;
    QPixmap m_thumbnail;
    QPixmap m_processed;
    QString m_loadedImagePath;
    QString m_colorSpaceLabel;
    QList<QGraphicsRectItem*> m_handles;
    void loadImage();
    void renderCore(QPainter* painter, const QRectF& rect);
};

#endif
