module;

#include <optional>
#include <string>
#include <vector>
#include "layoutengine.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QString>

export module badge.documentio;

import badge.document;

export namespace badge {

struct JsonDocumentResult {
    DocumentData document;
    bool ok = false;
};

namespace detail {

inline ProductMode productModeFromJsonValue(const QJsonValue& value) {
    if (value.isDouble()) {
        return value.toInt(0) == 1 ? ProductMode::Sticker : ProductMode::Badge;
    }

    const QString text = value.toString(QStringLiteral("badge")).toLower();
    if (text == QStringLiteral("sticker")) {
        return ProductMode::Sticker;
    }
    return ProductMode::Badge;
}

inline QString productModeToJsonValue(ProductMode mode) {
    switch (mode) {
    case ProductMode::Sticker:
        return QStringLiteral("sticker");
    case ProductMode::Badge:
    default:
        return QStringLiteral("badge");
    }
}

inline GuideShape guideShapeFromJsonValue(const QJsonValue& value) {
    if (value.isDouble()) {
        switch (value.toInt(0)) {
        case 1:
            return GuideShape::Rectangle;
        case 2:
            return GuideShape::RoundedRectangle;
        case 3:
            return GuideShape::Oval;
        case 0:
        default:
            return GuideShape::Circle;
        }
    }

    const QString text = value.toString(QStringLiteral("circle")).toLower();
    if (text == QStringLiteral("rectangle")) {
        return GuideShape::Rectangle;
    }
    if (text == QStringLiteral("roundedrectangle") || text == QStringLiteral("rounded-rectangle") || text == QStringLiteral("rounded_rectangle")) {
        return GuideShape::RoundedRectangle;
    }
    if (text == QStringLiteral("oval")) {
        return GuideShape::Oval;
    }
    return GuideShape::Circle;
}

inline QString guideShapeToJsonValue(GuideShape shape) {
    switch (shape) {
    case GuideShape::Rectangle:
        return QStringLiteral("rectangle");
    case GuideShape::RoundedRectangle:
        return QStringLiteral("roundedRectangle");
    case GuideShape::Oval:
        return QStringLiteral("oval");
    case GuideShape::Circle:
    default:
        return QStringLiteral("circle");
    }
}

inline GuideData guideFromJson(const QJsonObject& obj, bool clipToCircle) {
    GuideData guide;
    guide.shape = clipToCircle ? GuideShape::Circle : GuideShape::Rectangle;
    if (obj.isEmpty()) {
        return guide;
    }

    guide.shape = guideShapeFromJsonValue(obj["shape"]);
    guide.bleedMm = obj["bleedMm"].toDouble(3.0);
    guide.safeInsetMm = obj["safeInsetMm"].toDouble(2.0);
    guide.cornerRadiusMm = obj["cornerRadiusMm"].toDouble(3.0);
    return guide;
}

inline QJsonObject guideToJson(const GuideData& guide) {
    return QJsonObject{
        {"shape", guideShapeToJsonValue(guide.shape)},
        {"bleedMm", guide.bleedMm},
        {"safeInsetMm", guide.safeInsetMm},
        {"cornerRadiusMm", guide.cornerRadiusMm},
    };
}

inline void migrateLegacyImageToLayers(std::string& imagePath, std::vector<LayerData>& layers) {
    if (imagePath.empty()) {
        return;
    }
    LayerData layer;
    layer.imagePath = imagePath;
    layer.name = QFileInfo(QString::fromStdString(imagePath)).baseName().toStdString();
    layers.insert(layers.begin(), layer);
    imagePath.clear();
}

inline LayerData layerFromJson(const QJsonObject& obj) {
    LayerData layer;
    layer.imagePath = obj["imagePath"].toString().toStdString();
    layer.name = obj["name"].toString().toStdString();
    layer.opacity = obj["opacity"].toDouble(1.0);
    layer.visible = obj["visible"].toBool(true);
    layer.offsetX = obj["offsetX"].toDouble(0.0);
    layer.offsetY = obj["offsetY"].toDouble(0.0);
    layer.blendMode = obj["blendMode"].toInt(0);
    return layer;
}

inline QJsonObject layerToJson(const LayerData& layer) {
    return QJsonObject{
        {"imagePath", QString::fromStdString(layer.imagePath)},
        {"name", QString::fromStdString(layer.name)},
        {"opacity", layer.opacity},
        {"visible", layer.visible},
        {"offsetX", layer.offsetX},
        {"offsetY", layer.offsetY},
        {"blendMode", layer.blendMode},
    };
}

inline BadgeData badgeFromJson(const QJsonObject& obj) {
    BadgeData badge;
    badge.productMode = productModeFromJsonValue(obj["productMode"]);
    badge.widthMm = obj["widthMm"].toDouble(32.0);
    badge.heightMm = obj["heightMm"].toDouble(32.0);
    badge.imageScale = obj["imageScale"].toDouble(1.0);
    badge.materialPreset = obj["materialPreset"].toInt(0);
    badge.specularStrength = obj["specularStrength"].toDouble(0.85);
    badge.envReflectionStrength = obj["envReflectionStrength"].toDouble(0.55);
    badge.glitterStrength = obj["glitterStrength"].toDouble(0.35);
    badge.xMm = obj["xMm"].toDouble(10.0);
    badge.yMm = obj["yMm"].toDouble(10.0);
    badge.rotation = obj["rotation"].toDouble(0.0);
    badge.label = obj["label"].toString().toStdString();
    badge.imagePath = obj["imagePath"].toString().toStdString();
    badge.displayText = obj["displayText"].toString().toStdString();
    badge.clipToCircle = obj["clipToCircle"].toBool(false);
    badge.guide = guideFromJson(obj["guide"].toObject(), badge.clipToCircle);
    badge.brightness = obj["brightness"].toDouble(0.0);
    badge.contrast = obj["contrast"].toDouble(0.0);
    badge.saturation = obj["saturation"].toDouble(0.0);
    badge.flattenedForLayoutTransfer = obj["flattenedForLayoutTransfer"].toBool(false);
    badge.isSelected = obj["isSelected"].toBool(false);
    const auto layers = obj["layers"].toArray();
    badge.layers.reserve(layers.size());
    for (const auto& value : layers) {
        badge.layers.push_back(layerFromJson(value.toObject()));
    }
    migrateLegacyImageToLayers(badge.imagePath, badge.layers);
    return badge;
}

inline QJsonObject badgeToJson(const BadgeData& badge) {
    std::vector<LayerData> layers = badge.layers;
    std::string legacyImagePath = badge.imagePath;
    migrateLegacyImageToLayers(legacyImagePath, layers);

    QJsonArray layerArray;
    for (const auto& layer : layers) {
        layerArray.append(layerToJson(layer));
    }

    return QJsonObject{
        {"productMode", productModeToJsonValue(badge.productMode)},
        {"guide", guideToJson(badge.guide)},
        {"widthMm", badge.widthMm},
        {"heightMm", badge.heightMm},
        {"imageScale", badge.imageScale},
        {"materialPreset", badge.materialPreset},
        {"specularStrength", badge.specularStrength},
        {"envReflectionStrength", badge.envReflectionStrength},
        {"glitterStrength", badge.glitterStrength},
        {"xMm", badge.xMm},
        {"yMm", badge.yMm},
        {"rotation", badge.rotation},
        {"label", QString::fromStdString(badge.label)},
        {"imagePath", QString()},
        {"displayText", QString::fromStdString(badge.displayText)},
        {"clipToCircle", badge.clipToCircle},
        {"brightness", badge.brightness},
        {"contrast", badge.contrast},
        {"saturation", badge.saturation},
        {"flattenedForLayoutTransfer", badge.flattenedForLayoutTransfer},
        {"isSelected", badge.isSelected},
        {"layers", layerArray},
    };
}

inline PaperConfig paperFromJson(const QJsonObject& obj) {
    PaperConfig paper;
    paper.widthMm = obj["widthMm"].toDouble(210.0);
    paper.heightMm = obj["heightMm"].toDouble(297.0);
    paper.marginMm = obj["marginMm"].toDouble(5.0);
    paper.spacingMm = obj["spacingMm"].toDouble(1.0);
    return paper;
}

inline QJsonObject paperToJson(const PaperConfig& paper) {
    return QJsonObject{
        {"widthMm", paper.widthMm},
        {"heightMm", paper.heightMm},
        {"marginMm", paper.marginMm},
        {"spacingMm", paper.spacingMm},
    };
}

}

export inline JsonDocumentResult loadDocumentFromJson(const QByteArray& json) {
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    JsonDocumentResult result;

    if (doc.isArray()) {
        result.document.title = "";
        result.document.paper = PaperConfig{};
        const auto arr = doc.array();
        result.document.badges.reserve(arr.size());
        for (const auto& value : arr) {
            result.document.badges.push_back(detail::badgeFromJson(value.toObject()));
        }
        result.ok = true;
        return result;
    }

    if (!doc.isObject()) {
        return result;
    }

    const QJsonObject obj = doc.object();
    result.document.title = obj["title"].toString().toStdString();
    result.document.productMode = detail::productModeFromJsonValue(obj["productMode"]);
    result.document.paper = detail::paperFromJson(obj["paper"].toObject());
    const auto arr = obj["badges"].toArray();
    result.document.badges.reserve(arr.size());
    for (const auto& value : arr) {
        result.document.badges.push_back(detail::badgeFromJson(value.toObject()));
    }
    result.ok = true;
    return result;
}

export inline QByteArray saveDocumentToJson(const DocumentData& document) {
    QJsonArray badges;
    for (const auto& badge : document.badges) {
        badges.append(detail::badgeToJson(badge));
    }

    const QJsonObject obj{
        {"title", QString::fromStdString(document.title)},
        {"productMode", detail::productModeToJsonValue(document.productMode)},
        {"paper", detail::paperToJson(document.paper)},
        {"badges", badges},
    };
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

}
