#ifndef TRANSFERDEBUGDIALOG_H
#define TRANSFERDEBUGDIALOG_H

#include <QDialog>
#include <QImage>
#include <wobjectdefs.h>

class QLabel;

class TransferDebugDialog : public QDialog {
    W_OBJECT(TransferDebugDialog)
public:
    explicit TransferDebugDialog(QWidget* parent = nullptr);

    void setImages(const QImage& designerImage,
                   const QImage& layoutImage,
                   const QString& titleText,
                   const QString& detailText);

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QLabel* m_designerLabel = nullptr;
    QLabel* m_layoutLabel = nullptr;

    static QPixmap toPreviewPixmap(const QImage& image);
};

#endif
