#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QPixmap>

class ImageProcessor {
public:
    static QPixmap applyCorrection(const QPixmap& src, double brightness, double contrast, double saturation);
};

#endif
