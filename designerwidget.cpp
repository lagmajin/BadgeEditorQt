#include "designerwidget.h"
#include <QGraphicsSceneMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QImageReader>
#include <QSet>
#include <cmath>

class LightingOverlayItem : public QGraphicsItem {
public:
    LightingOverlayItem(QGraphicsItem* parent = nullptr) : QGraphicsItem(parent) {
        setFlag(ItemIgnoresTransformations, true);
    }
    QRectF boundingRect() const override { return QRectF(-m_radius, -m_radius, m_radius * 2, m_radius * 2); }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (!m_enabled) return;
        QRectF r = boundingRect();
        painter->save();
        QRadialGradient shade(r.center(), r.width() / 2);
        double rad = m_angle * M_PI / 180.0;
        QPointF lightCenter = r.center() + QPointF(cos(rad) * r.width() * 0.3, -sin(rad) * r.height() * 0.3);
        shade.setCenter(lightCenter);
        shade.setFocalPoint(lightCenter);
        shade.setColorAt(0.0, QColor(255, 255, 255, 0));
        shade.setColorAt(0.35, QColor(0, 0, 0, int(10 * m_intensity)));
        shade.setColorAt(0.7, QColor(0, 0, 0, int(50 * m_intensity)));
        shade.setColorAt(1.0, QColor(0, 0, 0, int(120 * m_intensity)));
        painter->setBrush(shade);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(r);
        QRadialGradient spec(QPointF(r.left() + r.width() * 0.22, r.top() + r.height() * 0.14), r.width() * 0.15);
        spec.setColorAt(0.0, QColor(255, 255, 255, int(200 * m_intensity)));
        spec.setColorAt(0.5, QColor(255, 255, 255, int(80 * m_intensity)));
        spec.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter->setBrush(spec);
        double specR = r.width() * 0.12;
        painter->drawEllipse(QPointF(r.left() + r.width() * 0.22, r.top() + r.height() * 0.14), specR, specR * 0.7);
        painter->restore();
    }
    void setEnabled(bool on) { m_enabled = on; update(); }
    void setAngle(double a) { m_angle = a; update(); }
    void setIntensity(double v) { m_intensity = v; update(); }
    void setRadius(double r) { prepareGeometryChange(); m_radius = r; update(); }
private:
    bool m_enabled = false;
    double m_angle = 0;
    double m_intensity = 0.5;
    double m_radius = 100;
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
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(Qt::lightGray);

    // Guide circles (drawn in drawForeground, not as scene items)
    m_guideBleed = new GuideItem;
    m_guideVisible = new GuideItem;

    // Lighting overlay centered on guide
    m_lightingOverlay = new LightingOverlayItem;
    m_scene->addItem(m_lightingOverlay);
    m_lightingOverlay->setZValue(9999);
}

void DesignerWidget::addBadge(const BadgeItem& item) {
    auto* gi = new BadgeGraphicItem(item);
    m_scene->addItem(gi);
    gi->syncFromBadge();
    m_graphicItems.append(gi);
    connect(gi, &BadgeGraphicItem::badgeClicked, this, &DesignerWidget::badgeSelected);
    connect(gi, &BadgeGraphicItem::badgeDoubleClicked, this, &DesignerWidget::badgeDoubleClicked);
    connect(gi, &BadgeGraphicItem::badgeMoved, this, &DesignerWidget::badgeMoved);
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
    m_guideSceneCenter = mapToScene(viewport()->rect().center());
    positionGuideOverlays();
    if (m_glitterGroup) regenerateGlitter();
    viewport()->update();
}

void DesignerWidget::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    m_guideSceneCenter = mapToScene(viewport()->rect().center());
    positionGuideOverlays();
}

void DesignerWidget::positionGuideOverlays() {
    const double mmToPx = 96.0 / 25.4;
    double finishR = m_badgeSizeMm * mmToPx / 2;
    if (m_glitterGroup) m_glitterGroup->setPos(m_guideSceneCenter);
    if (m_lightingOverlay) {
        m_lightingOverlay->setPos(m_guideSceneCenter);
        auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
        if (lo) lo->setRadius(finishR);
    }
}

void DesignerWidget::setGuidesVisible(bool b) {
    m_guideBleed->setVisible(b);
    m_guideVisible->setVisible(b);
}

void DesignerWidget::setBleedVisible(bool b) { m_guideBleed->setVisible(b); }
void DesignerWidget::setVisibleVisible(bool b) { m_guideVisible->setVisible(b); }
void DesignerWidget::setLightingEnabled(bool on) {
    m_lightingEnabled = on;
    auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
    if (lo) lo->setEnabled(on);
    setViewportUpdateMode(on ? QGraphicsView::FullViewportUpdate : QGraphicsView::SmartViewportUpdate);
}
void DesignerWidget::setLightAngle(int degrees) {
    m_lightAngle = degrees;
    auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
    if (lo) lo->setAngle(degrees);
}
void DesignerWidget::setLightIntensity(double val) {
    m_lightIntensity = val;
    auto* lo = qgraphicsitem_cast<LightingOverlayItem*>(m_lightingOverlay);
    if (lo) lo->setIntensity(val);
}

void DesignerWidget::setGlitterEnabled(bool on) {
    if (m_glitterGroup && !on) { m_scene->removeItem(m_glitterGroup); delete m_glitterGroup; m_glitterGroup = nullptr; }
    if (on) regenerateGlitter();
}

void DesignerWidget::setGlitterPattern(int pattern) {
    m_glitterPattern = pattern;
    if (m_glitterGroup) regenerateGlitter();
}

void DesignerWidget::regenerateGlitter() {
    if (m_glitterGroup) { m_scene->destroyItemGroup(qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup)); m_glitterGroup = nullptr; }
    m_glitterGroup = m_scene->createItemGroup({});
    auto* group = qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup);
    group->setPos(m_guideSceneCenter);
    createGlitter(m_glitterPattern);
}

void DesignerWidget::createGlitter(int pattern) {
    if (!m_glitterGroup) m_glitterGroup = m_scene->createItemGroup({});
    auto* group = qgraphicsitem_cast<QGraphicsItemGroup*>(m_glitterGroup);
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
        if (group) group->addToGroup(dot);
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
    const int tileSize = 16;
    QColor c1(255, 255, 255);
    QColor c2(191, 191, 191);

    int left = int(rect.left()) - ((int(rect.left()) % tileSize + tileSize) % tileSize);
    int top = int(rect.top()) - ((int(rect.top()) % tileSize + tileSize) % tileSize);

    for (int y = top; y < int(rect.bottom()); y += tileSize) {
        for (int x = left; x < int(rect.right()); x += tileSize) {
            QColor c = ((x / tileSize + y / tileSize) & 1) ? c2 : c1;
            painter->fillRect(QRect(x, y, tileSize, tileSize), c);
        }
    }
}

void DesignerWidget::drawForeground(QPainter* painter, const QRectF& rect) {
    Q_UNUSED(rect);
    const double mmToPx = 96.0 / 25.4;
    double bleedPx = (m_badgeSizeMm + 3) * mmToPx;
    double finishPx = m_badgeSizeMm * mmToPx;
    double visiblePx = std::max(0.0, (m_badgeSizeMm - 4)) * mmToPx;

    double cx = m_guideSceneCenter.x(), cy = m_guideSceneCenter.y();

    double rb = bleedPx / 2, rf = finishPx / 2, rv = visiblePx / 2;

    // バッチモード: ガイドライン外のバッジを暗くする
    if (m_batchMode) {
        for (auto* gi : m_graphicItems) {
            QRectF badgeRect = gi->mapToScene(gi->boundingRect()).boundingRect();
            QPointF badgeCenter = badgeRect.center();
            double dist = QLineF(cx, cy, badgeCenter.x(), badgeCenter.y()).length();
            if (dist > rv + badgeRect.width() / 2) {
                // バッジが可視エリアの外にある
                painter->save();
                QTransform t;
                t.translate(gi->pos().x(), gi->pos().y());
                t.rotate(gi->rotation());
                painter->setTransform(t, true);
                painter->fillRect(gi->boundingRect(), QColor(0, 0, 0, 80));
                painter->restore();
            }
        }
    }

    // 1. 全体を暗くするオーバーレイ（後で可視エリアをくり抜く）
    painter->save();
    QPainterPath darkOverlay;
    darkOverlay.addRect(rect);
    QPainterPath visibleCircle;
    visibleCircle.addEllipse(QPointF(cx, cy), rv, rv);
    painter->fillPath(darkOverlay.subtracted(visibleCircle), QColor(0, 0, 0, 60));
    painter->restore();

    // 2. 可視エリア内に斜線パターン（白ハッチで目立たせる）
    QPainterPath visiblePath;
    visiblePath.addEllipse(QPointF(cx, cy), rv, rv);
    QBrush visibleHatch(QColor(255, 255, 255, 30), Qt::FDiagPattern);
    painter->fillPath(visiblePath, visibleHatch);

    // 3. bleedとfinishの間にハッチ fill
    QPainterPath bleedPath;
    bleedPath.addEllipse(QPointF(cx, cy), rb, rb);
    QPainterPath finishPath;
    finishPath.addEllipse(QPointF(cx, cy), rf, rf);
    QPainterPath hatchRegion = bleedPath.subtracted(finishPath);
    QBrush hatchBrush(QColor(255, 128, 128, 80), Qt::BDiagPattern);
    painter->fillPath(hatchRegion, hatchBrush);

    // 4. Bleed line (red dashed)
    if (m_guideBleed->isVisible()) {
        QPen pen(QColor(255, 0, 0), 1.5);
        pen.setDashPattern({4, 2});
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rb, rb);
    }

    // 5. Finish line (thin solid)
    QPen finishPen(QColor(255, 0, 0, 180), 0.8);
    painter->setPen(finishPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(cx, cy), rf, rf);

    // 6. Visible area (green solid)
    if (m_guideVisible->isVisible()) {
        QPen pen(QColor(0, 255, 0), 2.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), rv, rv);
    }
}

void DesignerWidget::setBatchMode(bool on) { m_batchMode = on; viewport()->update(); }

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
            BadgeItem b; b.imagePath = path;
            b.label = QFileInfo(path).baseName();
            addBadge(b);
        }
    }
}
