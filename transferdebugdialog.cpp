#include "transferdebugdialog.h"

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QVBoxLayout>
#include <wobjectimpl.h>

namespace {
QWidget* makePane(const QString& title, QLabel** outLabel) {
    auto* wrapper = new QWidget;
    auto* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* header = new QLabel(title, wrapper);
    layout->addWidget(header);

    auto* imageLabel = new QLabel(wrapper);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setAutoFillBackground(true);
    imageLabel->setMinimumSize(320, 320);

    auto* scroll = new QScrollArea(wrapper);
    scroll->setWidgetResizable(true);
    scroll->setWidget(imageLabel);
    layout->addWidget(scroll, 1);

    *outLabel = imageLabel;
    return wrapper;
}
}

TransferDebugDialog::TransferDebugDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("転送デバッグ"));
    resize(900, 560);

    auto* root = new QVBoxLayout(this);
    m_titleLabel = new QLabel(this);
    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    root->addWidget(m_titleLabel);
    root->addWidget(m_detailLabel);

    auto* row = new QHBoxLayout;
    row->addWidget(makePane(QStringLiteral("Designer カット結果"), &m_designerLabel), 1);
    row->addWidget(makePane(QStringLiteral("Layout 入力画像"), &m_layoutLabel), 1);
    root->addLayout(row, 1);
}

W_OBJECT_IMPL(TransferDebugDialog)

void TransferDebugDialog::setImages(const QImage& designerImage,
                                    const QImage& layoutImage,
                                    const QString& titleText,
                                    const QString& detailText) {
    m_titleLabel->setText(titleText);
    m_detailLabel->setText(detailText);
    m_designerLabel->setPixmap(toPreviewPixmap(designerImage));
    m_layoutLabel->setPixmap(toPreviewPixmap(layoutImage));
}

QPixmap TransferDebugDialog::toPreviewPixmap(const QImage& image) {
    if (image.isNull()) {
        QImage fallback(QSize(320, 320), QImage::Format_ARGB32_Premultiplied);
        fallback.fill(QColor(40, 40, 40));
        return QPixmap::fromImage(fallback);
    }
    return QPixmap::fromImage(image);
}
