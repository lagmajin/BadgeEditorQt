module;

#include <QColor>
#include <QString>
#include <QList>
#include "badgeitem.h"
#include "badge.model.h"

export module badge.qtbridge;

import std;
export import badge.layout;

namespace badge::qt::detail {

inline void migrateLegacyImageToLayers(QString& imagePath, QList<LayerItem>& layers) {
    if (imagePath.isEmpty()) {
        return;
    }
    layers.prepend(layerFromImagePath(imagePath));
    imagePath.clear();
}

inline badge::ProductMode toCoreProductMode(::ProductMode mode) {
    switch (mode) {
    case ::ProductMode::Sticker:
        return badge::ProductMode::Sticker;
    case ::ProductMode::Badge:
    default:
        return badge::ProductMode::Badge;
    }
}

inline ::ProductMode fromCoreProductMode(badge::ProductMode mode) {
    switch (mode) {
    case badge::ProductMode::Sticker:
        return ::ProductMode::Sticker;
    case badge::ProductMode::Badge:
    default:
        return ::ProductMode::Badge;
    }
}

inline badge::GuideShape toCoreGuideShape(::GuideShape shape) {
    switch (shape) {
    case ::GuideShape::Rectangle:
        return badge::GuideShape::Rectangle;
    case ::GuideShape::RoundedRectangle:
        return badge::GuideShape::RoundedRectangle;
    case ::GuideShape::Oval:
        return badge::GuideShape::Oval;
    case ::GuideShape::Circle:
    default:
        return badge::GuideShape::Circle;
    }
}

inline ::GuideShape fromCoreGuideShape(badge::GuideShape shape) {
    switch (shape) {
    case badge::GuideShape::Rectangle:
        return ::GuideShape::Rectangle;
    case badge::GuideShape::RoundedRectangle:
        return ::GuideShape::RoundedRectangle;
    case badge::GuideShape::Oval:
        return ::GuideShape::Oval;
    case badge::GuideShape::Circle:
    default:
        return ::GuideShape::Circle;
    }
}

}

export namespace badge::qt {

export inline badge::LayerData toCoreLayer(const LayerItem& layer) {
    badge::LayerData core;
    core.imagePath = layer.imagePath.toStdString();
    core.name = layer.name.toStdString();
    core.opacity = layer.opacity;
    core.visible = layer.visible;
    core.offsetX = layer.offsetX;
    core.offsetY = layer.offsetY;
    core.blendMode = layerBlendModeToInt(layer.blendMode);
    core.fillColor = layer.fillColor.isValid() ? layer.fillColor.name(QColor::HexArgb).toStdString() : std::string();
    return core;
}

export inline LayerItem fromCoreLayer(const badge::LayerData& layer) {
    LayerItem qt;
    qt.imagePath = QString::fromStdString(layer.imagePath);
    qt.name = QString::fromStdString(layer.name);
    qt.opacity = layer.opacity;
    qt.visible = layer.visible;
    qt.offsetX = layer.offsetX;
    qt.offsetY = layer.offsetY;
    qt.blendMode = layerBlendModeFromInt(layer.blendMode);
    const QColor parsed(QString::fromStdString(layer.fillColor));
    qt.fillColor = parsed.isValid() ? parsed : QColor();
    return qt;
}

export inline badge::BadgeData toCoreBadge(const BadgeItem& item) {
    badge::BadgeData core;
    core.productMode = detail::toCoreProductMode(item.productMode);
    core.guide.shape = detail::toCoreGuideShape(item.guide.shape);
    core.guide.bleedMm = item.guide.bleedMm;
    core.guide.safeInsetMm = item.guide.safeInsetMm;
    core.guide.cornerRadiusMm = item.guide.cornerRadiusMm;
    core.widthMm = item.widthMm;
    core.heightMm = item.heightMm;
    core.imageScale = item.imageScale;
    core.materialPreset = item.materialPreset;
    core.specularStrength = item.specularStrength;
    core.envReflectionStrength = item.envReflectionStrength;
    core.glitterStrength = item.glitterStrength;
    core.xMm = item.xMm;
    core.yMm = item.yMm;
    core.rotation = item.rotation;
    core.label = item.label.toStdString();
    core.imagePath.clear();
    core.displayText = item.displayText.toStdString();
    core.clipToCircle = item.clipToCircle;
    core.brightness = item.brightness;
    core.contrast = item.contrast;
    core.saturation = item.saturation;
    core.flattenedForLayoutTransfer = item.flattenedForLayoutTransfer;
    core.isSelected = item.isSelected;
    QList<LayerItem> layers = item.layers;
    QString legacyImagePath = item.imagePath;
    detail::migrateLegacyImageToLayers(legacyImagePath, layers);
    core.layers.reserve(static_cast<size_t>(layers.size()));
    for (const auto& layer : layers) {
        core.layers.push_back(toCoreLayer(layer));
    }
    return core;
}

export inline BadgeItem fromCoreBadge(const badge::BadgeData& item) {
    BadgeItem qt;
    qt.productMode = detail::fromCoreProductMode(item.productMode);
    qt.guide.shape = detail::fromCoreGuideShape(item.guide.shape);
    qt.guide.bleedMm = item.guide.bleedMm;
    qt.guide.safeInsetMm = item.guide.safeInsetMm;
    qt.guide.cornerRadiusMm = item.guide.cornerRadiusMm;
    qt.widthMm = item.widthMm;
    qt.heightMm = item.heightMm;
    qt.imageScale = item.imageScale;
    qt.materialPreset = item.materialPreset;
    qt.specularStrength = item.specularStrength;
    qt.envReflectionStrength = item.envReflectionStrength;
    qt.glitterStrength = item.glitterStrength;
    qt.xMm = item.xMm;
    qt.yMm = item.yMm;
    qt.rotation = item.rotation;
    qt.label = QString::fromStdString(item.label);
    qt.imagePath = QString::fromStdString(item.imagePath);
    qt.displayText = QString::fromStdString(item.displayText);
    qt.clipToCircle = item.clipToCircle;
    qt.brightness = item.brightness;
    qt.contrast = item.contrast;
    qt.saturation = item.saturation;
    qt.flattenedForLayoutTransfer = item.flattenedForLayoutTransfer;
    qt.isSelected = item.isSelected;
    qt.layers.reserve(static_cast<int>(item.layers.size() + (item.imagePath.empty() ? 0 : 1)));
    if (!item.imagePath.empty()) {
        qt.layers.prepend(layerFromImagePath(qt.imagePath));
        qt.imagePath.clear();
    }
    for (const auto& layer : item.layers) {
        qt.layers.append(fromCoreLayer(layer));
    }
    return qt;
}

export inline std::vector<badge::BadgeData> toCoreBadges(const QList<BadgeItem>& items) {
    std::vector<badge::BadgeData> out;
    out.reserve(static_cast<size_t>(items.size()));
    for (const auto& item : items) {
        out.push_back(toCoreBadge(item));
    }
    return out;
}

export inline QList<BadgeItem> fromCoreBadges(const std::vector<badge::BadgeData>& items) {
    QList<BadgeItem> out;
    out.reserve(static_cast<qsizetype>(items.size()));
    for (const auto& item : items) {
        out.append(fromCoreBadge(item));
    }
    return out;
}

}
