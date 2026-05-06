#ifndef BADGEITEM_H
#define BADGEITEM_H

#include <QString>
#include <QPointF>
#include <QSizeF>
#include <QList>
#include <QFileInfo>

struct LayerItem {
    QString imagePath;
    QString name;
    double opacity = 1.0;
    bool visible = true;
    double offsetX = 0.0; // mm offset within badge
    double offsetY = 0.0;
};

inline LayerItem layerFromImagePath(const QString& imagePath) {
    LayerItem layer;
    layer.imagePath = imagePath;
    layer.name = QFileInfo(imagePath).baseName();
    return layer;
}

struct BadgeItem {
    double widthMm = 32.0;
    double heightMm = 32.0;
    double imageScale = 1.0;
    int materialPreset = 0;
    double specularStrength = 0.85;
    double envReflectionStrength = 0.55;
    double glitterStrength = 0.35;
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
    bool flattenedForLayoutTransfer = false;
    bool isSelected = false;
    QList<LayerItem> layers;
};

#endif
