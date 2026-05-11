module;

#include <QString>
#include <QPointF>
#include <QSizeF>
#include <QList>
#include <QFileInfo>

export module badgeitem;

export enum class LayerBlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    SoftLight = 4,
    Add = 5,
};

export inline LayerBlendMode layerBlendModeFromInt(int mode) {
    switch (mode) {
    case 1: return LayerBlendMode::Multiply;
    case 2: return LayerBlendMode::Screen;
    case 3: return LayerBlendMode::Overlay;
    case 4: return LayerBlendMode::SoftLight;
    case 5: return LayerBlendMode::Add;
    default: return LayerBlendMode::Normal;
    }
}

export inline int layerBlendModeToInt(LayerBlendMode mode) {
    return static_cast<int>(mode);
}

export struct LayerItem {
    QString imagePath;
    QString name;
    double opacity = 1.0;
    bool visible = true;
    double offsetX = 0.0; // mm offset within badge
    double offsetY = 0.0;
    LayerBlendMode blendMode = LayerBlendMode::Normal;
};

export inline LayerItem layerFromImagePath(const QString& imagePath) {
    LayerItem layer;
    layer.imagePath = imagePath;
    layer.name = QFileInfo(imagePath).baseName();
    return layer;
}

export enum class ProductMode {
    Badge = 0,
    Sticker = 1,
};

export enum class GuideShape {
    Circle = 0,
    Rectangle = 1,
    RoundedRectangle = 2,
    Oval = 3,
};

export struct GuideItemData {
    GuideShape shape = GuideShape::Circle;
    double bleedMm = 3.0;
    double safeInsetMm = 2.0;
    double cornerRadiusMm = 3.0;
};

export struct BadgeItem {
    ProductMode productMode = ProductMode::Badge;
    GuideItemData guide;
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
