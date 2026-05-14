#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QImage>
#include <QPixmap>
#include <QString>

class ImageProcessor {
public:
    static QImage loadImage(const QString& path, QString* colorSpaceLabel = nullptr);
    static QPixmap applyCorrection(const QPixmap& src, double brightness, double contrast, double saturation);
};

#endif
