#ifndef BADGE_MODEL_H
#define BADGE_MODEL_H

#include <string>
#include <vector>

namespace badge {

enum class ProductMode {
    Badge = 0,
    Sticker = 1,
};

enum class GuideShape {
    Circle = 0,
    Rectangle = 1,
    RoundedRectangle = 2,
    Oval = 3,
};

struct GuideData {
    GuideShape shape = GuideShape::Circle;
    double bleedMm = 3.0;
    double safeInsetMm = 2.0;
    double cornerRadiusMm = 3.0;
};

struct LayerData {
    std::string imagePath;
    std::string name;
    double opacity = 1.0;
    bool visible = true;
    double offsetX = 0.0;
    double offsetY = 0.0;
    int blendMode = 0;
    std::string fillColor;
};

struct BadgeData {
    ProductMode productMode = ProductMode::Badge;
    GuideData guide;
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
    std::string label;
    std::string imagePath;
    std::string displayText;
    bool clipToCircle = false;
    double brightness = 0.0;
    double contrast = 0.0;
    double saturation = 0.0;
    bool flattenedForLayoutTransfer = false;
    bool isSelected = false;
    std::vector<LayerData> layers;
};

}

#endif
