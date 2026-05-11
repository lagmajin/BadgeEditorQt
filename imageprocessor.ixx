module;

#include <QString>
#include <QImage>
#include <QPixmap>

export module imageprocessor;

export class ImageProcessor {
public:
    static QImage loadImage(const QString& path, QString* colorSpaceLabel = nullptr);
    static QPixmap applyCorrection(const QPixmap& src, double brightness, double contrast, double saturation);
};
