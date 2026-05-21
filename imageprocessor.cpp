#include "imageprocessor.h"
#include "constants.h"
#include <QBuffer>
#include <QFile>
#include <QColorSpace>
#include <QImageReader>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <string>

#ifdef BADGEEDITOR_HAS_QTSVG
#include <QSvgRenderer>
#endif

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#endif

import badge.imageio;

namespace {

#ifdef Q_OS_WIN
struct ScopedComInit {
    HRESULT hr = E_FAIL;

    ScopedComInit() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

    ~ScopedComInit() {
        if (hr == S_OK || hr == S_FALSE) {
            CoUninitialize();
        }
    }

    bool ok() const {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};

template <typename T>
void releaseCom(T*& ptr) {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}
#endif

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

QImage loadViaQtBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return {};
    }

    // Try a device-backed reader first so Qt can inspect the encoded stream directly.
    QBuffer buffer;
    buffer.setData(bytes);
    if (buffer.open(QIODevice::ReadOnly)) {
        QImageReader reader(&buffer);
        reader.setAutoTransform(true);
        reader.setDecideFormatFromContent(true);
        QImage image = reader.read();
        if (!image.isNull()) {
            return normalizeToSrgb(std::move(image));
        }
    }

    // Fall back to raw byte decoding. Some PNGs decode here even when file/path-based
    // readers reject them.
    QImage image = QImage::fromData(bytes);
    if (!image.isNull()) {
        return normalizeToSrgb(std::move(image));
    }

    if (path.endsWith(".png", Qt::CaseInsensitive)) {
        image = QImage::fromData(bytes, "PNG");
        if (!image.isNull()) {
            return normalizeToSrgb(std::move(image));
        }
    }

    return {};
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

#ifdef Q_OS_WIN
QImage loadViaWic(const QString& path) {
    ScopedComInit com;
    if (!com.ok()) {
        return {};
    }

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* source = nullptr;

    QImage result;

    do {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            break;
        }

        const std::wstring widePath = path.toStdWString();
        hr = factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) {
            break;
        }

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) {
            break;
        }

        UINT width = 0;
        UINT height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0) {
            break;
        }

        hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &source);
        if (FAILED(hr) || !source) {
            break;
        }

        result = QImage(static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32);
        if (result.isNull()) {
            break;
        }

        const UINT stride = static_cast<UINT>(result.bytesPerLine());
        const UINT bufferSize = stride * height;
        hr = source->CopyPixels(nullptr, stride, bufferSize, result.bits());
        if (FAILED(hr)) {
            result = {};
            break;
        }
    } while (false);

    releaseCom(source);
    releaseCom(frame);
    releaseCom(decoder);
    releaseCom(factory);

    if (result.isNull()) {
        return {};
    }
    return normalizeToSrgb(std::move(result));
}
#endif

}

QImage ImageProcessor::loadImage(const QString& path, QString* colorSpaceLabel) {
    const int dot = path.lastIndexOf(QLatin1Char('.'));
    const QString suffix = dot >= 0 ? path.mid(dot + 1).toLower() : QString();
#ifdef BADGEEDITOR_HAS_QTSVG
    if (suffix == "svg" || suffix == "svgz") {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QSize size = renderer.defaultSize();
            if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
                const QSize viewBox = renderer.viewBoxF().size().toSize();
                if (viewBox.isValid() && viewBox.width() > 0 && viewBox.height() > 0) {
                    size = viewBox;
                }
            }
            if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
                size = QSize(Constants::kDefaultSvgSize, Constants::kDefaultSvgSize);
            }

            const int maxSide = Constants::kMaxSvgSide;
            if (size.width() > maxSide || size.height() > maxSide) {
                const double scale = std::min(double(maxSide) / double(size.width()),
                                              double(maxSide) / double(size.height()));
                size = QSize(std::max(1, int(size.width() * scale)),
                             std::max(1, int(size.height() * scale)));
            }

            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            renderer.render(&painter);
            painter.end();
            image = normalizeToSrgb(std::move(image));
            if (!image.isNull()) {
                if (colorSpaceLabel) {
                    *colorSpaceLabel = QStringLiteral("SVG / sRGB");
                }
                return image;
            }
        }
    }
#else
    if (suffix == "svg" || suffix == "svgz") {
        if (colorSpaceLabel) {
            *colorSpaceLabel = QStringLiteral("SVG未対応");
        }
    }
#endif

#ifdef Q_OS_WIN
    if (QImage viaWic = loadViaWic(path); !viaWic.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = QStringLiteral("WIC / sRGB前提");
        }
        return viaWic;
    }
#endif

    if (QImage viaQt = loadViaQtReader(path); !viaQt.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = describeColorSpace(viaQt);
        }
        return viaQt;
    }

    if (QImage viaQtBytes = loadViaQtBytes(path); !viaQtBytes.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = describeColorSpace(viaQtBytes);
        }
        return viaQtBytes;
    }

#ifdef BADGEEDITOR_ENABLE_OIIO
    // Keep OIIO as a last resort. It is useful for some uncommon formats, but
    // Qt/WIC are much more stable for the typical image files this app handles.
    if (QImage viaOiio = loadViaOiio(path); !viaOiio.isNull()) {
        if (colorSpaceLabel) {
            *colorSpaceLabel = QStringLiteral("OIIO / sRGB前提");
        }
        return viaOiio;
    }
#endif

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
