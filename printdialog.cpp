#include "printdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPrinterInfo>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QPalette>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW static_cast<DWM_SYSTEMBACKDROP_TYPE>(3)
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND static_cast<DWM_WINDOW_CORNER_PREFERENCE>(2)
#endif
#endif

namespace {

QString formatPaperSummary(double widthMm, double heightMm) {
    return QStringLiteral("%1 × %2 mm")
        .arg(QString::number(widthMm, 'f', 1), QString::number(heightMm, 'f', 1));
}

void updatePaperSummaryLabel(QLabel* label, double widthMm, double heightMm) {
    if (!label) {
        return;
    }
    const QString orientation = widthMm >= heightMm ? QStringLiteral("横向き") : QStringLiteral("縦向き");
    label->setText(QStringLiteral("用紙: %1\n向き: %2\n実寸印刷で出力します。")
                       .arg(formatPaperSummary(widthMm, heightMm), orientation));
}

#ifdef Q_OS_WIN
static void applyWinBackdrop(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW;
    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}
#endif

} // namespace

PrintDialog::PrintDialog(double paperWidthMm, double paperHeightMm, int defaultResolution, QWidget* parent)
    : QDialog(parent), m_paperWidthMm(paperWidthMm), m_paperHeightMm(paperHeightMm) {
    setWindowTitle(QStringLiteral("印刷設定"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);

    auto* printerGroup = new QGroupBox(QStringLiteral("プリンタ"));
    auto* printerForm = new QFormLayout(printerGroup);
    m_printerCombo = new QComboBox;
    const auto printers = QPrinterInfo::availablePrinters();
    const QString defaultPrinter = QPrinterInfo::defaultPrinterName();
    for (const auto& info : printers) {
        m_printerCombo->addItem(info.printerName());
    }
    if (m_printerCombo->count() == 0) {
        m_printerCombo->addItem(defaultPrinter.isEmpty() ? QStringLiteral("既定のプリンタ") : defaultPrinter);
    }
    const int defaultIndex = qMax(0, m_printerCombo->findText(defaultPrinter));
    m_printerCombo->setCurrentIndex(defaultIndex);
    printerForm->addRow(QStringLiteral("プリンタ:"), m_printerCombo);

    m_paperSummary = new QLabel;
    m_paperSummary->setWordWrap(true);
    auto paperPalette = m_paperSummary->palette();
    paperPalette.setColor(QPalette::WindowText, paperPalette.color(QPalette::Mid));
    paperPalette.setColor(QPalette::Text, paperPalette.color(QPalette::Mid));
    m_paperSummary->setPalette(paperPalette);
    printerForm->addRow(QString(), m_paperSummary);
    root->addWidget(printerGroup);

    auto* optionGroup = new QGroupBox(QStringLiteral("印刷オプション"));
    auto* optionForm = new QFormLayout(optionGroup);
    m_resolutionSpin = new QSpinBox;
    m_resolutionSpin->setRange(72, 1200);
    m_resolutionSpin->setSuffix(QStringLiteral(" DPI"));
    m_resolutionSpin->setValue(qBound(72, defaultResolution, 1200));
    optionForm->addRow(QStringLiteral("解像度:"), m_resolutionSpin);

    m_copiesSpin = new QSpinBox;
    m_copiesSpin->setRange(1, 99);
    m_copiesSpin->setValue(1);
    optionForm->addRow(QStringLiteral("部数:"), m_copiesSpin);

    m_grayScale = new QCheckBox(QStringLiteral("グレースケールで印刷"));
    optionForm->addRow(m_grayScale);

    m_includeGuides = new QCheckBox(QStringLiteral("ガイド線を含める"));
    m_includeGuides->setChecked(false);
    optionForm->addRow(m_includeGuides);
    root->addWidget(optionGroup);

    auto* note = new QLabel(QStringLiteral("印刷は実寸を基準に行います。用紙の向きは文書設定に合わせます。"));
    note->setWordWrap(true);
    auto notePalette = note->palette();
    notePalette.setColor(QPalette::WindowText, notePalette.color(QPalette::Mid));
    note->setPalette(notePalette);
    root->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    refreshPaperSummary();
}

QString PrintDialog::printerName() const {
    return m_printerCombo ? m_printerCombo->currentText().trimmed() : QString();
}

int PrintDialog::resolution() const {
    return m_resolutionSpin ? m_resolutionSpin->value() : 300;
}

int PrintDialog::copies() const {
    return m_copiesSpin ? m_copiesSpin->value() : 1;
}

bool PrintDialog::includeGuides() const {
    return m_includeGuides ? m_includeGuides->isChecked() : false;
}

bool PrintDialog::grayScale() const {
    return m_grayScale ? m_grayScale->isChecked() : false;
}

void PrintDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (!m_backdropApplied) {
        applyBackdrop();
        m_backdropApplied = true;
    }
}

void PrintDialog::applyBackdrop() {
#ifdef Q_OS_WIN
    if (windowHandle()) {
        applyWinBackdrop(HWND(windowHandle()->winId()));
    }
#endif
}

void PrintDialog::refreshPaperSummary() {
    updatePaperSummaryLabel(m_paperSummary, m_paperWidthMm, m_paperHeightMm);
}
