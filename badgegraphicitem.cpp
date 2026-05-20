#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include "constants.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QFileInfo>
#include <QApplication>
#include <QStyleOptionGraphicsItem>
#include <QCursor>
#include <QRandomGenerator>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <wobjectimpl.h>

namespace {
QRectF fitRectInside(const QRectF& target, const QSizeF& sourceSize) {
    if (target.isEmpty() || sourceSize.width() <= 0.0 || sourceSize.height() <= 0.0) {
        return target;
    }

    const qreal scale = std::min(target.width() / sourceSize.width(),
                                 target.height() / sourceSize.height());
    const QSizeF fittedSize(sourceSize.width() * scale, sourceSize.height() * scale);
    return QRectF(QPointF(target.center().x() - fittedSize.width() * 0.5,
                          target.center().y() - fittedSize.height() * 0.5),
                  fittedSize);
}

QPainter::CompositionMode compositionModeForLayer(LayerBlendMode mode) {
    switch (mode) {
    case LayerBlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case LayerBlendMode::Screen: return QPainter::CompositionMode_Screen;
    case LayerBlendMode::Overlay: return QPainter::CompositionMode_Overlay;
    case LayerBlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
    case LayerBlendMode::Add: return QPainter::CompositionMode_Plus;
    case LayerBlendMode::Normal:
    default:
        return QPainter::CompositionMode_SourceOver;
    }
}

QPixmap applyLayerFillColor(QPixmap pixmap, const QColor& fillColor) {
    if (pixmap.isNull() || !fillColor.isValid()) {
        return pixmap;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return pixmap;
    }

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), fillColor);
    painter.end();
    return QPixmap::fromImage(image);
}

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

// --- ResizeHandle: corner/edge square for resizing ---
namespace {
enum class HandleKind {
    TopLeft = 0,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
};

bool isCornerHandle(HandleKind kind) {
    return kind == HandleKind::TopLeft
        || kind == HandleKind::TopRight
        || kind == HandleKind::BottomLeft
        || kind == HandleKind::BottomRight;
}

QCursor cursorForHandle(HandleKind kind) {
    switch (kind) {
    case HandleKind::TopLeft:
    case HandleKind::BottomRight:
        return Qt::SizeFDiagCursor;
    case HandleKind::TopRight:
    case HandleKind::BottomLeft:
        return Qt::SizeBDiagCursor;
    case HandleKind::Top:
    case HandleKind::Bottom:
        return Qt::SizeVerCursor;
    case HandleKind::Left:
    case HandleKind::Right:
        return Qt::SizeHorCursor;
    }
    return Qt::ArrowCursor;
}

QPointF handlePosForRect(const QRectF& rect, HandleKind kind, qreal margin) {
    const qreal left = rect.left() - margin;
    const qreal right = rect.right() + margin;
    const qreal top = rect.top() - margin;
    const qreal bottom = rect.bottom() + margin;
    const qreal centerX = rect.center().x();
    const qreal centerY = rect.center().y();

    switch (kind) {
    case HandleKind::TopLeft: return QPointF(left, top);
    case HandleKind::Top: return QPointF(centerX, top);
    case HandleKind::TopRight: return QPointF(right, top);
    case HandleKind::Right: return QPointF(right, centerY);
    case HandleKind::BottomRight: return QPointF(right, bottom);
    case HandleKind::Bottom: return QPointF(centerX, bottom);
    case HandleKind::BottomLeft: return QPointF(left, bottom);
    case HandleKind::Left: return QPointF(left, centerY);
    }
    return rect.center();
}

} // namespace

class ResizeHandle : public QGraphicsRectItem {
public:
    ResizeHandle(HandleKind kind, BadgeGraphicItem* parent)
        : QGraphicsRectItem(parent), m_kind(kind), m_badge(parent) {
        setRect(-8, -8, 16, 16);
        setBrush(Qt::white);
        setPen(QPen(Qt::blue, 1.5));
        setFlag(ItemIgnoresTransformations, true);
        setFlag(ItemIsMovable, false);
        setFlag(ItemIsSelectable, false);
        setAcceptHoverEvents(true);
        setZValue(1000);
        setCursor(cursorForHandle(m_kind));
    }
    HandleKind kind() const { return m_kind; }
    void setAsymmetricMode(bool on) {
        if (on) {
            setBrush(QColor(255, 172, 73));
            setPen(QPen(QColor(255, 242, 228), 1.5));
            setCursor(isCornerHandle(m_kind) ? Qt::SizeAllCursor : cursorForHandle(m_kind));
        } else {
            setBrush(Qt::white);
            setPen(QPen(Qt::blue, 1.5));
            setCursor(cursorForHandle(m_kind));
        }
        update();
    }
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
        const double mmToPx = Constants::kMmToPx;
        BadgeItem& b = m_badge->badge();
        const bool hasImageContent = !b.imagePath.isEmpty() || !b.layers.isEmpty();
        const bool edgeHandle = !isCornerHandle(m_kind);
        const bool asymmetricResize = edgeHandle || (hasImageContent && (e->modifiers() & Qt::AltModifier));
        const bool keepAspect = !asymmetricResize && (e->modifiers() & Qt::ShiftModifier);
        const bool precisionMode = e->modifiers() & Qt::ControlModifier;
        const double sensitivity = precisionMode ? 0.25 : 1.0;
        const double startX = b.xMm;
        const double startY = b.yMm;
        const double startW = std::max(0.1, b.widthMm);
        const double startH = std::max(0.1, b.heightMm);
        m_badge->beginGeometryUpdate();
        if (hasImageContent && !asymmetricResize && isCornerHandle(m_kind)) {
            const double signX = (m_kind == HandleKind::TopLeft || m_kind == HandleKind::BottomLeft) ? -1.0 : 1.0;
            const double signY = (m_kind == HandleKind::TopLeft || m_kind == HandleKind::TopRight) ? -1.0 : 1.0;
            const double basePx = std::max(1.0, std::max(b.widthMm, b.heightMm) * mmToPx);
            const double growthPx = std::max(signX * deltaLocal.x(), signY * deltaLocal.y()) * sensitivity;
            const double factor = 1.0 + (growthPx / basePx);
            b.imageScale = std::clamp(b.imageScale * factor, 0.1, 5.0);
        } else {
            const double dx = (deltaLocal.x() * sensitivity) / mmToPx;
            const double dy = (deltaLocal.y() * sensitivity) / mmToPx;
            constexpr double kMinSizeMm = 5.0;
            double left = startX;
            double right = startX + startW;
            double top = startY;
            double bottom = startY + startH;
            switch (m_kind) {
            case HandleKind::TopLeft:
                left = startX + dx;
                top = startY + dy;
                break;
            case HandleKind::Top:
                top = startY + dy;
                break;
            case HandleKind::TopRight:
                right = startX + startW + dx;
                top = startY + dy;
                break;
            case HandleKind::Right:
                right = startX + startW + dx;
                break;
            case HandleKind::BottomRight:
                right = startX + startW + dx;
                bottom = startY + startH + dy;
                break;
            case HandleKind::Bottom:
                bottom = startY + startH + dy;
                break;
            case HandleKind::BottomLeft:
                left = startX + dx;
                bottom = startY + startH + dy;
                break;
            case HandleKind::Left:
                left = startX + dx;
                break;
            }

            if (right - left < kMinSizeMm) {
                if (m_kind == HandleKind::Left || m_kind == HandleKind::TopLeft || m_kind == HandleKind::BottomLeft) {
                    left = right - kMinSizeMm;
                } else {
                    right = left + kMinSizeMm;
                }
            }
            if (bottom - top < kMinSizeMm) {
                if (m_kind == HandleKind::Top || m_kind == HandleKind::TopLeft || m_kind == HandleKind::TopRight) {
                    top = bottom - kMinSizeMm;
                } else {
                    bottom = top + kMinSizeMm;
                }
            }

            double nextW = std::max(kMinSizeMm, right - left);
            double nextH = std::max(kMinSizeMm, bottom - top);
            if (keepAspect) {
                const double scale = std::max(nextW / startW, nextH / startH);
                nextW = std::max(kMinSizeMm, startW * scale);
                nextH = std::max(kMinSizeMm, startH * scale);
            }
            if (m_kind == HandleKind::TopLeft || m_kind == HandleKind::BottomLeft || m_kind == HandleKind::Left) {
                b.xMm = startX + (startW - nextW);
            } else {
                b.xMm = startX;
            }
            if (m_kind == HandleKind::TopLeft || m_kind == HandleKind::TopRight || m_kind == HandleKind::Top) {
                b.yMm = startY + (startH - nextH);
            } else {
                b.yMm = startY;
            }
            b.widthMm = nextW;
            b.heightMm = nextH;
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
    HandleKind m_kind;
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
    const bool asymmetricMode = (QApplication::keyboardModifiers() & Qt::AltModifier);
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
    const HandleKind kinds[8] = {
        HandleKind::TopLeft,
        HandleKind::Top,
        HandleKind::TopRight,
        HandleKind::Right,
        HandleKind::BottomRight,
        HandleKind::Bottom,
        HandleKind::BottomLeft,
        HandleKind::Left,
    };
    for (int i = 0; i < 8; ++i) {
        const QPointF pos = handlePosForRect(rect, kinds[i], margin);
        if (i < m_handles.size()) {
            m_handles[i]->setPos(pos);
            m_handles[i]->setVisible(true);
            if (auto* handle = dynamic_cast<ResizeHandle*>(m_handles[i])) {
                handle->setAsymmetricMode(asymmetricMode);
            }
        } else {
            auto* h = new ResizeHandle(kinds[i], this);
            h->setPos(pos);
            h->setVisible(true);
            h->setAsymmetricMode(asymmetricMode);
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

void BadgeGraphicItem::beginGeometryUpdate() {
    prepareGeometryChange();
}

QRectF BadgeGraphicItem::contentRectPx() const {
    const double mmToPx = Constants::kMmToPx;
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
    const LayerItem* primaryLayer = m_badge.layers.isEmpty() ? nullptr : &m_badge.layers.first();
    if (!m_thumbnail.isNull() || !m_processed.isNull()) {
        QRectF imageRect = imageRectPx();
        if (primaryLayer) {
            imageRect.translate(primaryLayer->offsetX * Constants::kMmToPx,
                                primaryLayer->offsetY * Constants::kMmToPx);
        }
        rect = rect.united(imageRect);
    }

    const QRectF content = contentRectPx();
    for (const auto& layer : m_badge.layers) {
        const QRectF layerRect = content.translated(layer.offsetX * Constants::kMmToPx,
                                                   layer.offsetY * Constants::kMmToPx);
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
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    renderCore(painter, r, lod < 0.8);
}

QString BadgeGraphicItem::previewCacheSignature() const {
    QString key;
    key.reserve(256);
    key += QString::number(int(std::round(m_badge.widthMm * 10.0)));
    key += QChar('|');
    key += QString::number(int(std::round(m_badge.heightMm * 10.0)));
    key += QChar('|');
    key += QString::number(int(std::round(m_badge.imageScale * 1000.0)));
    key += QChar('|');
    key += QString::number(m_badge.clipToCircle ? 1 : 0);
    key += QChar('|');
    key += m_badge.imagePath;
    key += QChar('|');
    key += QString::number(m_badge.brightness);
    key += QChar('|');
    key += QString::number(m_badge.contrast);
    key += QChar('|');
    key += QString::number(m_badge.saturation);
    key += QChar('|');
    key += QString::number(m_badge.materialPreset);
    key += QChar('|');
    key += QString::number(int(std::round(m_badge.specularStrength * 1000.0)));
    key += QChar('|');
    key += QString::number(int(std::round(m_badge.envReflectionStrength * 1000.0)));
    key += QChar('|');
    key += QString::number(int(std::round(m_badge.glitterStrength * 1000.0)));
    key += QChar('|');
    key += QString::number(m_badge.layers.size());
    for (const auto& layer : m_badge.layers) {
        key += QChar('|');
        key += layer.imagePath;
        key += QChar('|');
        key += QString::number(layer.visible ? 1 : 0);
        key += QChar('|');
        key += QString::number(int(std::round(layer.opacity * 1000.0)));
        key += QChar('|');
        key += QString::number(layer.offsetX);
        key += QChar('|');
        key += QString::number(layer.offsetY);
        key += QChar('|');
        key += QString::number(int(layer.blendMode));
        key += QChar('|');
        key += layer.fillColor.isValid() ? layer.fillColor.name(QColor::HexArgb) : QStringLiteral("none");
    }
    return key;
}

QPixmap BadgeGraphicItem::loadedPreviewPixmapForLayer(const LayerItem& layer) const {
    if (layer.imagePath.isEmpty() || !QFileInfo::exists(layer.imagePath)) {
        return {};
    }

    QString cacheKey = layer.imagePath;
    cacheKey += QChar('|');
    cacheKey += QString::number(m_badge.brightness);
    cacheKey += QChar('|');
    cacheKey += QString::number(m_badge.contrast);
    cacheKey += QChar('|');
    cacheKey += QString::number(m_badge.saturation);

    if (const auto it = m_layerPreviewCache.constFind(cacheKey); it != m_layerPreviewCache.cend()) {
        return it.value();
    }

    QString colorSpaceLabel;
    const QImage loaded = ImageProcessor::loadImage(layer.imagePath, &colorSpaceLabel);
    if (loaded.isNull()) {
        return {};
    }

    QPixmap pixmap = QPixmap::fromImage(loaded);
    if (m_badge.brightness != 0.0 || m_badge.contrast != 0.0 || m_badge.saturation != 0.0) {
        pixmap = ImageProcessor::applyCorrection(pixmap, m_badge.brightness, m_badge.contrast, m_badge.saturation);
    }

    m_layerPreviewCache.insert(cacheKey, pixmap);
    return pixmap;
}

void BadgeGraphicItem::rebuildPreviewCache(const QSize& pixelSize) {
    const QRectF r = contentRectPx();
    const QSize cacheSize = pixelSize.expandedTo(QSize(1, 1));
    QImage cache(cacheSize, QImage::Format_ARGB32_Premultiplied);
    cache.fill(Qt::transparent);

    QPainter painter(&cache);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal sx = r.width() > 0.0 ? qreal(cacheSize.width()) / r.width() : 1.0;
    const qreal sy = r.height() > 0.0 ? qreal(cacheSize.height()) / r.height() : 1.0;
    painter.scale(sx, sy);
    painter.translate(-r.topLeft());

    bool drewAnything = false;
    const QRectF localRect(QPointF(0.0, 0.0), r.size());
    if (shouldClipDesignerPreview()) {
        QPainterPath path;
        path.addEllipse(localRect);
        painter.setClipPath(path);
    }

    const LayerItem* primaryLayer = m_badge.layers.isEmpty() ? nullptr : &m_badge.layers.first();
    const QPixmap baseSource = m_processed.isNull() ? m_thumbnail : m_processed;
    const QPixmap baseImage = applyLayerFillColor(baseSource, primaryLayer ? primaryLayer->fillColor : QColor());
    if (!baseImage.isNull() && (!primaryLayer || primaryLayer->visible)) {
        painter.save();
        painter.setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
        painter.setCompositionMode(primaryLayer ? compositionModeForLayer(primaryLayer->blendMode) : QPainter::CompositionMode_SourceOver);
        const double scale = std::max(0.1, m_badge.imageScale);
        const QRectF targetRect(localRect.center().x() - localRect.width() * scale * 0.5,
                                localRect.center().y() - localRect.height() * scale * 0.5,
                                localRect.width() * scale,
                                localRect.height() * scale);
        QRectF imageRect = fitRectInside(targetRect, baseImage.size());
        if (primaryLayer) {
            imageRect.translate(primaryLayer->offsetX * Constants::kMmToPx,
                                primaryLayer->offsetY * Constants::kMmToPx);
        }
        painter.drawPixmap(imageRect, baseImage, QRectF(baseImage.rect()));
        painter.restore();
        drewAnything = true;
    }

    const int startLayer = m_badge.layers.isEmpty() ? 0 : 1;
    for (int layerIndex = startLayer; layerIndex < m_badge.layers.size(); ++layerIndex) {
        const auto& layer = m_badge.layers[layerIndex];
        if (!layer.visible) continue;
        QPixmap img = loadedPreviewPixmapForLayer(layer);
        img = applyLayerFillColor(img, layer.fillColor);
        if (!img.isNull()) {
            painter.save();
            painter.setOpacity(layer.opacity);
            painter.setCompositionMode(compositionModeForLayer(layer.blendMode));
            const QRectF lr = fitRectInside(localRect.translated(layer.offsetX * Constants::kMmToPx,
                                                                 layer.offsetY * Constants::kMmToPx),
                                            img.size());
            painter.drawPixmap(lr, img, QRectF(img.rect()));
            painter.restore();
            drewAnything = true;
        }
    }

    if (!drewAnything) {
        painter.fillRect(localRect, QColor(240, 240, 240));
        QPen dashPen(Qt::lightGray);
        dashPen.setDashPattern({2, 2});
        painter.setPen(dashPen);
        painter.drawEllipse(localRect.adjusted(2, 2, -2, -2));
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Arial", 7));
        painter.drawText(localRect, Qt::AlignCenter, "Drop image\nor DoubleClick");
    }

    painter.end();
    m_previewCache = cache;
    m_previewCacheSignature = previewCacheSignature();
    m_previewCachePixelSize = cacheSize;
}

void BadgeGraphicItem::renderCore(QPainter* painter, const QRectF& r, bool simplifiedPreview) {
    const QString signature = previewCacheSignature();
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    const qreal deviceScale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const qreal qualityScale = simplifiedPreview ? 1.0 : std::clamp(lod * deviceScale, 1.0, 6.0);
    const QSize requestedPixelSize(
        std::clamp(int(std::ceil(r.width() * qualityScale)), 1, 4096),
        std::clamp(int(std::ceil(r.height() * qualityScale)), 1, 4096));

    if (m_previewCache.isNull()
        || m_previewCacheSignature != signature
        || m_previewCachePixelSize != requestedPixelSize) {
        rebuildPreviewCache(requestedPixelSize);
    }

    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, !simplifiedPreview);
    painter->setRenderHint(QPainter::Antialiasing, !simplifiedPreview);
    painter->drawImage(r, m_previewCache);

    if (!simplifiedPreview && !m_badge.displayText.isEmpty()) {
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
    m_previewCache = QImage();
    m_previewCacheSignature.clear();
    m_previewCachePixelSize = QSize();
    m_layerPreviewCache.clear();

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
    m_previewCache = QImage();
    m_previewCacheSignature.clear();
    m_previewCachePixelSize = QSize();
    m_layerPreviewCache.clear();
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
    m_previewCache = QImage();
    m_previewCacheSignature.clear();
    m_previewCachePixelSize = QSize();
    m_layerPreviewCache.clear();
    const QString currentPrimaryPath = !m_badge.layers.isEmpty() ? m_badge.layers.first().imagePath : m_badge.imagePath;
    if (currentPrimaryPath != m_loadedImagePath) {
        loadImage();
    }
    bool changed = false;
    double px = m_badge.xMm * Constants::kMmToPx;
    double py = m_badge.yMm * Constants::kMmToPx;
    if (qAbs(pos().x() - px) > 0.1 || qAbs(pos().y() - py) > 0.1) {
        const bool snapToGrid = m_snapToGrid;
        m_snapToGrid = false;
        setPos(px, py);
        m_snapToGrid = snapToGrid;
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

QPointF BadgeGraphicItem::snappedPosition(const QPointF& scenePos) const {
    if (!m_snapToGrid || m_gridSpacingMm <= 0.0) {
        return scenePos;
    }

    const double mmToPx = Constants::kMmToPx;
    const double stepPx = m_gridSpacingMm * mmToPx;
    if (stepPx <= 0.0) {
        return scenePos;
    }

    QPointF snapped = scenePos;
    snapped.setX(std::round(snapped.x() / stepPx) * stepPx);
    snapped.setY(std::round(snapped.y() / stepPx) * stepPx);
    return snapped;
}

void BadgeGraphicItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_moveNotifyTimer.restart();
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
        const bool precisionMode = event->modifiers() & Qt::ControlModifier;
        if (!precisionMode) {
            const bool snapToGrid = m_snapToGrid;
            m_snapToGrid = false;
            setPos(snappedPosition(pos()));
            m_snapToGrid = snapToGrid;
        }
        m_dragging = false;
        endInteractiveEdit();
    }
}

QVariant BadgeGraphicItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && m_snapToGrid && m_gridSpacingMm > 0.0 && !m_dragging) {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            return value;
        }
        return snappedPosition(value.toPointF());
    }
    if (change == ItemPositionHasChanged) {
        const double mmToPx = Constants::kMmToPx;
        m_badge.xMm = pos().x() / mmToPx;
        m_badge.yMm = pos().y() / mmToPx;
        if (!m_dragging) {
            emit badgeMoved(this);
        } else if (!m_moveNotifyTimer.isValid() || m_moveNotifyTimer.elapsed() >= 16) {
            m_moveNotifyTimer.restart();
            emit badgeMoved(this);
        }
    }
    if (change == ItemSelectedHasChanged) {
        prepareGeometryChange();
        m_badge.isSelected = value.toBool();
        updateHandles();
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
