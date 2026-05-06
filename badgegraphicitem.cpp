#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QFileInfo>
#include <QStyleOptionGraphicsItem>
#include <QCursor>
#include <QRandomGenerator>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <wobjectimpl.h>

namespace {
void paintRealisticSurface(QPainter* painter,
                           const QRectF& r,
                           bool clipToCircle,
                           quint32 seed,
                           int materialPreset,
                           double specularStrength,
                           double envReflectionStrength,
                           double glitterStrength) {
    painter->save();
    if (clipToCircle) {
        QPainterPath path;
        path.addEllipse(r);
        painter->setClipPath(path);
    }

    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setCompositionMode(QPainter::CompositionMode_Screen);
    const double specBoost = std::clamp(specularStrength, 0.0, 1.25);
    const double envBoost = std::clamp(envReflectionStrength, 0.0, 1.0);
    const double glitterBoost = std::clamp(glitterStrength, 0.0, 0.85);
    const double matteBias = materialPreset == 1 ? 0.45 : 1.0;
    const double holoBias = materialPreset == 2 ? 1.05 : 1.0;
    const double pearlBias = materialPreset == 3 ? 0.95 : 1.0;
    const double glitterSheetBias = materialPreset == 4 ? 1.05 : 1.0;

    QRadialGradient coreGlow(r.center() - QPointF(0.0, r.height() * 0.08), std::max(r.width(), r.height()) * 0.60);
    coreGlow.setColorAt(0.0, QColor(255, 255, 255, int(24 * specBoost * matteBias)));
    coreGlow.setColorAt(0.40, QColor(255, 255, 255, int(9 * specBoost * matteBias)));
    coreGlow.setColorAt(0.75, QColor(255, 255, 255, int(3 * specBoost)));
    coreGlow.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillRect(r, coreGlow);

    QLinearGradient mirrorBand(r.topLeft(), r.bottomRight());
    mirrorBand.setColorAt(0.0, QColor(255, 255, 255, 0));
    mirrorBand.setColorAt(0.22, QColor(255, 255, 255, int(6 * envBoost * pearlBias)));
    mirrorBand.setColorAt(0.42, QColor(255, 255, 255, int(22 * specBoost * holoBias)));
    mirrorBand.setColorAt(0.58, QColor(255, 255, 255, int(10 * envBoost * holoBias)));
    mirrorBand.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->setBrush(mirrorBand);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(r.adjusted(r.width() * 0.03, r.height() * 0.02,
                                    -r.width() * 0.03, -r.height() * 0.02));

    painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    QRadialGradient edgeShade(r.center(), std::max(r.width(), r.height()) * 0.60);
    edgeShade.setColorAt(0.0, QColor(0, 0, 0, 0));
    edgeShade.setColorAt(0.72, QColor(0, 0, 0, int(3 + 2 * (1.0 - specBoost))));
    edgeShade.setColorAt(1.0, QColor(0, 0, 0, int(32 + 6 * (1.0 - matteBias))));
    painter->fillRect(r, edgeShade);

    painter->setCompositionMode(QPainter::CompositionMode_Screen);
    const int glitterAlpha = int(8 * glitterBoost * glitterSheetBias);
    QBrush glitterBrush(QColor(255, 255, 255, glitterAlpha), Qt::BDiagPattern);
    QTransform glitterTransform;
    glitterTransform.rotate(18.0 + (seed % 19));
    glitterTransform.scale(0.80, 0.80);
    glitterBrush.setTransform(glitterTransform);
    painter->setBrush(glitterBrush);
    painter->drawEllipse(r.adjusted(r.width() * 0.05, r.height() * 0.05,
                                    -r.width() * 0.05, -r.height() * 0.05));

    QRandomGenerator rng(seed);
    painter->setPen(Qt::NoPen);
    const int sparkleCount = materialPreset == 4 ? 10 : (materialPreset == 2 ? 6 : 8);
    for (int i = 0; i < sparkleCount; ++i) {
        const double t = rng.generateDouble();
        const double angle = rng.generateDouble() * 2.0 * M_PI;
        const double rad = std::sqrt(rng.generateDouble()) * std::min(r.width(), r.height()) * 0.44;
        const QPointF p = r.center() + QPointF(std::cos(angle) * rad, std::sin(angle) * rad * 0.82 - r.height() * 0.05);
        const int alpha = 6 + int(14 * t * glitterBoost);
        painter->setBrush(QColor(255, 255, 255, alpha));
        painter->drawEllipse(p, 0.8 + rng.generateDouble() * 1.8, 0.5 + rng.generateDouble() * 1.2);
    }

    painter->restore();
}
} // namespace

// --- ResizeHandle: small corner square for resizing ---
class ResizeHandle : public QGraphicsRectItem {
public:
    enum Corner { TL, TR, BL, BR };
    ResizeHandle(Corner c, BadgeGraphicItem* parent) : QGraphicsRectItem(parent), m_corner(c), m_badge(parent) {
        setRect(-6, -6, 12, 12);
        setBrush(Qt::white);
        setPen(QPen(Qt::blue, 1.5));
        setFlag(ItemIgnoresTransformations, true);
        setFlag(ItemIsMovable, false);
        setFlag(ItemIsSelectable, false);
        setAcceptHoverEvents(true);
        setZValue(1000);
        switch (c) {
            case TL: case BR: setCursor(Qt::SizeFDiagCursor); break;
            case TR: case BL: setCursor(Qt::SizeBDiagCursor); break;
        }
    }
    Corner corner() const { return m_corner; }
protected:
    QPointF m_startPos;
    
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        m_dragging = true;
        m_startPos = e->scenePos();
        if (m_badge) {
            m_badge->beginInteractiveEdit();
        }
        e->accept();
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (!m_dragging) return;
        QPointF deltaLocal = m_badge->mapFromScene(e->scenePos()) - m_badge->mapFromScene(m_startPos);
        m_startPos = e->scenePos();
        const double mmToPx = 96.0 / 25.4;
        BadgeItem& b = m_badge->badge();
        if (!b.imagePath.isEmpty() || !b.layers.isEmpty()) {
            const double signX = (m_corner == TL || m_corner == BL) ? -1.0 : 1.0;
            const double signY = (m_corner == TL || m_corner == TR) ? -1.0 : 1.0;
            const double basePx = std::max(1.0, std::max(b.widthMm, b.heightMm) * mmToPx);
            const double growthPx = std::max(signX * deltaLocal.x(), signY * deltaLocal.y());
            const double factor = 1.0 + (growthPx / basePx);
            b.imageScale = std::clamp(b.imageScale * factor, 0.1, 5.0);
        } else {
            if (m_corner == TL || m_corner == BL) { b.widthMm -= deltaLocal.x() / mmToPx; b.xMm += deltaLocal.x() / mmToPx; }
            else { b.widthMm += deltaLocal.x() / mmToPx; }
            if (m_corner == TL || m_corner == TR) { b.heightMm -= deltaLocal.y() / mmToPx; b.yMm += deltaLocal.y() / mmToPx; }
            else { b.heightMm += deltaLocal.y() / mmToPx; }
            if (b.widthMm < 5) b.widthMm = 5;
            if (b.heightMm < 5) b.heightMm = 5;
        }
        m_badge->updateHandles();
        m_badge->update();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        m_dragging = false;
        m_badge->syncFromBadge();
        if (m_badge) {
            m_badge->endInteractiveEdit();
        }
    }
private:
    Corner m_corner;
    BadgeGraphicItem* m_badge;
    bool m_dragging = false;
};

BadgeGraphicItem::BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_badge(badge) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setTransformOriginPoint(contentRectPx().center());
    loadImage();
    updateHandles();
}

W_OBJECT_IMPL(BadgeGraphicItem)

void BadgeGraphicItem::updateHandles() {
    if (!m_badge.isSelected) {
        for (auto* h : m_handles) {
            if (h) {
                h->setVisible(false);
            }
        }
        return;
    }
    const QRectF rect = visualRectPx();
    const double margin = 3.0;
    QPointF corners[4] = {
        QPointF(rect.left() - margin, rect.top() - margin),
        QPointF(rect.right() + margin, rect.top() - margin),
        QPointF(rect.left() - margin, rect.bottom() + margin),
        QPointF(rect.right() + margin, rect.bottom() + margin)
    };
    int types[4] = {0,1,2,3};
    for (int i = 0; i < 4; ++i) {
        if (i < m_handles.size()) {
            m_handles[i]->setPos(corners[i]);
            m_handles[i]->setVisible(true);
        } else {
            auto* h = new ResizeHandle(static_cast<ResizeHandle::Corner>(types[i]), this);
            h->setPos(corners[i]);
            h->setVisible(true);
            m_handles.append(h);
        }
    }
}

void BadgeGraphicItem::beginInteractiveEdit() {
    if (m_interactionActive) {
        return;
    }
    m_interactionActive = true;
    emit badgeEditStarted(this);
}

void BadgeGraphicItem::endInteractiveEdit() {
    if (!m_interactionActive) {
        return;
    }
    m_interactionActive = false;
    emit badgeEditFinished(this);
}

QRectF BadgeGraphicItem::contentRectPx() const {
    const double mmToPx = 96.0 / 25.4;
    const double pw = m_badge.widthMm * mmToPx;
    const double ph = m_badge.heightMm * mmToPx;
    const double margin = m_badge.isSelected ? 3.0 : 2.0;
    return QRectF(margin, margin, pw, ph);
}

QRectF BadgeGraphicItem::imageRectPx() const {
    const QRectF content = contentRectPx();
    const double scale = std::max(0.1, m_badge.imageScale);
    const QSizeF scaledSize(content.width() * scale, content.height() * scale);
    return QRectF(content.center().x() - scaledSize.width() * 0.5,
                  content.center().y() - scaledSize.height() * 0.5,
                  scaledSize.width(),
                  scaledSize.height());
}

QRectF BadgeGraphicItem::visualRectPx() const {
    QRectF rect = contentRectPx();
    if (!m_thumbnail.isNull() || !m_processed.isNull()) {
        rect = rect.united(imageRectPx());
    }

    const QRectF content = contentRectPx();
    for (const auto& layer : m_badge.layers) {
        const QRectF layerRect = content.translated(layer.offsetX * 96.0 / 25.4,
                                                    layer.offsetY * 96.0 / 25.4);
        rect = rect.united(layerRect);
    }

    return rect.adjusted(-3.0, -3.0, 3.0, 3.0);
}

QRectF BadgeGraphicItem::boundingRect() const {
    return visualRectPx();
}

bool BadgeGraphicItem::shouldClipDesignerPreview() const {
    const bool hasImageContent = !m_thumbnail.isNull()
        || !m_processed.isNull()
        || !m_badge.imagePath.isEmpty()
        || !m_badge.layers.isEmpty();
    return m_badge.clipToCircle && !hasImageContent;
}

QPainterPath BadgeGraphicItem::shape() const {
    QPainterPath path;
    const QRectF rect = visualRectPx();
    if (shouldClipDesignerPreview()) {
        path.addEllipse(rect);
    } else {
        path.addRect(rect);
    }
    return path;
}

void BadgeGraphicItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    const QRectF r = contentRectPx();
    renderCore(painter, r);
}

void BadgeGraphicItem::renderCore(QPainter* painter, const QRectF& r) {
    painter->save();
    if (shouldClipDesignerPreview()) {
        QPainterPath path;
        path.addEllipse(r);
        painter->setClipPath(path);
    }
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    bool drewAnything = false;

    // Draw the primary image layer first, then overlay any additional layers on top.
    const QPixmap& baseImage = m_processed.isNull() ? m_thumbnail : m_processed;
    const LayerItem* primaryLayer = m_badge.layers.isEmpty() ? nullptr : &m_badge.layers.first();
    if (!baseImage.isNull() && (!primaryLayer || primaryLayer->visible)) {
        painter->setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
        const double scale = std::max(0.1, m_badge.imageScale);
        const QSizeF scaledSize(r.width() * scale, r.height() * scale);
        const QRectF imageRect(r.center().x() - scaledSize.width() * 0.5,
                               r.center().y() - scaledSize.height() * 0.5,
                               scaledSize.width(),
                               scaledSize.height());
        painter->drawPixmap(imageRect, baseImage, QRectF(baseImage.rect()));
        drewAnything = true;
    }

    // Render layers on top of the primary image.
    const int startLayer = m_badge.layers.isEmpty() ? 0 : 1;
    for (int layerIndex = startLayer; layerIndex < m_badge.layers.size(); ++layerIndex) {
        const auto& layer = m_badge.layers[layerIndex];
        if (!layer.visible) continue;
        QPixmap img;
        if (!layer.imagePath.isEmpty() && QFileInfo::exists(layer.imagePath))
            img.load(layer.imagePath);
        if (!img.isNull()) {
            painter->setOpacity(layer.opacity);
            const QRectF lr = r.translated(layer.offsetX * 96.0 / 25.4,
                                           layer.offsetY * 96.0 / 25.4);
            painter->drawPixmap(lr, img, QRectF(img.rect()));
            drewAnything = true;
        }
    }

    painter->setOpacity(1.0);

    // Placeholder if nothing drawn
    if (!drewAnything) {
        painter->fillRect(r, QColor(240, 240, 240));
        QPen dashPen(Qt::lightGray);
        dashPen.setDashPattern({2, 2});
        painter->setPen(dashPen);
        painter->drawEllipse(r.adjusted(2,2,-2,-2));
        painter->setPen(Qt::gray);
        painter->setFont(QFont("Arial", 7));
        painter->drawText(r, Qt::AlignCenter, "Drop image\nor DoubleClick");
    }

    if (!m_badge.displayText.isEmpty()) {
        painter->setOpacity(1.0);
        QFont font("Arial", 10);
        painter->setFont(font);
        painter->setPen(Qt::white);
        painter->drawText(r.adjusted(1,1,1,1), Qt::AlignCenter, m_badge.displayText);
        painter->setPen(Qt::black);
        painter->drawText(r, Qt::AlignCenter, m_badge.displayText);
    }
    painter->restore();
}

void BadgeGraphicItem::loadImage() {
    m_loadedImagePath = !m_badge.layers.isEmpty() ? m_badge.layers.first().imagePath : m_badge.imagePath;
    m_thumbnail = QPixmap();
    m_processed = QPixmap();
    m_colorSpaceLabel = QStringLiteral("読み込みなし");

    if (m_loadedImagePath.isEmpty() || !QFileInfo::exists(m_loadedImagePath)) {
        update();
        return;
    }

    QString label;
    m_thumbnail = QPixmap::fromImage(ImageProcessor::loadImage(m_loadedImagePath, &label));
    m_colorSpaceLabel = label;
    applyColorCorrection();
}

void BadgeGraphicItem::applyColorCorrection() {
    if (m_badge.brightness == 0 && m_badge.contrast == 0 && m_badge.saturation == 0) {
        m_processed = QPixmap();
        update();
        return;
    }
    if (!m_thumbnail.isNull())
        m_processed = ImageProcessor::applyCorrection(m_thumbnail, m_badge.brightness, m_badge.contrast, m_badge.saturation);
    update();
}

void BadgeGraphicItem::syncFromBadge() {
    prepareGeometryChange();
    setTransformOriginPoint(contentRectPx().center());
    const QString currentPrimaryPath = !m_badge.layers.isEmpty() ? m_badge.layers.first().imagePath : m_badge.imagePath;
    if (currentPrimaryPath != m_loadedImagePath) {
        loadImage();
    }
    bool changed = false;
    double px = m_badge.xMm * 96.0 / 25.4;
    double py = m_badge.yMm * 96.0 / 25.4;
    if (qAbs(pos().x() - px) > 0.1 || qAbs(pos().y() - py) > 0.1) {
        setPos(px, py);
        changed = true;
    }
    if (qAbs(rotation() - m_badge.rotation) > 0.1) {
        setRotation(m_badge.rotation);
        changed = true;
    }
    updateHandles();
    update();
    if (changed)
        emit badgeMoved(this);
}

void BadgeGraphicItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        beginInteractiveEdit();
        emit badgeClicked(this);
    }
}

void BadgeGraphicItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    Q_UNUSED(event);
    emit badgeDoubleClicked(this);
}

void BadgeGraphicItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        endInteractiveEdit();
    }
}

QVariant BadgeGraphicItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && m_snapToGrid && m_gridSpacingMm > 0.0) {
        const double mmToPx = 96.0 / 25.4;
        const double stepPx = m_gridSpacingMm * mmToPx;
        if (stepPx > 0.0) {
            QPointF p = value.toPointF();
            p.setX(std::round(p.x() / stepPx) * stepPx);
            p.setY(std::round(p.y() / stepPx) * stepPx);
            return p;
        }
    }
    if (change == ItemPositionHasChanged) {
        const double mmToPx = 96.0 / 25.4;
        m_badge.xMm = pos().x() / mmToPx;
        m_badge.yMm = pos().y() / mmToPx;
        emit badgeMoved(this);
    }
    if (change == ItemSelectedHasChanged) {
        prepareGeometryChange();
        m_badge.isSelected = value.toBool();
        updateHandles();
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
