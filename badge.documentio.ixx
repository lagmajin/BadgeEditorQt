module;

export module badge.documentio;
import std;

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QFileInfo>
#include <QString>
#include <cmath>
#include "badge.model.h"

import badge.document;

export namespace badge {

struct JsonDocumentResult {
    DocumentData document;
    bool ok = false;
    QString errorMessage;
};

namespace detail {

constexpr int maxBadges = 10000;
constexpr int maxLayers = 1000;

inline bool validNumber(const QJsonObject& obj, const char* key, double minimum, double maximum) {
    const QJsonValue value = obj[key];
    if (value.isUndefined()) {
        return true;
    }
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= minimum && value.toDouble() <= maximum;
}

inline bool validOptionalObject(const QJsonObject& obj, const char* key) {
    return obj[key].isUndefined() || obj[key].isObject();
}

inline bool validateLayer(const QJsonValue& value) {
    if (!value.isObject()) {
        return false;
    }
    const auto obj = value.toObject();
    return validNumber(obj, "opacity", 0.0, 1.0)
        && validNumber(obj, "offsetX", -100000.0, 100000.0)
        && validNumber(obj, "offsetY", -100000.0, 100000.0);
}

inline bool validateBadge(const QJsonValue& value) {
    if (!value.isObject()) {
        return false;
    }
    const auto obj = value.toObject();
    if (!validNumber(obj, "widthMm", 0.1, 1000.0)
        || !validNumber(obj, "heightMm", 0.1, 1000.0)
        || !validNumber(obj, "imageScale", 0.001, 100.0)
        || !validNumber(obj, "specularStrength", 0.0, 1.0)
        || !validNumber(obj, "envReflectionStrength", 0.0, 1.0)
        || !validNumber(obj, "glitterStrength", 0.0, 1.0)
        || !validNumber(obj, "xMm", -100000.0, 100000.0)
        || !validNumber(obj, "yMm", -100000.0, 100000.0)
        || !validNumber(obj, "rotation", -36000.0, 36000.0)
        || !validNumber(obj, "brightness", -1.0, 1.0)
        || !validNumber(obj, "contrast", -1.0, 1.0)
        || !validNumber(obj, "saturation", -1.0, 1.0)
        || !validOptionalObject(obj, "guide")) {
        return false;
    }
    if (obj["guide"].isObject()) {
        const auto guide = obj["guide"].toObject();
        if (!validNumber(guide, "bleedMm", 0.0, 1000.0)
            || !validNumber(guide, "safeInsetMm", 0.0, 1000.0)
            || !validNumber(guide, "cornerRadiusMm", 0.0, 1000.0)) {
            return false;
        }
    }
    const QJsonValue layersValue = obj["layers"];
    if (layersValue.isUndefined()) {
        return true;
    }
    if (!layersValue.isArray() || layersValue.toArray().size() > maxLayers) {
        return false;
    }
    for (const auto& layer : layersValue.toArray()) {
        if (!validateLayer(layer)) {
            return false;
        }
    }
    return true;
}

inline bool validatePaper(const QJsonValue& value) {
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const auto obj = value.toObject();
    return validNumber(obj, "widthMm", 1.0, 10000.0)
        && validNumber(obj, "heightMm", 1.0, 10000.0)
        && validNumber(obj, "marginMm", 0.0, 1000.0)
        && validNumber(obj, "spacingMm", 0.0, 1000.0);
}

inline bool validateBadges(const QJsonValue& value) {
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isArray() || value.toArray().size() > maxBadges) {
        return false;
    }
    for (const auto& badge : value.toArray()) {
        if (!validateBadge(badge)) {
            return false;
        }
    }
    return true;
}

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
    layer.fillColor = obj["fillColor"].toString().toStdString();
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
        {"fillColor", QString::fromStdString(layer.fillColor)},
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
    JsonDocumentResult result;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        result.errorMessage = QStringLiteral("JSON の解析に失敗しました: %1 (offset %2)")
                                  .arg(parseError.errorString())
                                  .arg(parseError.offset);
        return result;
    }

    if (doc.isArray()) {
        if (doc.array().size() > detail::maxBadges) {
            result.errorMessage = QStringLiteral("JSON 文書のバッジ数が上限を超えています");
            return result;
        }
        result.document.title = "";
        result.document.paper = PaperConfig{};
        const auto arr = doc.array();
        result.document.badges.reserve(arr.size());
        for (const auto& value : arr) {
            if (!detail::validateBadge(value)) {
                result.errorMessage = QStringLiteral("JSON 文書のバッジまたはレイヤーの形式が不正です");
                return result;
            }
            result.document.badges.push_back(detail::badgeFromJson(value.toObject()));
        }
        result.ok = true;
        return result;
    }

    if (!doc.isObject()) {
        result.errorMessage = QStringLiteral("JSON 文書の形式が未対応です");
        return result;
    }

    const QJsonObject obj = doc.object();
    if (!detail::validatePaper(obj["paper"]) || !detail::validateBadges(obj["badges"])) {
        result.errorMessage = QStringLiteral("JSON 文書の用紙、バッジ、またはレイヤーの形式が不正です");
        return result;
    }
    result.document.title = obj["title"].toString().toStdString();
    result.document.productMode = detail::productModeFromJsonValue(obj["productMode"]);
    result.document.paper = detail::paperFromJson(obj["paper"].toObject());
    const auto arr = obj["badges"].toArray();
    result.document.badges.reserve(arr.size());
    for (const auto& value : arr) {
        if (!detail::validateBadge(value)) {
            result.errorMessage = QStringLiteral("JSON 文書のバッジまたはレイヤーの形式が不正です");
            return result;
        }
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
