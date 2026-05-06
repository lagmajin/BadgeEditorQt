#ifndef APPSETTINGSDIALOG_H
#define APPSETTINGSDIALOG_H

#include <QDialog>
#include <QShowEvent>

struct AppSettings {
    bool darkTheme = true;
    bool gridVisible = true;
    bool snapToGrid = true;
    double gridSpacingMm = 5.0;
    bool lightingEnabled = false;
    int lightAngle = 315;
    int lightIntensity = 45;
    bool glitterEnabled = false;
    int glitterPattern = 0;
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
    QCheckBox* m_lightingEnabled = nullptr;
    QSpinBox* m_lightAngle = nullptr;
    QSpinBox* m_lightIntensity = nullptr;
    QCheckBox* m_glitterEnabled = nullptr;
    QComboBox* m_glitterPattern = nullptr;
    bool m_backdropApplied = false;
    bool m_darkTheme = true;
};

#endif
