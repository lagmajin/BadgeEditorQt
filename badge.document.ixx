module;

#include "layoutengine.h"

#include <string>
#include <vector>

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
