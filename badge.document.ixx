module;

#include <string>
#include <vector>
#include "layoutengine.h"

export module badge.document;

export import badge.paper;
export import badge.model;
export import badge.layout;

export namespace badge {

struct DocumentData {
    std::string title;
    ProductMode productMode = ProductMode::Badge;
    PaperConfig paper;
    std::vector<BadgeData> badges;
};

}
