#include "designerwidget.h"
#include <QGraphicsSceneMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <cmath>

DesignerWidget::DesignerWidget(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(-500, -500, 1000, 1000, this);
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(Qt::lightGray);

    // Guide circles
    m_guideBleed = new GuideItem;
    m_guideBleed->setStyle(QColor(255, 0, 0), 1.5, {4, 2});
    m_scene->addItem(m_guideBleed);

    m_guideVisible = new GuideItem;
    m_guideVisible->setStyle(QColor(0, 255, 0), 2.0, {});
    m_scene->addItem(m_guideVisible);

    // Lighting effect
    m_lighting = new LightingEffect(this);
    m_lighting->setEnabled(false);
    setGraphicsEffect(m_lighting);
}

void DesignerWidget::addBadge(const BadgeItem& item) {
    auto* gi = new BadgeGraphicItem(item);
    m_scene->addItem(gi);
    gi->syncFromBadge();
    m_graphicItems.append(gi);
    connect(gi, &BadgeGraphicItem::badgeClicked, this, &DesignerWidget::badgeSelected);
    connect(gi, &BadgeGraphicItem::badgeDoubleClicked, this, &DesignerWidget::badgeDoubleClicked);
}

void DesignerWidget::removeSelectedBadges() {
    auto items = m_scene->selectedItems();
    for (auto* item : items) {
        if (auto* gi = dynamic_cast<BadgeGraphicItem*>(item)) {
            m_graphicItems.removeOne(gi);
            m_scene->removeItem(gi);
            delete gi;
        }
    }
}

void DesignerWidget::refreshAll() {
    for (auto* gi : m_graphicItems) gi->syncFromBadge();
}

void DesignerWidget::syncToBadgeList(QList<BadgeItem>& badges) {
    for (int i = 0; i < m_graphicItems.size() && i < badges.size(); ++i) {
        m_graphicItems[i]->badge().xMm = badges[i].xMm;
        m_graphicItems[i]->badge().yMm = badges[i].yMm;
        m_graphicItems[i]->syncFromBadge();
    }
    if (m_graphicItems.size() != badges.size())
        emit badgeDeselected();
}

BadgeGraphicItem* DesignerWidget::selectedGraphic() const {
    auto sel = m_scene->selectedItems();
    if (!sel.isEmpty())
        return dynamic_cast<BadgeGraphicItem*>(sel.first());
    return nullptr;
}

void DesignerWidget::updateGuides(double badgeSizeMm) {
    m_badgeSizeMm = badgeSizeMm;
    const double mmToPx = 96.0 / 25.4;
    double finishPx = badgeSizeMm * mmToPx;
    double bleedPx = (badgeSizeMm + 3) * mmToPx;
    double visiblePx = std::max(0.0, (badgeSizeMm - 4)) * mmToPx;
    m_guideBleed->update(bleedPx / 2, m_guideBleed->isVisible());
    m_guideVisible->update(visiblePx / 2, m_guideVisible->isVisible());
}

void DesignerWidget::setGuidesVisible(bool b) {
    m_guideBleed->setVisible(b);
    m_guideVisible->setVisible(b);
}

void DesignerWidget::setBleedVisible(bool b) { m_guideBleed->setVisible(b); }
void DesignerWidget::setVisibleVisible(bool b) { m_guideVisible->setVisible(b); }
void DesignerWidget::setLightingEnabled(bool on) { m_lighting->setEnabled(on); setViewportUpdateMode(on ? QGraphicsView::FullViewportUpdate : QGraphicsView::SmartViewportUpdate); }
void DesignerWidget::setLightAngle(int degrees) { m_lighting->setLightAngle(degrees); }
void DesignerWidget::setLightIntensity(double val) { m_lighting->setIntensity(val); }

void DesignerWidget::setGlitterEnabled(bool on) {
    if (m_glitterGroup && !on) { m_scene->removeItem(m_glitterGroup); delete m_glitterGroup; m_glitterGroup = nullptr; }
    if (on) regenerateGlitter();
}

void DesignerWidget::setGlitterPattern(int pattern) {
    if (m_glitterGroup) regenerateGlitter();
}

void DesignerWidget::regenerateGlitter() {
    if (m_glitterGroup) { m_scene->removeItem(m_glitterGroup); delete m_glitterGroup; }
    m_glitterGroup = m_scene->createItemGroup({});
    // Glitter will be added via createGlitter when pattern changes
    createGlitter(0);
}

void DesignerWidget::createGlitter(int pattern) {
    if (!m_glitterGroup) m_glitterGroup = m_scene->createItemGroup({});
    const double mmToPx = 96.0 / 25.4;
    double visibleR = std::max(0.0, (m_badgeSizeMm - 4)) * mmToPx / 2.0;
    auto& rng = *QRandomGenerator::global();
    const int count = pattern == 0 ? 60 : (pattern == 1 ? 18 : 12);

    for (int i = 0; i < count; ++i) {
        double angle = (pattern == 1 || pattern == 2) ? i * 2.0 * M_PI / count + rng.generateDouble() * 0.3
                                                       : rng.generateDouble() * 2.0 * M_PI;
        double dist = rng.generateDouble() * visibleR * 0.85 + visibleR * 0.1;
        double x = cos(angle) * dist;
        double y = sin(angle) * dist;
        double size = 2 + rng.generateDouble() * 4;
        double opacity = 0.2 + rng.generateDouble() * 0.5;

        QGraphicsItem* dot;
        if (pattern == 1) {
            auto* path = new QGraphicsPathItem(createStar(size));
            path->setBrush(QColor(255, 255, 255, int(opacity * 255)));
            path->setPen(Qt::NoPen);
            dot = path;
        } else if (pattern == 2) {
            auto* path = new QGraphicsPathItem(createSnowflake(size));
            path->setBrush(QColor(200, 220, 255, int(opacity * 255)));
            path->setPen(Qt::NoPen);
            dot = path;
        } else {
            auto* ellipse = new QGraphicsEllipseItem(-size/2, -size/2, size, size);
            ellipse->setBrush(QColor(255, 255, 255, int(opacity * 255)));
            ellipse->setPen(Qt::NoPen);
            dot = ellipse;
        }
        dot->setPos(x, y);
        dot->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        m_glitterGroup->addToGroup(dot);
    }
}

QPainterPath DesignerWidget::createStar(double size) {
    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        double a = i * M_PI / 5.0 - M_PI / 2.0;
        double r = (i % 2 == 0) ? size : size * 0.4;
        QPointF pt(cos(a) * r, sin(a) * r);
        if (i == 0) path.moveTo(pt);
        else path.lineTo(pt);
    }
    path.closeSubpath();
    return path;
}

QPainterPath DesignerWidget::createSnowflake(double size) {
    QPainterPath path;
    for (int arm = 0; arm < 6; ++arm) {
        double a = arm * M_PI / 3.0;
        double cx = cos(a) * size;
        double cy = sin(a) * size;
        path.moveTo(0, 0);
        path.lineTo(cx, cy);
        double bx = cos(a + 0.4) * size * 0.4;
        double by = sin(a + 0.4) * size * 0.4;
        path.moveTo(cx, cy);
        path.lineTo(cx + bx, cy + by);
        double bx2 = cos(a - 0.4) * size * 0.4;
        double by2 = sin(a - 0.4) * size * 0.4;
        path.moveTo(cx, cy);
        path.lineTo(cx + bx2, cy + by2);
    }
    return path;
}

void DesignerWidget::updateBackground(const QBrush& brush) { m_scene->setBackgroundBrush(brush); }

void DesignerWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DesignerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !itemAt(event->pos())) {
        m_scene->clearSelection();
        emit badgeDeselected();
    }
    QGraphicsView::mousePressEvent(event);
}

void DesignerWidget::drawBackground(QPainter* painter, const QRectF& rect) {
    // Alpha checkerboard
    const int tileSize = 16;
    QColor c1(255, 255, 255);
    QColor c2(191, 191, 191);
    painter->fillRect(rect, c1);
    for (int y = int(rect.top() / tileSize) * tileSize; y < rect.bottom(); y += tileSize) {
        for (int x = int(rect.left() / tileSize) * tileSize; x < rect.right(); x += tileSize) {
            if ((x / tileSize + y / tileSize) % 2 == 1)
                painter->fillRect(x, y, tileSize, tileSize, c2);
        }
    }
}
