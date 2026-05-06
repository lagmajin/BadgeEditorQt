module;

#include <algorithm>
#include <string>
#include <vector>

export module badge.model;

export namespace badge {

struct LayerData {
    std::string imagePath;
    std::string name;
    double opacity = 1.0;
    bool visible = true;
    double offsetX = 0.0;
    double offsetY = 0.0;
};

struct BadgeData {
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
