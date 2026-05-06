#include "exportdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>

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
