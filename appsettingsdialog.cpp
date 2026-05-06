#include "appsettingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QOperatingSystemVersion>
#include <QWindow>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

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

#ifdef Q_OS_WIN
static void applyWinAcrylic(HWND hwnd, bool darkMode) {
    if (!hwnd) {
        return;
    }
    const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW;
    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    const BOOL dark = darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}
#endif

AppSettingsDialog::AppSettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("アプリ設定");
    setAttribute(Qt::WA_StyledBackground, true);
    m_darkTheme = settings.darkTheme;

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel("アプリ全体の表示や編集時の既定値をまとめて調整できます。");
    info->setWordWrap(true);
    root->addWidget(info);

    auto* appearanceGroup = new QGroupBox("外観");
    auto* appearanceForm = new QFormLayout(appearanceGroup);
    m_themeCombo = new QComboBox;
    m_themeCombo->addItems({"ダーク", "ライト"});
    m_themeCombo->setCurrentIndex(settings.darkTheme ? 0 : 1);
    appearanceForm->addRow("テーマ:", m_themeCombo);
    root->addWidget(appearanceGroup);

    auto* gridGroup = new QGroupBox("編集ガイド");
    auto* gridForm = new QFormLayout(gridGroup);
    m_gridVisible = new QCheckBox("表示する");
    m_gridVisible->setChecked(settings.gridVisible);
    m_snapToGrid = new QCheckBox("スナップする");
    m_snapToGrid->setChecked(settings.snapToGrid);
    m_gridSpacing = new QDoubleSpinBox;
    m_gridSpacing->setRange(0.5, 50.0);
    m_gridSpacing->setSingleStep(0.5);
    m_gridSpacing->setSuffix(" mm");
    m_gridSpacing->setDecimals(1);
    m_gridSpacing->setValue(settings.gridSpacingMm);
    gridForm->addRow(m_gridVisible);
    gridForm->addRow(m_snapToGrid);
    gridForm->addRow("間隔:", m_gridSpacing);
    root->addWidget(gridGroup);

    auto* previewGroup = new QGroupBox("プレビュー");
    auto* previewForm = new QFormLayout(previewGroup);
    m_lightingEnabled = new QCheckBox("光沢を有効にする");
    m_lightingEnabled->setChecked(settings.lightingEnabled);
    m_lightAngle = new QSpinBox;
    m_lightAngle->setRange(0, 360);
    m_lightAngle->setSuffix("°");
    m_lightAngle->setValue(settings.lightAngle);
    m_lightIntensity = new QSpinBox;
    m_lightIntensity->setRange(0, 100);
    m_lightIntensity->setSuffix(" %");
    m_lightIntensity->setValue(settings.lightIntensity);
    previewForm->addRow(m_lightingEnabled);
    previewForm->addRow("向き:", m_lightAngle);
    previewForm->addRow("強さ:", m_lightIntensity);

    m_glitterEnabled = new QCheckBox("ラメを有効にする");
    m_glitterEnabled->setChecked(settings.glitterEnabled);
    m_glitterPattern = new QComboBox;
    m_glitterPattern->addItems({"全面輝き", "星型", "雪型"});
    m_glitterPattern->setCurrentIndex(std::clamp(settings.glitterPattern, 0, 2));
    previewForm->addRow(m_glitterEnabled);
    previewForm->addRow("ラメパターン:", m_glitterPattern);
    root->addWidget(previewGroup);

    auto* printGroup = new QGroupBox("印刷");
    auto* printForm = new QFormLayout(printGroup);
    m_printResolution = new QSpinBox;
    m_printResolution->setRange(72, 1200);
    m_printResolution->setSuffix(" DPI");
    m_printResolution->setValue(std::clamp(settings.printResolution, 72, 1200));
    printForm->addRow("解像度:", m_printResolution);
    root->addWidget(printGroup);

    auto updateEnabledState = [this]() {
        const bool lightingOn = m_lightingEnabled && m_lightingEnabled->isChecked();
        const bool glitterOn = m_glitterEnabled && m_glitterEnabled->isChecked();
        if (m_lightAngle) {
            m_lightAngle->setEnabled(lightingOn);
        }
        if (m_lightIntensity) {
            m_lightIntensity->setEnabled(lightingOn);
        }
        if (m_glitterPattern) {
            m_glitterPattern->setEnabled(glitterOn);
        }
    };

    connect(m_lightingEnabled, &QCheckBox::toggled, this, [updateEnabledState](bool) { updateEnabledState(); });
    connect(m_glitterEnabled, &QCheckBox::toggled, this, [updateEnabledState](bool) { updateEnabledState(); });
    updateEnabledState();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void AppSettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_backdropApplied) {
        return;
    }
    m_backdropApplied = true;
    applyBackdrop();
}

void AppSettingsDialog::applyBackdrop() {
#ifdef Q_OS_WIN
    if (QOperatingSystemVersion::currentType() != QOperatingSystemVersion::Windows) {
        return;
    }
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11) {
        return;
    }
    if (auto* window = windowHandle()) {
        applyWinAcrylic(reinterpret_cast<HWND>(window->winId()), m_darkTheme);
    } else if (winId()) {
        applyWinAcrylic(reinterpret_cast<HWND>(winId()), m_darkTheme);
    }
#endif
}

AppSettings AppSettingsDialog::settings() const {
    AppSettings result;
    result.darkTheme = m_themeCombo ? (m_themeCombo->currentIndex() == 0) : true;
    result.gridVisible = m_gridVisible ? m_gridVisible->isChecked() : true;
    result.snapToGrid = m_snapToGrid ? m_snapToGrid->isChecked() : true;
    result.gridSpacingMm = m_gridSpacing ? m_gridSpacing->value() : 5.0;
    result.lightingEnabled = m_lightingEnabled ? m_lightingEnabled->isChecked() : false;
    result.lightAngle = m_lightAngle ? m_lightAngle->value() : 315;
    result.lightIntensity = m_lightIntensity ? m_lightIntensity->value() : 45;
    result.glitterEnabled = m_glitterEnabled ? m_glitterEnabled->isChecked() : false;
    result.glitterPattern = m_glitterPattern ? m_glitterPattern->currentIndex() : 0;
    result.printResolution = m_printResolution ? m_printResolution->value() : 300;
    return result;
}
