#include "designerwidget.h"
#include "viewportbackend.h"
#include "constants.h"
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
#include <QSignalBlocker>
#include <QTimer>
#include <QPixmap>
#include <QPalette>
#include <algorithm>
#include <cmath>
#include <utility>
#include <wobjectimpl.h>

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
    scale(1.0, 1.0);
    setFocusPolicy(Qt::StrongFocus);

    // Guide circles (drawn in drawForeground, not as scene items)
    m_guideBleed = new GuideItem;
    m_guideVisible = new GuideItem;
    m_lightingOverlay = new LightingOverlayItem;
    m_scene->addItem(m_lightingOverlay);
    m_lightingOverlay->setZValue(9998);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        m_hasSelection = !m_scene->selectedItems().isEmpty();
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
    // An image-backed item can build its preview cache on its first paint.
    // Invalidate the viewport now so the first subsequent drag cannot retain
    // a stale partial update from before the cache existed.
    viewport()->update();
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

    m_hasSelection = !selectedSet.isEmpty();
    emit selectionChanged();
}

void DesignerWidget::clearBadges() {
    if (!m_scene->selectedItems().isEmpty()) {
        m_scene->clearSelection();
    }
    const QSignalBlocker blockSceneSignals(m_scene);
    for (auto* gi : std::as_const(m_graphicItems)) {
        m_scene->removeItem(gi);
        delete gi;
    }
    m_graphicItems.clear();
    m_hasSelection = false;
}

void DesignerWidget::removeSelectedBadges() {
    auto items = m_scene->selectedItems();
    if (!items.isEmpty()) {
        m_scene->clearSelection();
    }
    const QSignalBlocker blockSceneSignals(m_scene);
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
    if (m_eyedropperActive && event->key() == Qt::Key_Escape) {
        setEyedropperActive(false);
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        for (auto* gi : m_graphicItems) {
            gi->setSelected(true);
            gi->badge().isSelected = true;
            gi->updateHandles();
        }
        m_hasSelection = !m_graphicItems.isEmpty();
        emit selectionChanged();
        event->accept();
        return;
    }

    const bool hasSelection = m_hasSelection;
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

void DesignerWidget::setEyedropperActive(bool on) {
    if (m_eyedropperActive == on) {
        return;
    }
    m_eyedropperActive = on;
    if (m_eyedropperActive) {
        viewport()->setCursor(Qt::CrossCursor);
    } else {
        viewport()->unsetCursor();
    }
    emit eyedropperModeChanged(m_eyedropperActive);
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
        const double mmToPx = Constants::kMmToPx;
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

void DesignerWidget::setRulersVisible(bool b) {
    if (m_rulersVisible == b) {
        return;
    }
    m_rulersVisible = b;
    viewport()->update();
}

void DesignerWidget::setCanvasBackgroundMode(CanvasBackgroundMode mode) {
    if (m_canvasBackgroundMode == mode) {
        return;
    }
    m_canvasBackgroundMode = mode;
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
    const double mmToPx = Constants::kMmToPx;
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

void DesignerWidget::setZoomFactor(double factor) {
    constexpr double minZoom = 0.1;
    constexpr double maxZoom = 8.0;
    factor = std::clamp(factor, minZoom, maxZoom);
    const double current = zoomFactor();
    if (current <= 0.0 || qFuzzyCompare(current, factor)) {
        return;
    }
    scale(factor / current, factor / current);
    emit zoomChanged(factor);
}

void DesignerWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        setZoomFactor(zoomFactor() * factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DesignerWidget::mousePressEvent(QMouseEvent* event) {
    if (m_eyedropperActive) {
        if (event->button() == Qt::LeftButton) {
            const QColor color = sampleViewportColor(event->pos());
            if (color.isValid()) {
                emit eyedropperColorPicked(color);
            }
            setEyedropperActive(false);
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton) {
            setEyedropperActive(false);
            event->accept();
            return;
        }
    }
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

QColor DesignerWidget::sampleViewportColor(const QPoint& viewPos) const {
    if (!viewport()) {
        return {};
    }
    const QPixmap pixel = viewport()->grab(QRect(viewPos, QSize(1, 1)));
    if (pixel.isNull()) {
        return {};
    }
    const QImage image = pixel.toImage();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }
    return image.pixelColor(0, 0);
}

void DesignerWidget::drawBackground(QPainter* painter, const QRectF& rect) {
    const int tileSize = 16;
    const QColor windowColor = palette().color(QPalette::Window);
    const QColor baseColor = palette().color(QPalette::Base);
    const bool dark = windowColor.lightness() < 128;
    const bool printGuideGrid = m_canvasBackgroundMode == CanvasBackgroundMode::PrintGuideGrid;
    const QColor c1 = printGuideGrid ? QColor(250, 249, 246)
                                     : (dark ? blend(windowColor, baseColor, 0.16) : QColor(248, 247, 243));

    if (m_canvasBackgroundMode == CanvasBackgroundMode::White) {
        painter->fillRect(rect, Qt::white);
    } else if (m_canvasBackgroundMode == CanvasBackgroundMode::TransparentCheckerboard) {
        const QColor checkLight = dark ? QColor(55, 66, 80) : QColor(248, 249, 251);
        const QColor checkDark = dark ? QColor(42, 52, 65) : QColor(228, 232, 238);
        int left = int(rect.left()) - ((int(rect.left()) % tileSize + tileSize) % tileSize);
        int top = int(rect.top()) - ((int(rect.top()) % tileSize + tileSize) % tileSize);

        for (int y = top; y < int(rect.bottom()); y += tileSize) {
            for (int x = left; x < int(rect.right()); x += tileSize) {
                const QColor c = ((x / tileSize + y / tileSize) & 1) ? checkDark : checkLight;
                painter->fillRect(QRect(x, y, tileSize, tileSize), c);
            }
        }
    } else {
        painter->fillRect(rect, c1);
    }

    if (!m_gridVisible) {
        return;
    }

    const double mmToPx = Constants::kMmToPx;
    const double gridSpacingMm = std::max(0.5, m_gridSpacingMm);
    const double stepPx = std::max(1.0, gridSpacingMm * mmToPx);
    const double majorIntervalCount = std::max(1.0, std::ceil(10.0 / gridSpacingMm));
    const double majorStepPx = stepPx * majorIntervalCount;
    const double screenStepPx = stepPx * std::abs(transform().m11());
    const bool simplifiedGrid = screenStepPx < 3.0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    const QColor majorGridBase = dark && !printGuideGrid ? palette().color(QPalette::Mid) : QColor(170, 179, 194);
    const QColor minorGridBase = dark && !printGuideGrid
        ? blend(palette().color(QPalette::Mid), palette().color(QPalette::Highlight), 0.10)
        : QColor(201, 205, 211);
    const int majorAlpha = dark && !printGuideGrid ? 150 : 105;
    const int minorAlpha = dark && !printGuideGrid ? 86 : 58;
    const QColor majorGridColor = QColor::fromRgb(majorGridBase.red(), majorGridBase.green(), majorGridBase.blue(), majorAlpha);
    const QColor minorGridColor = QColor::fromRgb(minorGridBase.red(), minorGridBase.green(), minorGridBase.blue(), minorAlpha);

    const double screenFineStepPx = mmToPx * std::abs(transform().m11());
    if (printGuideGrid && gridSpacingMm > 1.0 && screenFineStepPx >= 3.0) {
        const QColor fineGridColor(191, 197, 206, 32);
        const double fineLeft = std::floor(rect.left() / mmToPx) * mmToPx;
        const double fineTop = std::floor(rect.top() / mmToPx) * mmToPx;
        painter->setPen(QPen(fineGridColor, 0.5));
        for (double x = fineLeft; x <= rect.right(); x += mmToPx) {
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (double y = fineTop; y <= rect.bottom(); y += mmToPx) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }

    const double gridStep = simplifiedGrid ? majorStepPx : stepPx;
    const int gridLeft = int(std::floor(rect.left() / gridStep) * gridStep);
    const int gridTop = int(std::floor(rect.top() / gridStep) * gridStep);

    for (double x = gridLeft; x <= rect.right(); x += gridStep) {
        const bool major = simplifiedGrid || std::fmod(std::abs(x), majorStepPx) < 0.5 || std::fmod(std::abs(x), majorStepPx) > majorStepPx - 0.5;
        painter->setPen(QPen(major ? majorGridColor : minorGridColor, major ? 1.0 : 0.5));
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    for (double y = gridTop; y <= rect.bottom(); y += gridStep) {
        const bool major = simplifiedGrid || std::fmod(std::abs(y), majorStepPx) < 0.5 || std::fmod(std::abs(y), majorStepPx) > majorStepPx - 0.5;
        painter->setPen(QPen(major ? majorGridColor : minorGridColor, major ? 1.0 : 0.5));
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    painter->restore();
}

void DesignerWidget::drawForeground(QPainter* painter, const QRectF& rect) {
    Q_UNUSED(rect);
    const double mmToPx = Constants::kMmToPx;
    double bleedPx = (m_badgeSizeMm + 3) * mmToPx;
    double finishPx = m_badgeSizeMm * mmToPx;
    double visiblePx = std::max(0.0, (m_badgeSizeMm - 4)) * mmToPx;

    const QPointF center = guideSceneCenter();
    double cx = center.x(), cy = center.y();

    double rb = bleedPx / 2, rf = finishPx / 2, rv = visiblePx / 2;

    // Bleed line (red dashed)
    if (m_guideBleed->isVisible()) {
        QPen pen(QColor(239, 68, 68), 1.5);
        pen.setDashPattern({4, 2});
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rb, rb);
    }

    // Visible area (green solid)
    if (m_guideVisible->isVisible()) {
        QPen pen(QColor(34, 197, 94), 2.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rv, rv);
    }

    if (m_graphicItems.isEmpty()) {
        painter->save();
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        const QColor title = dark ? QColor(241, 245, 252) : QColor(37, 48, 69);
        const QColor hint = dark ? QColor(177, 190, 208) : QColor(105, 117, 140);
        const double contentWidth = visiblePx * 0.78;
        const double contentHeight = visiblePx * 0.64;
        const QRectF titleRect(center.x() - contentWidth / 2.0,
                               center.y() - contentHeight * 0.49,
                               contentWidth,
                               contentHeight * 0.42);
        const QRectF hintRect(center.x() - contentWidth / 2.0,
                              center.y() - contentHeight * 0.01,
                              contentWidth,
                              contentHeight * 0.52);

        QFont titleFont = painter->font();
        titleFont.setPointSizeF(std::clamp(visiblePx / 23.0, 7.0, 11.0));
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(title);
        painter->drawText(titleRect,
                          Qt::AlignCenter | Qt::TextWordWrap,
                          QStringLiteral("ここにバッジを追加してください"));

        QFont hintFont = painter->font();
        hintFont.setPointSizeF(std::clamp(visiblePx / 27.0, 6.0, 9.0));
        hintFont.setBold(false);
        painter->setFont(hintFont);
        painter->setPen(hint);
        painter->drawText(hintRect,
                          Qt::AlignCenter | Qt::TextWordWrap,
                          QStringLiteral("画像をドロップするか、\nツールバーから追加できます"));
        painter->restore();
    }

    if (!m_graphicItems.isEmpty() && m_hasSelection) {
        QList<BadgeGraphicItem*> selectedBadges;
        QRectF selectionBounds;
        for (auto* item : m_scene->selectedItems()) {
            auto* badge = dynamic_cast<BadgeGraphicItem*>(item);
            if (!badge) {
                continue;
            }
            selectedBadges.push_back(badge);
            selectionBounds = selectionBounds.isNull()
                ? badge->sceneBoundingRect()
                : selectionBounds.united(badge->sceneBoundingRect());
        }
        if (!selectedBadges.isEmpty()) {
            const double mmToPx = Constants::kMmToPx;
            const QString dimensions = selectedBadges.size() == 1
                ? QStringLiteral("W: %1 mm\nH: %2 mm")
                      .arg(selectedBadges.first()->badge().widthMm, 0, 'f', 1)
                      .arg(selectedBadges.first()->badge().heightMm, 0, 'f', 1)
                : QStringLiteral("選択範囲\nW: %1 mm  H: %2 mm")
                      .arg(selectionBounds.width() / mmToPx, 0, 'f', 1)
                      .arg(selectionBounds.height() / mmToPx, 0, 'f', 1);
            const QPoint anchor = mapFromScene(selectionBounds.topRight()) + QPoint(10, -8);
            painter->save();
            painter->resetTransform();
            QFont dimensionFont = painter->font();
            dimensionFont.setPointSizeF(std::max(8.0, dimensionFont.pointSizeF() - 1.0));
            dimensionFont.setBold(true);
            painter->setFont(dimensionFont);
            const QFontMetricsF metrics(dimensionFont);
            QRectF box = metrics.boundingRect(QRectF(0.0, 0.0, 180.0, 80.0),
                                               Qt::AlignLeft | Qt::AlignVCenter,
                                               dimensions)
                             .adjusted(-8.0, -5.0, 8.0, 5.0);
            box.moveTopLeft(QPointF(anchor) + QPointF(0.0, -box.height()));
            const QRect viewportRect = viewport()->rect().adjusted(42, 28, -8, -8);
            if (box.right() > viewportRect.right()) {
                box.moveRight(viewportRect.right());
            }
            if (box.top() < viewportRect.top()) {
                box.moveTop(viewportRect.top());
            }
            const bool dark = palette().color(QPalette::Window).lightness() < 128;
            painter->setPen(Qt::NoPen);
            painter->setBrush(dark ? QColor(23, 34, 48, 238) : QColor(255, 255, 255, 245));
            painter->drawRoundedRect(box, 6.0, 6.0);
            painter->setPen(dark ? QColor(231, 239, 249) : QColor(38, 50, 74));
            painter->drawText(box.adjusted(8.0, 5.0, -8.0, -5.0),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              dimensions);
            painter->restore();
        }

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

    if (!m_rulersVisible) {
        return;
    }

    constexpr int kRulerWidth = 40;
    constexpr int kRulerHeight = 26;
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor rulerBackground = dark ? QColor(32, 42, 54) : QColor(248, 249, 252);
    const QColor rulerBorder = dark ? QColor(65, 82, 103) : QColor(211, 218, 230);
    const QColor rulerText = dark ? QColor(173, 188, 208) : QColor(105, 117, 140);
    const QColor rulerTick = dark ? QColor(111, 132, 159) : QColor(152, 165, 184);
    const QRect viewportRect = viewport()->rect();
    const QRect topRuler(kRulerWidth, 0, viewportRect.width() - kRulerWidth, kRulerHeight);
    const QRect leftRuler(0, kRulerHeight, kRulerWidth, viewportRect.height() - kRulerHeight);
    if (!topRuler.isEmpty() && !leftRuler.isEmpty()) {
        painter->save();
        painter->resetTransform();
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->fillRect(QRect(0, 0, kRulerWidth, kRulerHeight), rulerBackground);
        painter->fillRect(topRuler, rulerBackground);
        painter->fillRect(leftRuler, rulerBackground);
        painter->setPen(QPen(rulerBorder, 1.0));
        painter->drawLine(kRulerWidth, kRulerHeight - 1, viewportRect.right(), kRulerHeight - 1);
        painter->drawLine(kRulerWidth - 1, 0, kRulerWidth - 1, viewportRect.bottom());

        QFont rulerFont = painter->font();
        rulerFont.setPointSize(8);
        painter->setFont(rulerFont);
        painter->setPen(rulerText);

        const QRectF sceneBounds = m_scene->sceneRect();
        const QPointF sceneOrigin = sceneBounds.center();
        const QRectF visibleScene = mapToScene(viewportRect).boundingRect();
        const double pixelsPerMm = std::abs(transform().m11()) * mmToPx;
        const double targetIntervalMm = 32.0 / std::max(1e-6, pixelsPerMm);
        const double intervalMagnitude = std::pow(10.0, std::floor(std::log10(targetIntervalMm)));
        const double normalizedInterval = targetIntervalMm / intervalMagnitude;
        const double intervalStep = normalizedInterval <= 1.0 ? 1.0
                                  : normalizedInterval <= 2.0 ? 2.0
                                  : normalizedInterval <= 5.0 ? 5.0
                                  : 10.0;
        const double intervalMm = intervalStep * intervalMagnitude;
        const double intervalScene = intervalMm * mmToPx;
        const bool showMinorTicks = intervalMm * pixelsPerMm / 5.0 >= 6.0;
        const int firstX = static_cast<int>(std::ceil((visibleScene.left() - sceneOrigin.x()) / intervalScene));
        const int lastX = static_cast<int>(std::floor((visibleScene.right() - sceneOrigin.x()) / intervalScene));
        for (int tick = firstX; tick <= lastX; ++tick) {
            const double majorSceneX = sceneOrigin.x() + tick * intervalScene;
            if (showMinorTicks) {
                for (int minor = 1; minor < 5; ++minor) {
                    const qreal minorX = mapFromScene(QPointF(majorSceneX + minor * intervalScene / 5.0,
                                                              sceneBounds.top())).x();
                    painter->setPen(QPen(rulerTick, 1.0));
                    painter->drawLine(QPointF(minorX, kRulerHeight - 3), QPointF(minorX, kRulerHeight));
                }
            }
            const qreal x = mapFromScene(QPointF(majorSceneX, sceneBounds.top())).x();
            painter->setPen(QPen(rulerTick, 1.0));
            painter->drawLine(QPointF(x, kRulerHeight - 6), QPointF(x, kRulerHeight));
            painter->setPen(rulerText);
            painter->drawText(QRectF(x - 21.0, 1.0, 42.0, kRulerHeight - 8.0),
                              Qt::AlignHCenter | Qt::AlignVCenter,
                              QString::number(tick * intervalMm, 'f', 0));
        }

        const int firstY = static_cast<int>(std::ceil((visibleScene.top() - sceneOrigin.y()) / intervalScene));
        const int lastY = static_cast<int>(std::floor((visibleScene.bottom() - sceneOrigin.y()) / intervalScene));
        for (int tick = firstY; tick <= lastY; ++tick) {
            const double majorSceneY = sceneOrigin.y() + tick * intervalScene;
            if (showMinorTicks) {
                for (int minor = 1; minor < 5; ++minor) {
                    const qreal minorY = mapFromScene(QPointF(sceneBounds.left(), majorSceneY + minor * intervalScene / 5.0)).y();
                    painter->setPen(QPen(rulerTick, 1.0));
                    painter->drawLine(QPointF(kRulerWidth - 3, minorY), QPointF(kRulerWidth, minorY));
                }
            }
            const qreal y = mapFromScene(QPointF(sceneBounds.left(), majorSceneY)).y();
            painter->setPen(QPen(rulerTick, 1.0));
            painter->drawLine(QPointF(kRulerWidth - 6, y), QPointF(kRulerWidth, y));
            painter->setPen(rulerText);
            painter->drawText(QRectF(1.0, y - 9.0, kRulerWidth - 10.0, 18.0),
                              Qt::AlignRight | Qt::AlignVCenter,
                              QString::number(tick * intervalMm, 'f', 0));
        }
        painter->setPen(rulerText);
        painter->drawText(QRect(1, 0, kRulerWidth - 3, kRulerHeight - 2),
                          Qt::AlignCenter,
                          QStringLiteral("mm"));
        painter->restore();
    }
}

void DesignerWidget::setBatchMode(bool on) { m_batchMode = on; viewport()->update(); }

void DesignerWidget::setExperimentalGpuViewport(bool on) {
    viewportbackend::applySceneViewportProfile(this, on);
}

void DesignerWidget::setSafetyGuideEntries(const QList<SafetyGuideEntry>& entries) {
    m_safetyGuideEntries = entries;
    viewport()->update();
}

void DesignerWidget::setInteractiveViewportMode(bool on) {
    // Image layers and antialiased handles can paint slightly beyond the
    // moved item's previous update region. A full update while dragging
    // avoids the one-time ghost left after adding a new image.
    setViewportUpdateMode(on ? QGraphicsView::FullViewportUpdate : QGraphicsView::SmartViewportUpdate);
    viewport()->update();
}

bool DesignerWidget::isNearSelectedItem(const QPoint& viewPos) const {
    constexpr int kTolerancePx = 10;
    if (!m_hasSelection) {
        return false;
    }
    for (auto* gi : m_graphicItems) {
        if (!gi) {
            continue;
        }
        if (!gi->isSelected()) {
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
    const QPointF scenePosition = mapToScene(event->position().toPoint());
    for (const auto& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (isImageFile(path)) {
            emit imageDropRequested(path, scenePosition);
        }
    }
    event->acceptProposedAction();
}
