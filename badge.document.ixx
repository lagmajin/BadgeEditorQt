module;

#include "badge.paper.h"
#include "badge.model.h"

#include <string>
#include <vector>

export module badge.document;

export import badge.layout;

export namespace badge {

struct DocumentData {
    std::string title;
    ProductMode productMode = ProductMode::Badge;
    PaperConfig paper;
    std::vector<BadgeData> badges;
};

}
