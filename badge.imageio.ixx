module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OpenImageIO/imageio.h>

export module badge.imageio;

export namespace badge {

struct RawImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;
};

export std::optional<RawImage> load_image(const std::string& path) {
    using namespace OIIO;

    auto input = std::unique_ptr<ImageInput>(ImageInput::open(path));
    if (!input) {
        return std::nullopt;
    }

    const ImageSpec& spec = input->spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
        return std::nullopt;
    }

    RawImage image;
    image.width = spec.width;
    image.height = spec.height;
    image.channels = spec.nchannels;
    image.pixels.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * static_cast<size_t>(image.channels));

    if (!input->read_image(0, 0, 0, image.channels, TypeDesc::UINT8, image.pixels.data())) {
        return std::nullopt;
    }

    return image;
}

}
