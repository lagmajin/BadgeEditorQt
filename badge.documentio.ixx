module;

#include <optional>
#include "layoutengine.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

export module badge.documentio;

import badge.document;

export namespace badge {

struct JsonDocumentResult {
    DocumentData document;
    bool ok = false;
};

namespace detail {

inline LayerData layerFromJson(const QJsonObject& obj) {
    LayerData layer;
    layer.imagePath = obj["imagePath"].toString().toStdString();
    layer.name = obj["name"].toString().toStdString();
    layer.opacity = obj["opacity"].toDouble(1.0);
    layer.visible = obj["visible"].toBool(true);
    layer.offsetX = obj["offsetX"].toDouble(0.0);
    layer.offsetY = obj["offsetY"].toDouble(0.0);
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
    };
}

inline BadgeData badgeFromJson(const QJsonObject& obj) {
    BadgeData badge;
    badge.widthMm = obj["widthMm"].toDouble(57.0);
    badge.heightMm = obj["heightMm"].toDouble(57.0);
    badge.xMm = obj["xMm"].toDouble(10.0);
    badge.yMm = obj["yMm"].toDouble(10.0);
    badge.rotation = obj["rotation"].toDouble(0.0);
    badge.label = obj["label"].toString().toStdString();
    badge.imagePath = obj["imagePath"].toString().toStdString();
    badge.displayText = obj["displayText"].toString().toStdString();
    badge.clipToCircle = obj["clipToCircle"].toBool(false);
    badge.brightness = obj["brightness"].toDouble(0.0);
    badge.contrast = obj["contrast"].toDouble(0.0);
    badge.saturation = obj["saturation"].toDouble(0.0);
    badge.isSelected = obj["isSelected"].toBool(false);
    const auto layers = obj["layers"].toArray();
    badge.layers.reserve(layers.size());
    for (const auto& value : layers) {
        badge.layers.push_back(layerFromJson(value.toObject()));
    }
    return badge;
}

inline QJsonObject badgeToJson(const BadgeData& badge) {
    QJsonArray layers;
    for (const auto& layer : badge.layers) {
        layers.append(layerToJson(layer));
    }

    return QJsonObject{
        {"widthMm", badge.widthMm},
        {"heightMm", badge.heightMm},
        {"xMm", badge.xMm},
        {"yMm", badge.yMm},
        {"rotation", badge.rotation},
        {"label", QString::fromStdString(badge.label)},
        {"imagePath", QString::fromStdString(badge.imagePath)},
        {"displayText", QString::fromStdString(badge.displayText)},
        {"clipToCircle", badge.clipToCircle},
        {"brightness", badge.brightness},
        {"contrast", badge.contrast},
        {"saturation", badge.saturation},
        {"isSelected", badge.isSelected},
        {"layers", layers},
    };
}

inline PaperConfig paperFromJson(const QJsonObject& obj) {
    PaperConfig paper;
    paper.widthMm = obj["widthMm"].toDouble(210.0);
    paper.heightMm = obj["heightMm"].toDouble(297.0);
    paper.marginMm = obj["marginMm"].toDouble(10.0);
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
        {"paper", detail::paperToJson(document.paper)},
        {"badges", badges},
    };
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

}
