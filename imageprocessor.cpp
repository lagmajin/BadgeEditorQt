#include "imageprocessor.h"
#include <QColorSpace>
#include <QImageReader>
#include <QImage>
#include <cmath>

import badge.imageio;

namespace {

QString describeColorSpace(const QImage& image) {
    const QColorSpace srgb(QColorSpace::SRgb);
    const QColorSpace cs = image.colorSpace();
    if (cs.isValid()) {
        const QString desc = cs.description().trimmed();
        if (!desc.isEmpty()) {
            return desc;
        }
        if (cs == srgb) {
            return QStringLiteral("sRGB");
        }
        return QStringLiteral("埋め込み色空間");
    }
    return QStringLiteral("sRGB前提");
}

QImage normalizeToSrgb(QImage image) {
    if (image.isNull()) {
        return {};
    }

    const QColorSpace srgb(QColorSpace::SRgb);
    const QColorSpace sourceSpace = image.colorSpace();
    if (sourceSpace.isValid() && sourceSpace != srgb) {
        image.convertToColorSpace(srgb);
    } else if (!sourceSpace.isValid()) {
        image.setColorSpace(srgb);
    }

    if (image.colorSpace().isValid() && image.colorSpace() != srgb) {
        image.convertToColorSpace(srgb);
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    image.setColorSpace(srgb);
    return image;
}

QImage loadViaQtReader(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);

    QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    return normalizeToSrgb(std::move(image));
}

QImage loadViaOiio(const QString& path) {
    const auto raw = badge::load_image(path.toUtf8().constData());
    if (!raw) {
        return {};
    }

    const auto& img = *raw;
    if (img.width <= 0 || img.height <= 0 || img.channels <= 0 || img.pixels.empty()) {
        return {};
    }

    QImage out(img.width, img.height, QImage::Format_ARGB32);
    if (out.isNull()) {
        return {};
    }

    for (int y = 0; y < img.height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < img.width; ++x) {
            const int idx = (y * img.width + x) * img.channels;
            const int r = img.pixels[idx + 0];
            const int g = img.channels > 1 ? img.pixels[idx + 1] : r;
            const int b = img.channels > 2 ? img.pixels[idx + 2] : r;
            const int a = img.channels > 3 ? img.pixels[idx + 3] : 255;
            line[x] = qRgba(r, g, b, a);
        }
    }

    return normalizeToSrgb(std::move(out));
}

}

QImage ImageProcessor::loadImage(const QString& path, QString* colorSpaceLabel) {
    if (QImage viaQt = loadViaQtReader(path); !viaQt.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = describeColorSpace(viaQt);
        }
        return viaQt;
    }

    if (QImage viaOiio = loadViaOiio(path); !viaOiio.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = QStringLiteral("OIIO / sRGB前提");
        }
        return viaOiio;
    }

    QImage fallback;
    if (fallback.load(path)) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = describeColorSpace(fallback);
        }
        return normalizeToSrgb(std::move(fallback));
    }
    if (colorSpaceLabel) {
        *colorSpaceLabel = QStringLiteral("読み込み失敗");
    }
    return {};
}

QPixmap ImageProcessor::applyCorrection(const QPixmap& src, double brightness, double contrast, double saturation) {
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    double b = brightness * 2.55;
    double c = contrast / 100.0;
    double s = saturation / 100.0 + 1.0;

    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb pixel = line[x];
            int a = qAlpha(pixel);
            if (a == 0) continue;
            double r = qRed(pixel) * 255.0 / a;
            double g = qGreen(pixel) * 255.0 / a;
            double bl = qBlue(pixel) * 255.0 / a;
            r += b; g += b; bl += b;
            r = (r - 128) * (1.0 + c) + 128;
            g = (g - 128) * (1.0 + c) + 128;
            bl = (bl - 128) * (1.0 + c) + 128;
            if (s != 1.0) {
                double gray = 0.299 * r + 0.587 * g + 0.114 * bl;
                r = gray + s * (r - gray);
                g = gray + s * (g - gray);
                bl = gray + s * (bl - gray);
            }
            r = qBound(0.0, r, 255.0);
            g = qBound(0.0, g, 255.0);
            bl = qBound(0.0, bl, 255.0);
            line[x] = qRgba(int(r * a / 255.0), int(g * a / 255.0), int(bl * a / 255.0), a);
        }
    }
    return QPixmap::fromImage(img);
}
