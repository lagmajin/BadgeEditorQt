#include "badgeitem.h"
#include <QJsonDocument>

QJsonObject BadgeItem::toJson() const {
    return QJsonObject{
        {"widthMm", widthMm}, {"heightMm", heightMm},
        {"xMm", xMm}, {"yMm", yMm}, {"rotation", rotation},
        {"label", label}, {"imagePath", imagePath}, {"displayText", displayText},
        {"clipToCircle", clipToCircle},
        {"brightness", brightness}, {"contrast", contrast}, {"saturation", saturation}
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
