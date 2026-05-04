#ifndef DESIGNERWIDGET_H
#define DESIGNERWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QList>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include "badgeitem.h"
#include "badgegraphicitem.h"
#include "guideitem.h"
#include "lightingeffect.h"
#include "imageprocessor.h"

class DesignerWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit DesignerWidget(QWidget* parent = nullptr);
    
    void addBadge(const BadgeItem& item);
    void removeSelectedBadges();
    void refreshAll();
    void syncToBadgeList(QList<BadgeItem>& badges);
    QList<BadgeGraphicItem*> graphicItems() const { return m_graphicItems; }
    BadgeGraphicItem* selectedGraphic() const;
    
    // Guides
    void updateGuides(double badgeSizeMm);
    void setGuidesVisible(bool b);
    void setBleedVisible(bool b);
    void setVisibleVisible(bool b);
    
    // Lighting
    void setLightingEnabled(bool on);
    void setLightAngle(int degrees);
    void setLightIntensity(double val);
    
    // Glitter
    void setGlitterEnabled(bool on);
    void setGlitterPattern(int pattern); // 0=full, 1=star, 2=snow
    void regenerateGlitter();

    void updateBackground(const QBrush& brush);
    void setBatchMode(bool on);

signals:
    void badgeSelected(BadgeGraphicItem* item);
    void badgeDeselected();
    void badgeDoubleClicked(BadgeGraphicItem* item);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    QGraphicsScene* m_scene;
    GuideItem* m_guideBleed;
    GuideItem* m_guideVisible;
    QGraphicsItem* m_glitterGroup = nullptr;
    QGraphicsItem* m_lightingOverlay = nullptr;
    int m_glitterPattern = 0;
    double m_lightAngle = 0;
    double m_lightIntensity = 0.5;
    bool m_lightingEnabled = false;
    QList<BadgeGraphicItem*> m_graphicItems;
    double m_zoomLevel = 1.0;
    double m_badgeSizeMm = 57.0;
    QPointF m_guideSceneCenter{0, 0};
    bool m_batchMode = false;
    
    void createGlitter(int pattern);
    QPainterPath createStar(double size);
    QPainterPath createSnowflake(double size);
    void positionGuideOverlays();
};

#endif
