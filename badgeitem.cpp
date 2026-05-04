#include "badgeitem.h"
#include <QJsonDocument>

QJsonObject LayerItem::toJson() const {
    return QJsonObject{{"imagePath", imagePath}, {"name", name}, {"opacity", opacity}, {"visible", visible}, {"offsetX", offsetX}, {"offsetY", offsetY}};
}

LayerItem LayerItem::fromJson(const QJsonObject& obj) {
    LayerItem l;
    l.imagePath = obj["imagePath"].toString();
    l.name = obj["name"].toString();
    l.opacity = obj["opacity"].toDouble(1.0);
    l.visible = obj["visible"].toBool(true);
    l.offsetX = obj["offsetX"].toDouble(0.0);
    l.offsetY = obj["offsetY"].toDouble(0.0);
    return l;
}

QJsonObject BadgeItem::toJson() const {
    QJsonArray layerArr;
    for (const auto& l : layers) layerArr.append(l.toJson());
    return QJsonObject{
        {"widthMm", widthMm}, {"heightMm", heightMm},
        {"xMm", xMm}, {"yMm", yMm}, {"rotation", rotation},
        {"label", label}, {"imagePath", imagePath}, {"displayText", displayText},
        {"clipToCircle", clipToCircle},
        {"brightness", brightness}, {"contrast", contrast}, {"saturation", saturation},
        {"layers", layerArr}
    };
}

BadgeItem BadgeItem::fromJson(const QJsonObject& obj) {
    BadgeItem b;
    b.widthMm = obj["widthMm"].toDouble(32.0);
    b.heightMm = obj["heightMm"].toDouble(32.0);
    b.xMm = obj["xMm"].toDouble(10.0);
    b.yMm = obj["yMm"].toDouble(10.0);
    b.rotation = obj["rotation"].toDouble(0.0);
    b.label = obj["label"].toString();
    b.imagePath = obj["imagePath"].toString();
    b.displayText = obj["displayText"].toString();
    b.clipToCircle = obj["clipToCircle"].toBool(true);
    b.brightness = obj["brightness"].toDouble(0.0);
    b.contrast = obj["contrast"].toDouble(0.0);
    b.saturation = obj["saturation"].toDouble(0.0);
    for (const auto& v : obj["layers"].toArray())
        b.layers.append(LayerItem::fromJson(v.toObject()));
    return b;
}

QJsonArray BadgeItem::toJsonArray(const QList<BadgeItem>& items) {
    QJsonArray arr;
    for (const auto& b : items) arr.append(b.toJson());
    return arr;
}

QList<BadgeItem> BadgeItem::fromJsonArray(const QJsonArray& arr) {
    QList<BadgeItem> list;
    for (const auto& v : arr) list.append(fromJson(v.toObject()));
    return list;
}
