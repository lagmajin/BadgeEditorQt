#ifndef BADGEITEM_H
#define BADGEITEM_H

#include <QString>
#include <QPointF>
#include <QSizeF>
#include <QJsonObject>
#include <QJsonArray>

struct LayerItem {
    QString imagePath;
    QString name;
    double opacity = 1.0;
    bool visible = true;
    double offsetX = 0.0; // mm offset within badge
    double offsetY = 0.0;
    
    QJsonObject toJson() const;
    static LayerItem fromJson(const QJsonObject& obj);
};

struct BadgeItem {
    double widthMm = 57.0;
    double heightMm = 57.0;
    double xMm = 10.0;
    double yMm = 10.0;
    double rotation = 0.0;
    QString label;
    QString imagePath;
    QString displayText;
    bool clipToCircle = false;
    double brightness = 0.0;
    double contrast = 0.0;
    double saturation = 0.0;
    bool isSelected = false;
    QList<LayerItem> layers;

    QJsonObject toJson() const;
    static BadgeItem fromJson(const QJsonObject& obj);
    
    static QJsonArray toJsonArray(const QList<BadgeItem>& items);
    static QList<BadgeItem> fromJsonArray(const QJsonArray& arr);
};

#endif
