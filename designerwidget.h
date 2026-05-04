#ifndef DESIGNERWIDGET_H
#define DESIGNERWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QList>
#include "badgeitem.h"
#include "badgegraphicitem.h"
#include "guideitem.h"
#include "lightingeffect.h"

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
    LightingEffect* lightingEffect() { return m_lighting; }
    void setLightingEnabled(bool on);
    void setLightAngle(int degrees);
    void setLightIntensity(double val);
    
    // Glitter
    void setGlitterEnabled(bool on);
    void setGlitterPattern(int pattern); // 0=full, 1=star, 2=snow
    void regenerateGlitter();

    void updateBackground(const QBrush& brush);

signals:
    void badgeSelected(BadgeGraphicItem* item);
    void badgeDeselected();
    void badgeDoubleClicked(BadgeGraphicItem* item);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene* m_scene;
    GuideItem* m_guideBleed;
    GuideItem* m_guideVisible;
    LightingEffect* m_lighting;
    QGraphicsItem* m_glitterGroup = nullptr;
    QList<BadgeGraphicItem*> m_graphicItems;
    double m_zoomLevel = 1.0;
    double m_badgeSizeMm = 32.0;
    
    void createGlitter(int pattern);
    QPainterPath createStar(double size);
    QPainterPath createSnowflake(double size);
};

#endif
