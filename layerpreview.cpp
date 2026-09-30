#include "layerpreview.h"
#include "imageprocessor.h"
#include "constants.h"

#include <QImage>
#include <QPainter>
#include <QFileInfo>
#include <QString>
#include <cmath>
#include <algorithm>

QRectF fitRectInside(const QRectF& target, const QSizeF& sourceSize) {
    if (target.isEmpty() || sourceSize.width() <= 0.0 || sourceSize.height() <= 0.0) return target;
    const qreal scale = std::min(target.width() / sourceSize.width(), target.height() / sourceSize.height());
    const QSizeF fittedSize(sourceSize.width() * scale, sourceSize.height() * scale);
    return QRectF(QPointF(target.center().x() - fittedSize.width() * 0.5,
                          target.center().y() - fittedSize.height() * 0.5), fittedSize);
}

QRectF badgeContentRectPx(const BadgeItem& badge) {
    const double margin = badge.isSelected ? 3.0 : 2.0;
    return QRectF(margin, margin, badge.widthMm * Constants::kMmToPx, badge.heightMm * Constants::kMmToPx);
}

QRectF badgePrimaryImageRectPx(const BadgeItem& badge, const QSizeF& sourceSize) {
    const QRectF content = badgeContentRectPx(badge);
    const double scale = std::max(0.1, badge.imageScale);
    const QRectF targetRect(content.center().x() - content.width() * scale * 0.5,
                            content.center().y() - content.height() * scale * 0.5,
                            content.width() * scale, content.height() * scale);
    QRectF imageRect = fitRectInside(targetRect, sourceSize);
    if (!badge.layers.isEmpty()) {
        imageRect.translate(badge.layers.first().offsetX * Constants::kMmToPx,
                            badge.layers.first().offsetY * Constants::kMmToPx);
    }
    return imageRect;
}

QPixmap correctedPixmapForBadge(const BadgeItem& badge, const QString& path) {
    if (path.isEmpty()) return {};
    QString colorSpaceLabel;
    const QImage loaded = ImageProcessor::loadImage(path, &colorSpaceLabel);
    if (loaded.isNull()) return {};
    QPixmap pixmap = QPixmap::fromImage(loaded);
    if (badge.brightness != 0.0 || badge.contrast != 0.0 || badge.saturation != 0.0) {
        pixmap = ImageProcessor::applyCorrection(pixmap, badge.brightness, badge.contrast, badge.saturation);
    }
    return pixmap;
}

QPixmap renderedLayerPixmap(const BadgeItem& badge, const LayerItem& layer) {
    return applyLayerFillColor(correctedPixmapForBadge(badge, layer.imagePath), layer.fillColor);
}

QPainter::CompositionMode compositionModeForLayer(LayerBlendMode mode) {
    switch (mode) {
    case LayerBlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case LayerBlendMode::Screen: return QPainter::CompositionMode_Screen;
    case LayerBlendMode::Overlay: return QPainter::CompositionMode_Overlay;
    case LayerBlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
    case LayerBlendMode::Add: return QPainter::CompositionMode_Plus;
    case LayerBlendMode::Normal:
    default: return QPainter::CompositionMode_SourceOver;
    }
}

QString layerBlendModeText(LayerBlendMode mode) {
    switch (mode) {
    case LayerBlendMode::Multiply: return QStringLiteral("Multiply");
    case LayerBlendMode::Screen: return QStringLiteral("Screen");
    case LayerBlendMode::Overlay: return QStringLiteral("Overlay");
    case LayerBlendMode::SoftLight: return QStringLiteral("Soft Light");
    case LayerBlendMode::Add: return QStringLiteral("Add");
    case LayerBlendMode::Normal:
    default: return QStringLiteral("Normal");
    }
}

QString layerItemSummary(const LayerItem& layer) {
    QString summary = QStringLiteral("%1  [%2, %3%]")
        .arg(layer.name.isEmpty() ? QFileInfo(layer.imagePath).baseName() : layer.name,
             layerBlendModeText(layer.blendMode),
             QString::number(int(std::round(std::clamp(layer.opacity, 0.0, 1.0) * 100.0))));
    if (layer.fillColor.isValid()) summary += QStringLiteral(" %1").arg(layer.fillColor.name(QColor::HexArgb).toUpper());
    return summary;
}

QPixmap applyLayerFillColor(QPixmap pixmap, const QColor& fillColor) {
    if (pixmap.isNull() || !fillColor.isValid()) return pixmap;
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return pixmap;
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), fillColor);
    painter.end();
    return QPixmap::fromImage(image);
}
