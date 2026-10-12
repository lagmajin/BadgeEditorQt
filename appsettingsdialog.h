#ifndef APPSETTINGSDIALOG_H
#define APPSETTINGSDIALOG_H

#include <QDialog>
#include <QShowEvent>
#include "canvasbackgroundmode.h"

struct AppSettings {
    bool darkTheme = false;
    bool gridVisible = true;
    bool snapToGrid = true;
    double gridSpacingMm = 5.0;
    CanvasBackgroundMode canvasBackgroundMode = CanvasBackgroundMode::PrintGuideGrid;
    double defaultBadgeSizeMm = 57.0;
    bool lightingEnabled = false;
    int lightAngle = 315;
    int lightIntensity = 45;
    bool glitterEnabled = false;
    int glitterPattern = 0;
    int printResolution = 300;
    bool experimentalGpuViewport = false;
    double duplicateOffsetMm = 2.0;
};

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QWidget;

class AppSettingsDialog : public QDialog {
public:
    explicit AppSettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);

    AppSettings settings() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void applyBackdrop();

    QComboBox* m_themeCombo = nullptr;
    QCheckBox* m_gridVisible = nullptr;
    QCheckBox* m_snapToGrid = nullptr;
    QDoubleSpinBox* m_gridSpacing = nullptr;
    QComboBox* m_canvasBackground = nullptr;
    QComboBox* m_defaultBadgeSize = nullptr;
    QCheckBox* m_lightingEnabled = nullptr;
    QSpinBox* m_lightAngle = nullptr;
    QSpinBox* m_lightIntensity = nullptr;
    QCheckBox* m_glitterEnabled = nullptr;
    QComboBox* m_glitterPattern = nullptr;
    QSpinBox* m_printResolution = nullptr;
    QCheckBox* m_experimentalGpuViewport = nullptr;
    QDoubleSpinBox* m_duplicateOffset = nullptr;
    bool m_backdropApplied = false;
    bool m_darkTheme = true;
};

#endif
