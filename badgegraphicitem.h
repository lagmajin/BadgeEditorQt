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
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    BadgeItem& badge() { return m_badge; }
    const BadgeItem& badge() const { return m_badge; }
    QString colorSpaceLabel() const { return m_colorSpaceLabel; }
    void syncFromBadge();
    void applyColorCorrection();
    void updateHandles();
    void beginGeometryUpdate();
    void setGridVisible(bool on) { m_gridVisible = on; }
    void setSnapToGrid(bool on) { m_snapToGrid = on; }
    void setGridSpacingMm(double mm) { m_gridSpacingMm = mm; }
    void beginInteractiveEdit();
    void endInteractiveEdit();

Q_SIGNALS:
    void badgeClicked(BadgeGraphicItem* item) W_SIGNAL(badgeClicked, item);
    void badgeDoubleClicked(BadgeGraphicItem* item) W_SIGNAL(badgeDoubleClicked, item);
    void badgeMoved(BadgeGraphicItem* item) W_SIGNAL(badgeMoved, item);
    void badgeEditStarted(BadgeGraphicItem* item) W_SIGNAL(badgeEditStarted, item);
    void badgeEditFinished(BadgeGraphicItem* item) W_SIGNAL(badgeEditFinished, item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    BadgeItem m_badge;
    QPixmap m_thumbnail;
    QPixmap m_processed;
    QString m_loadedImagePath;
    QString m_colorSpaceLabel;
    QList<QGraphicsRectItem*> m_handles;
    bool m_gridVisible = true;
    bool m_snapToGrid = true;
    double m_gridSpacingMm = 5.0;
    bool m_dragging = false;
    bool m_interactionActive = false;
    QRectF contentRectPx() const;
    QRectF imageRectPx() const;
    QRectF visualRectPx() const;
    bool shouldClipDesignerPreview() const;
    void loadImage();
    void renderCore(QPainter* painter, const QRectF& rect);
};

#endif
