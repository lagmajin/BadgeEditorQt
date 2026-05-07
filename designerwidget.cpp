#include "designerwidget.h"
#include <QGraphicsSceneMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QImageReader>
#include <QSet>
#include <QKeySequence>
#include <QApplication>
#include <QFontMetricsF>
#include <QScrollBar>
#include <QTimer>
#include <QPalette>
#include <algorithm>
#include <cmath>
#include <utility>
#include <wobjectimpl.h>
#include "viewportbackend.h"

static QColor blend(const QColor& a, const QColor& b, qreal ratio) {
    const qreal clamped = std::clamp(ratio, 0.0, 1.0);
    return QColor::fromRgbF(
        a.redF()   * (1.0 - clamped) + b.redF()   * clamped,
        a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
        a.blueF()  * (1.0 - clamped) + b.blueF()  * clamped,
        a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped
    );
}

class LightingOverlayItem : public QGraphicsItem {
public:
    LightingOverlayItem(QGraphicsItem* parent = nullptr) : QGraphicsItem(parent) {
        setFlag(ItemIgnoresTransformations, true);
    }

    QRectF boundingRect() const override {
        return QRectF(-m_boundsRadius, -m_boundsRadius, m_boundsRadius * 2.0, m_boundsRadius * 2.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (!m_enabled) {
            return;
        }

        const QRectF r(-m_radius, -m_radius, m_radius * 2.0, m_radius * 2.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient shade(r.center(), r.width() / 2.0);
        const double rad = m_angle * M_PI / 180.0;
        const QPointF lightCenter = r.center() + QPointF(std::cos(rad) * r.width() * 0.22,
                                                         -std::sin(rad) * r.height() * 0.22);
        shade.setCenter(lightCenter);
        shade.setFocalPoint(lightCenter);
        shade.setColorAt(0.0, QColor(255, 255, 255, 0));
        shade.setColorAt(0.42, QColor(255, 255, 255, int(14 * m_intensity)));
        shade.setColorAt(0.68, QColor(0, 0, 0, int(28 * m_intensity)));
        shade.setColorAt(1.0, QColor(0, 0, 0, int(88 * m_intensity)));
        painter->setBrush(shade);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(r);

        QRadialGradient spec(QPointF(r.left() + r.width() * 0.24, r.top() + r.height() * 0.16),
                             r.width() * 0.14);
        spec.setColorAt(0.0, QColor(255, 255, 255, int(120 * m_intensity)));
        spec.setColorAt(0.5, QColor(255, 255, 255, int(45 * m_intensity)));
        spec.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter->setBrush(spec);
        const double specR = r.width() * 0.11;
        painter->drawEllipse(QPointF(r.left() + r.width() * 0.24, r.top() + r.height() * 0.16),
                             specR, specR * 0.72);
        painter->restore();
    }

    void setEnabled(bool on) { m_enabled = on; update(); }
    void setAngle(double a) { m_angle = a; update(); }
    void setIntensity(double v) { m_intensity = v; update(); }
    void setRadius(double r) { m_radius = r; update(); }

private:
    bool m_enabled = false;
    double m_angle = 0.0;
    double m_intensity = 0.35;
    double m_radius = 100.0;
    double m_boundsRadius = 1000.0;
};

static bool isImageFile(const QString& path) {
    if (!QFileInfo::exists(path)) return false;
    QImageReader reader(path);
    if (!reader.canRead()) {
        // フォールバック: 拡張子チェック
        static const QSet<QString> formats = []{
            QSet<QString> s;
            for (const auto& fmt : QImageReader::supportedImageFormats())
                s.insert(QString::fromUtf8(fmt).toLower());
            return s;
        }();
        QString ext = QFileInfo(path).suffix().toLower();
        return !ext.isEmpty() && formats.contains(ext);
    }
    return true;
}

DesignerWidget::DesignerWidget(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(-500, -500, 1000, 1000, this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    viewportbackend::applySceneViewportProfile(this, viewportbackend::experimentalGpuViewportEnabled());
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(Qt::NoBrush);
    scale(1.25, 1.25);
    setFocusPolicy(Qt::StrongFocus);

    // Guide circles (drawn in drawForeground, not as scene items)
    m_guideBleed = new GuideItem;
    m_guideVisible = new GuideItem;
    m_lightingOverlay = new LightingOverlayItem;
    m_scene->addItem(m_lightingOverlay);
    m_lightingOverlay->setZValue(9998);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        emit selectionChanged();
    });

}

W_OBJECT_IMPL(DesignerWidget)

void DesignerWidget::applyThemePalette(const QPalette& palette) {
    setPalette(palette);
    viewport()->setPalette(palette);
    setBackgroundBrush(palette.brush(QPalette::Window));
    m_scene->setBackgroundBrush(palette.brush(QPalette::Window));
    viewport()->setAutoFillBackground(true);
    viewport()->update();
}

void DesignerWidget::addBadge(const BadgeItem& item) {
    auto* gi = new BadgeGraphicItem(item);
    gi->setGridVisible(m_gridVisible);
    gi->setSnapToGrid(m_snapToGrid);
    gi->setGridSpacingMm(m_gridSpacingMm);
    m_scene->addItem(gi);
    gi->syncFromBadge();
    m_graphicItems.append(gi);
    connect(gi, &BadgeGraphicItem::badgeClicked, this, &DesignerWidget::badgeSelected);
    connect(gi, &BadgeGraphicItem::badgeDoubleClicked, this, &DesignerWidget::badgeDoubleClicked);
    connect(gi, &BadgeGraphicItem::badgeMoved, this, &DesignerWidget::badgeMoved);
    connect(gi, &BadgeGraphicItem::badgeEditStarted, this, [this](BadgeGraphicItem* item) {
        if (m_interactiveEditDepth++ == 0) {
            setInteractiveViewportMode(true);
        }
        emit badgeEditStarted(item);
    });
    connect(gi, &BadgeGraphicItem::badgeEditFinished, this, [this](BadgeGraphicItem* item) {
        if (m_interactiveEditDepth > 0 && --m_interactiveEditDepth == 0) {
            setInteractiveViewportMode(false);
        }
        emit badgeEditFinished(item);
    });
}

void DesignerWidget::setBadgeItems(const QList<BadgeItem>& items, const QList<int>& selectedIndices) {
    clearBadges();
    for (const auto& item : items) {
        addBadge(item);
    }

    QSet<int> selectedSet;
    for (int index : selectedIndices) {
        selectedSet.insert(index);
    }
    for (int i = 0; i < m_graphicItems.size(); ++i) {
        auto* gi = m_graphicItems[i];
        const bool selected = selectedSet.contains(i);
        gi->badge().isSelected = selected;
        gi->setSelected(selected);
        gi->updateHandles();
        gi->update();
    }

    emit selectionChanged();
}

void DesignerWidget::clearBadges() {
    for (auto* gi : std::as_const(m_graphicItems)) {
        m_scene->removeItem(gi);
        delete gi;
    }
    m_graphicItems.clear();
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
    for (auto* gi : m_graphicItems) {
        gi->setGridVisible(m_gridVisible);
        gi->setSnapToGrid(m_snapToGrid);
        gi->setGridSpacingMm(m_gridSpacingMm);
        gi->syncFromBadge();
    }
}

QList<BadgeItem> DesignerWidget::badgeItems() const {
    QList<BadgeItem> items;
    items.reserve(m_graphicItems.size());
    for (auto* gi : m_graphicItems) {
        items.append(gi->badge());
    }
    return items;
}

QList<BadgeGraphicItem*> DesignerWidget::selectedGraphics() const {
    QList<BadgeGraphicItem*> items;
    const auto selectedItems = m_scene->selectedItems();
    items.reserve(selectedItems.size());
    for (auto* item : selectedItems) {
        if (auto* gi = dynamic_cast<BadgeGraphicItem*>(item)) {
            items.append(gi);
        }
    }
    return items;
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

void DesignerWidget::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::SelectAll)) {
        for (auto* gi : m_graphicItems) {
            gi->setSelected(true);
            gi->badge().isSelected = true;
            gi->updateHandles();
        }
        emit selectionChanged();
        event->accept();
        return;
    }

    const bool hasSelection = !selectedGraphics().isEmpty();
    if (hasSelection) {
        const bool fast = (event->modifiers() & Qt::ShiftModifier);
        const double step = fast ? 1.0 : 0.1;
        double dx = 0.0;
        double dy = 0.0;
        switch (event->key()) {
        case Qt::Key_Left: dx = -step; break;
        case Qt::Key_Right: dx = step; break;
        case Qt::Key_Up: dy = -step; break;
        case Qt::Key_Down: dy = step; break;
        default: break;
        }
        if (dx != 0.0 || dy != 0.0) {
            emit nudgeRequested(dx, dy);
            event->accept();
            return;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void DesignerWidget::updateGuides(double badgeSizeMm) {
    m_badgeSizeMm = badgeSizeMm;
    if (m_guideUpdatePending) {
        return;
    }
    m_guideUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_guideUpdatePending = false;
        positionGuideOverlays();
        if (m_glitterGroup) {
            regenerateGlitter();
        }
        viewport()->update();
    });
}

void DesignerWidget::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    positionGuideOverlays();
    viewport()->update();
}

void DesignerWidget::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    positionGuideOverlays();
    viewport()->update();
}

QPointF DesignerWidget::guideSceneCenter() const {
    return m_scene->sceneRect().center();
}

void DesignerWidget::positionGuideOverlays() {
    const QPointF center = guideSceneCenter();
    if (m_glitterGroup) m_glitterGroup->setPos(center);
    if (m_lightingOverlay) {
        const double mmToPx = 96.0 / 25.4;
        auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
        if (lo) {
            lo->setRadius((m_badgeSizeMm * mmToPx) / 2.0);
        }
        m_lightingOverlay->setPos(center);
        m_lightingOverlay->setVisible(m_lightingEnabled);
    }
}

void DesignerWidget::setGuidesVisible(bool b) {
    m_guideBleed->setVisible(b);
    m_guideVisible->setVisible(b);
}

void DesignerWidget::setBleedVisible(bool b) { m_guideBleed->setVisible(b); }
void DesignerWidget::setVisibleVisible(bool b) { m_guideVisible->setVisible(b); }
void DesignerWidget::setGridVisible(bool b) {
    m_gridVisible = b;
    viewport()->update();
}

void DesignerWidget::setSnapToGrid(bool b) {
    m_snapToGrid = b;
    for (auto* gi : m_graphicItems) {
        gi->setSnapToGrid(b);
    }
}

void DesignerWidget::setGridSpacingMm(double mm) {
    m_gridSpacingMm = mm;
    for (auto* gi : m_graphicItems) {
        gi->setGridSpacingMm(mm);
    }
    viewport()->update();
}

void DesignerWidget::setLightingEnabled(bool on) {
    m_lightingEnabled = on;
    if (m_lightingOverlay) {
        m_lightingOverlay->setVisible(on);
    }
    if (m_interactiveEditDepth == 0) {
        setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    }
    viewport()->update();
}
void DesignerWidget::setLightAngle(int degrees) {
    m_lightAngle = degrees;
    auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
    if (lo) {
        lo->setAngle(degrees);
    }
}
void DesignerWidget::setLightIntensity(double val) {
    m_lightIntensity = std::clamp(val, 0.0, 0.6);
    auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
    if (lo) {
        lo->setIntensity(m_lightIntensity);
    }
}

void DesignerWidget::setGlitterEnabled(bool on) {
    m_glitterEnabled = on;
    if (!on) {
        if (m_glitterGroup) { m_scene->removeItem(m_glitterGroup); delete m_glitterGroup; m_glitterGroup = nullptr; }
        viewport()->update();
        return;
    }
    regenerateGlitter();
}

void DesignerWidget::setGlitterPattern(int pattern) {
    m_glitterPattern = pattern;
    if (m_glitterEnabled) regenerateGlitter();
}

void DesignerWidget::regenerateGlitter() {
    if (!m_glitterEnabled) {
        return;
    }
    if (m_glitterGroup) { m_scene->destroyItemGroup(qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup)); m_glitterGroup = nullptr; }
    m_glitterGroup = m_scene->createItemGroup({});
    auto* group = qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup);
    group->setPos(guideSceneCenter());
    createGlitter(m_glitterPattern);
}

void DesignerWidget::createGlitter(int pattern) {
    if (!m_glitterEnabled) {
        return;
    }
    if (!m_glitterGroup) m_glitterGroup = m_scene->createItemGroup({});
    auto* group = qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup);
    const double mmToPx = 96.0 / 25.4;
    double visibleR = std::max(0.0, (m_badgeSizeMm - 4)) * mmToPx / 2.0;
    auto& rng = *QRandomGenerator::global();
    const int count = pattern == 0 ? 72 : (pattern == 1 ? 22 : 16);
    auto addSpark = [&](QGraphicsItem* dot, double x, double y, double baseSize, double alpha, const QColor& color) {
        if (!group || !dot) {
            return;
        }
        auto* halo = new QGraphicsEllipseItem(-baseSize, -baseSize, baseSize * 2.0, baseSize * 2.0);
        halo->setBrush(QColor(color.red(), color.green(), color.blue(), int(alpha * 40)));
        halo->setPen(Qt::NoPen);
        halo->setPos(x, y);
        halo->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        group->addToGroup(halo);
        dot->setPos(x, y);
        dot->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        group->addToGroup(dot);
    };

    for (int i = 0; i < count; ++i) {
        const double angleBias = m_lightAngle * M_PI / 180.0;
        double angle = (pattern == 1 || pattern == 2)
                           ? i * 2.0 * M_PI / count + rng.generateDouble() * 0.35 + angleBias * 0.05
                           : rng.generateDouble() * 2.0 * M_PI;
        double dist = std::pow(rng.generateDouble(), 0.55) * visibleR * 0.95;
        double x = cos(angle) * dist;
        double y = sin(angle) * dist;
        double size = 1.4 + rng.generateDouble() * 5.0;
        double opacity = 0.18 + rng.generateDouble() * 0.65;

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
        addSpark(dot, x, y, size * 0.7, opacity, pattern == 2 ? QColor(200, 220, 255) : QColor(255, 255, 255));
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
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panLastPos = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !itemAt(event->pos()) && !isNearSelectedItem(event->pos())) {
        m_scene->clearSelection();
        emit badgeDeselected();
    }
    QGraphicsView::mousePressEvent(event);
}

void DesignerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const QPoint delta = event->pos() - m_panLastPos;
        m_panLastPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void DesignerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void DesignerWidget::drawBackground(QPainter* painter, const QRectF& rect) {
    const int tileSize = 16;
    const QColor windowColor = palette().color(QPalette::Window);
    const QColor baseColor = palette().color(QPalette::Base);
    const QColor c1 = blend(windowColor, baseColor, 0.16);
    const QColor c2 = blend(windowColor, baseColor, 0.26);

    int left = int(rect.left()) - ((int(rect.left()) % tileSize + tileSize) % tileSize);
    int top = int(rect.top()) - ((int(rect.top()) % tileSize + tileSize) % tileSize);

    for (int y = top; y < int(rect.bottom()); y += tileSize) {
        for (int x = left; x < int(rect.right()); x += tileSize) {
            QColor c = ((x / tileSize + y / tileSize) & 1) ? c2 : c1;
            painter->fillRect(QRect(x, y, tileSize, tileSize), c);
        }
    }

    if (!m_gridVisible) {
        return;
    }

    const double mmToPx = 96.0 / 25.4;
    const double stepPx = std::max(1.0, m_gridSpacingMm * mmToPx);
    const double majorStepPx = stepPx * 5.0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    const QColor majorGridBase = palette().color(QPalette::Mid);
    const QColor minorGridBase = blend(palette().color(QPalette::Mid), palette().color(QPalette::Highlight), 0.10);
    const QColor majorGridColor = QColor::fromRgb(majorGridBase.red(), majorGridBase.green(), majorGridBase.blue(), 150);
    const QColor minorGridColor = QColor::fromRgb(minorGridBase.red(), minorGridBase.green(), minorGridBase.blue(), 86);

    const int gridLeft = int(std::floor(rect.left() / stepPx) * stepPx);
    const int gridTop = int(std::floor(rect.top() / stepPx) * stepPx);

    for (double x = gridLeft; x <= rect.right(); x += stepPx) {
        const bool major = std::fmod(std::abs(x), majorStepPx) < 0.5 || std::fmod(std::abs(x), majorStepPx) > majorStepPx - 0.5;
        painter->setPen(QPen(major ? majorGridColor : minorGridColor, major ? 1.0 : 0.5));
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    for (double y = gridTop; y <= rect.bottom(); y += stepPx) {
        const bool major = std::fmod(std::abs(y), majorStepPx) < 0.5 || std::fmod(std::abs(y), majorStepPx) > majorStepPx - 0.5;
        painter->setPen(QPen(major ? majorGridColor : minorGridColor, major ? 1.0 : 0.5));
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    painter->restore();
}

void DesignerWidget::drawForeground(QPainter* painter, const QRectF& rect) {
    Q_UNUSED(rect);
    const double mmToPx = 96.0 / 25.4;
    double bleedPx = (m_badgeSizeMm + 3) * mmToPx;
    double finishPx = m_badgeSizeMm * mmToPx;
    double visiblePx = std::max(0.0, (m_badgeSizeMm - 4)) * mmToPx;

    const QPointF center = guideSceneCenter();
    double cx = center.x(), cy = center.y();

    double rb = bleedPx / 2, rf = finishPx / 2, rv = visiblePx / 2;

    // Bleed line (red dashed)
    if (m_guideBleed->isVisible()) {
        QPen pen(QColor(255, 0, 0), 1.5);
        pen.setDashPattern({4, 2});
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rb, rb);
    }

    // Visible area (green solid)
    if (m_guideVisible->isVisible()) {
        QPen pen(QColor(0, 255, 0), 2.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rv, rv);
    }

    // Small left-top HUD for editor context.
    {
        const QList<BadgeGraphicItem*> selection = selectedGraphics();
        const int selectionCount = selection.size();
        const double zoomPercent = transform().m11() * 100.0;
        const QString hudText = QStringLiteral("Zoom %1% | Snap %2 | Grid %3 mm | Sel %4 | Badge %5 mm")
                                    .arg(zoomPercent, 0, 'f', 0)
                                    .arg(m_snapToGrid ? QStringLiteral("ON") : QStringLiteral("OFF"))
                                    .arg(m_gridSpacingMm, 0, 'f', 1)
                                    .arg(selectionCount)
                                    .arg(m_badgeSizeMm, 0, 'f', 0);

        painter->save();
        QFont hudFont = painter->font();
        hudFont.setPointSizeF(std::max(8.0, hudFont.pointSizeF() - 1.0));
        hudFont.setBold(true);
        painter->setFont(hudFont);
        const QFontMetricsF fm(hudFont);
        const QRectF hudBox = fm.boundingRect(hudText).adjusted(-10.0, -7.0, 10.0, 9.0);
        const QRectF hudRect(rect.left() + 14.0, rect.top() + 14.0, hudBox.width(), hudBox.height());

        QColor hudBg = palette().color(QPalette::Window);
        hudBg.setAlpha(208);
        QColor hudBorder = palette().color(QPalette::Mid);
        hudBorder.setAlpha(180);
        QColor hudTextColor = palette().color(QPalette::WindowText);
        hudTextColor.setAlpha(230);

        painter->setPen(QPen(hudBorder, 1.0));
        painter->setBrush(hudBg);
        painter->drawRoundedRect(hudRect, 8.0, 8.0);
        painter->setPen(hudTextColor);
        painter->drawText(hudRect, Qt::AlignCenter, hudText);
        painter->restore();
    }

    if (m_graphicItems.isEmpty()) {
        painter->save();
        QColor titleColor = palette().color(QPalette::WindowText);
        titleColor.setAlpha(230);
        painter->setPen(titleColor);
        QFont titleFont = painter->font();
        titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        const QRectF titleRect(rect.center().x() - 220.0, rect.center().y() - 36.0, 440.0, 30.0);
        painter->drawText(titleRect, Qt::AlignCenter, QStringLiteral("ここにバッジを追加してください"));

        QFont hintFont = painter->font();
        hintFont.setPointSizeF(std::max(8.0, hintFont.pointSizeF() - 1.0));
        hintFont.setBold(false);
        painter->setFont(hintFont);
        const QRectF hintRect(rect.center().x() - 260.0, rect.center().y() - 4.0, 520.0, 30.0);
        painter->drawText(hintRect,
                          Qt::AlignCenter,
                          QStringLiteral("画像をドラッグ&ドロップするか、ツールバーから追加できます"));
        painter->restore();
    }

    if (!m_graphicItems.isEmpty() && !selectedGraphics().isEmpty()) {
        const auto modifiers = QApplication::keyboardModifiers();
        const bool altDown = modifiers & Qt::AltModifier;
        const bool shiftDown = modifiers & Qt::ShiftModifier;
        painter->save();
        const QColor bg = altDown ? QColor(255, 172, 73, 220) : (shiftDown ? QColor(76, 124, 255, 220) : QColor(54, 58, 66, 220));
        const QColor fg = altDown ? QColor(24, 24, 26, 240) : palette().color(QPalette::WindowText);
        const QString text = altDown
            ? QStringLiteral("Alt + ドラッグ: 非対称リサイズ")
            : (shiftDown
                   ? QStringLiteral("Shift + ドラッグ: 比率固定")
                   : QStringLiteral("Alt: 非対称 / Shift: 比率固定"));
        QFont font = painter->font();
        font.setPointSizeF(std::max(8.0, font.pointSizeF() - 1.0));
        font.setBold(true);
        painter->setFont(font);
        const QFontMetricsF fm(font);
        const QRectF box = fm.boundingRect(text).adjusted(-10.0, -6.0, 10.0, 8.0);
        const QRectF placed(rect.left() + 18.0, rect.bottom() - box.height() - 18.0, box.width(), box.height());
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(placed, 8.0, 8.0);
        painter->setPen(fg);
        painter->drawText(placed, Qt::AlignCenter, text);
        painter->restore();
    }
}

void DesignerWidget::setBatchMode(bool on) { m_batchMode = on; viewport()->update(); }

void DesignerWidget::setExperimentalGpuViewport(bool on) {
    viewportbackend::applySceneViewportProfile(this, on);
}

void DesignerWidget::setInteractiveViewportMode(bool on) {
    setViewportUpdateMode(on ? QGraphicsView::FullViewportUpdate : QGraphicsView::SmartViewportUpdate);
    viewport()->update();
}

bool DesignerWidget::isNearSelectedItem(const QPoint& viewPos) const {
    constexpr int kTolerancePx = 10;
    for (auto* gi : selectedGraphics()) {
        if (!gi) {
            continue;
        }
        const QRectF sceneRect = gi->sceneBoundingRect();
        const QRect viewRect = mapFromScene(sceneRect).boundingRect().adjusted(-kTolerancePx, -kTolerancePx, kTolerancePx, kTolerancePx);
        if (viewRect.contains(viewPos)) {
            return true;
        }
    }
    return false;
}

void DesignerWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto& url : event->mimeData()->urls()) {
            if (isImageFile(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void DesignerWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto& url : event->mimeData()->urls()) {
            if (isImageFile(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void DesignerWidget::dropEvent(QDropEvent* event) {
    for (const auto& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (isImageFile(path)) {
            BadgeItem b;
            b.layers.append(layerFromImagePath(path));
            b.label = QFileInfo(path).baseName();
            addBadge(b);
        }
    }
}
