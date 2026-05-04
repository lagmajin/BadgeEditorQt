#ifndef BADGEITEM_H
#define BADGEITEM_H

#include <QString>
#include <QPointF>
#include <QSizeF>
#include <QJsonObject>
#include <QJsonArray>

struct BadgeItem {
    double widthMm = 32.0;
    double heightMm = 32.0;
    double xMm = 10.0;
    double yMm = 10.0;
    double rotation = 0.0;
    QString label;
    QString imagePath;
    QString displayText;
    bool clipToCircle = true;
    double brightness = 0.0;
    double contrast = 0.0;
    double saturation = 0.0;
    bool isSelected = false;

    QJsonObject toJson() const;
    static BadgeItem fromJson(const QJsonObject& obj);
    
    static QJsonArray toJsonArray(const QList<BadgeItem>& items);
    static QList<BadgeItem> fromJsonArray(const QJsonArray& arr);
};

#endif
