module;

#include "badge.paper.h"
#include "badge.model.h"

export module badge.document;

import std;
export import badge.layout;

export namespace badge {

struct DocumentData {
    std::string title;
    ProductMode productMode = ProductMode::Badge;
    PaperConfig paper;
    std::vector<BadgeData> badges;
};

}
