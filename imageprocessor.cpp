#include "imageprocessor.h"
#include <QImage>
#include <cmath>

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
