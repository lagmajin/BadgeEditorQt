module;

#include <QString>
#include <QList>
#include <vector>

#include "layoutengine.h"

export module badge.qtbridge;

export import badge.model;
export import badge.layout;

namespace badge::qt::detail {

inline void migrateLegacyImageToLayers(QString& imagePath, QList<LayerItem>& layers) {
    if (imagePath.isEmpty()) {
        return;
    }
    layers.prepend(layerFromImagePath(imagePath));
    imagePath.clear();
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
    return qt;
}

export inline badge::BadgeData toCoreBadge(const BadgeItem& item) {
    badge::BadgeData core;
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
