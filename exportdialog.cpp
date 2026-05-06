#include "exportdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QPalette>

namespace {

QString pdfPresetHelpText(int index) {
    switch (index) {
    case 1:
        return QStringLiteral("印刷所向けの設定です。高めの DPI と CMYK を使います。");
    case 2:
        return QStringLiteral("白黒印刷や確認用です。色は落として扱います。");
    default:
        return QStringLiteral("通常のPDF出力です。まずはこれを選べば大丈夫です。");
    }
}

QString pdfColorModelHelpText(int index) {
    switch (index) {
    case 1:
        return QStringLiteral("CMYK を指定します。入稿先の指定があるとき向けです。");
    case 2:
        return QStringLiteral("グレースケールで出力します。確認用途に向きます。");
    default:
        return QStringLiteral("RGB で出力します。画面の見え方に近い出力です。");
    }
}

void updatePdfHelpLabel(QLabel* label, int presetIndex, int colorModelIndex, int dpi) {
    if (!label) {
        return;
    }
    label->setText(QStringLiteral("%1\n%2\n現在の解像度: %3 DPI")
                       .arg(pdfPresetHelpText(presetIndex),
                            pdfColorModelHelpText(colorModelIndex),
                            QString::number(dpi)));
}

}

ExportDialog::ExportDialog(Format format, const QString& defaultPath, QWidget* parent)
    : QDialog(parent), m_format(format) {
    setWindowTitle(format == Format::Pdf ? "PDF出力設定" : "画像出力設定");
    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(
        format == Format::Pdf
            ? "用紙サイズは mm 基準で出力します。PDF は印刷向けの高解像度出力です。"
            : "PNG はラスタ画像として出力します。解像度を上げると書き出しサイズも増えます。");
    info->setWordWrap(true);
    root->addWidget(info);

    auto* fileGroup = new QGroupBox("保存先");
    auto* fileForm = new QFormLayout(fileGroup);
    auto* pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit(defaultPath);
    auto* browseBtn = new QPushButton("参照...");
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString filter = (m_format == Format::Pdf) ? "PDF (*.pdf)" : "PNG (*.png)";
        const QString selected = QFileDialog::getSaveFileName(this, windowTitle(), m_pathEdit->text(), filter);
        if (!selected.isEmpty()) {
            m_pathEdit->setText(selected);
        }
    });
    pathRow->addWidget(m_pathEdit);
    pathRow->addWidget(browseBtn);
    fileForm->addRow("ファイル:", pathRow);
    root->addWidget(fileGroup);

    auto* optGroup = new QGroupBox("出力設定");
    auto* optForm = new QFormLayout(optGroup);
    m_dpiSpin = new QSpinBox;
    m_dpiSpin->setRange(72, 1200);
    m_dpiSpin->setValue(300);
    m_dpiSpin->setSuffix(" DPI");
    optForm->addRow("解像度:", m_dpiSpin);
    if (m_format == Format::Png) {
        m_whiteBackground = new QCheckBox("白背景で出力");
        m_whiteBackground->setChecked(true);
        optForm->addRow(m_whiteBackground);
    } else {
        m_pdfPreset = new QComboBox;
        m_pdfPreset->addItems({"標準", "印刷向け", "グレースケール"});
        optForm->addRow("プリセット:", m_pdfPreset);

        m_pdfColorModel = new QComboBox;
        m_pdfColorModel->addItems({"RGB", "CMYK", "グレースケール"});
        m_pdfColorModel->setCurrentIndex(0);
        optForm->addRow("色モード:", m_pdfColorModel);

        m_pdfPresetHelp = new QLabel;
        m_pdfPresetHelp->setWordWrap(true);
        auto helpPalette = m_pdfPresetHelp->palette();
        helpPalette.setColor(QPalette::WindowText, helpPalette.color(QPalette::Mid));
        helpPalette.setColor(QPalette::Text, helpPalette.color(QPalette::Mid));
        m_pdfPresetHelp->setPalette(helpPalette);
        optForm->addRow(QString(), m_pdfPresetHelp);

        auto refreshHelp = [this]() {
            updatePdfHelpLabel(m_pdfPresetHelp,
                               m_pdfPreset ? m_pdfPreset->currentIndex() : 0,
                               m_pdfColorModel ? m_pdfColorModel->currentIndex() : 0,
                               m_dpiSpin ? m_dpiSpin->value() : 300);
        };

        connect(m_pdfPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, refreshHelp](int index) {
            if (!m_dpiSpin || !m_pdfColorModel) {
                return;
            }

            QSignalBlocker dpiBlock(m_dpiSpin);
            QSignalBlocker colorBlock(m_pdfColorModel);
            switch (index) {
            case 1:
                m_dpiSpin->setValue(600);
                m_pdfColorModel->setCurrentIndex(1);
                break;
            case 2:
                m_dpiSpin->setValue(300);
                m_pdfColorModel->setCurrentIndex(2);
                break;
            default:
                m_dpiSpin->setValue(300);
                m_pdfColorModel->setCurrentIndex(0);
                break;
            }
            refreshHelp();
        });
        connect(m_pdfColorModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [refreshHelp](int) {
            refreshHelp();
        });
        connect(m_dpiSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [refreshHelp](int) {
            refreshHelp();
        });
        m_pdfPreset->setCurrentIndex(0);
        refreshHelp();
    }
    root->addWidget(optGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QString ExportDialog::filePath() const {
    return m_pathEdit ? m_pathEdit->text().trimmed() : QString();
}

int ExportDialog::dpi() const {
    return m_dpiSpin ? m_dpiSpin->value() : 300;
}

bool ExportDialog::whiteBackground() const {
    return m_whiteBackground ? m_whiteBackground->isChecked() : true;
}

int ExportDialog::pdfColorModelIndex() const {
    return m_pdfColorModel ? m_pdfColorModel->currentIndex() : 0;
}
