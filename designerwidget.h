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
#include <QPalette>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <wobjectdefs.h>
#include "badgeitem.h"
#include "badgegraphicitem.h"
#include "guideitem.h"
#include "lightingeffect.h"
#include "imageprocessor.h"

class DesignerWidget : public QGraphicsView {
    W_OBJECT(DesignerWidget)
public:
    explicit DesignerWidget(QWidget* parent = nullptr);
    
    void addBadge(const BadgeItem& item);
    void setBadgeItems(const QList<BadgeItem>& items, const QList<int>& selectedIndices = {});
    void clearBadges();
    void removeSelectedBadges();
    void refreshAll();
    QList<BadgeItem> badgeItems() const;
    void syncToBadgeList(QList<BadgeItem>& badges);
    QList<BadgeGraphicItem*> graphicItems() const { return m_graphicItems; }
    QList<BadgeGraphicItem*> selectedGraphics() const;
    BadgeGraphicItem* selectedGraphic() const;
    
    // Guides
    void updateGuides(double badgeSizeMm);
    void setGuidesVisible(bool b);
    void setBleedVisible(bool b);
    void setVisibleVisible(bool b);
    void setGridVisible(bool b);
    void setSnapToGrid(bool b);
    void setGridSpacingMm(double mm);
    bool gridVisible() const { return m_gridVisible; }
    bool snapToGrid() const { return m_snapToGrid; }
    
    // Lighting
    void setLightingEnabled(bool on);
    void setLightAngle(int degrees);
    void setLightIntensity(double val);
    
    // Glitter
    void setGlitterEnabled(bool on);
    void setGlitterPattern(int pattern); // 0=full, 1=star, 2=snow
    void regenerateGlitter();

    void applyThemePalette(const QPalette& palette);
    void updateBackground(const QBrush& brush);
    void setBatchMode(bool on);

Q_SIGNALS:
    void badgeSelected(BadgeGraphicItem* item) W_SIGNAL(badgeSelected, item);
    void badgeDeselected() W_SIGNAL(badgeDeselected);
    void badgeDoubleClicked(BadgeGraphicItem* item) W_SIGNAL(badgeDoubleClicked, item);
    void badgeMoved(BadgeGraphicItem* item) W_SIGNAL(badgeMoved, item);
    void badgeEditStarted(BadgeGraphicItem* item) W_SIGNAL(badgeEditStarted, item);
    void badgeEditFinished(BadgeGraphicItem* item) W_SIGNAL(badgeEditFinished, item);
    void selectionChanged() W_SIGNAL(selectionChanged);
    void nudgeRequested(double dxMm, double dyMm) W_SIGNAL(nudgeRequested, dxMm, dyMm);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
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
    double m_lightIntensity = 0.35;
    bool m_lightingEnabled = false;
    bool m_glitterEnabled = false;
    bool m_gridVisible = true;
    bool m_snapToGrid = true;
    double m_gridSpacingMm = 5.0;
    QList<BadgeGraphicItem*> m_graphicItems;
    double m_zoomLevel = 1.0;
    double m_badgeSizeMm = 65.0;
    bool m_batchMode = false;
    bool m_panning = false;
    QPoint m_panLastPos;
    bool m_guideUpdatePending = false;
    
    void createGlitter(int pattern);
    QPainterPath createStar(double size);
    QPainterPath createSnowflake(double size);
    QPointF guideSceneCenter() const;
    void positionGuideOverlays();
};

#endif
