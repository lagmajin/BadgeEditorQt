#ifndef PRINTDIALOG_H
#define PRINTDIALOG_H

#include <QDialog>
#include <QShowEvent>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QWidget;

class PrintDialog : public QDialog {
public:
    explicit PrintDialog(double paperWidthMm, double paperHeightMm, int defaultResolution, QWidget* parent = nullptr);

    QString printerName() const;
    int resolution() const;
    int copies() const;
    bool includeGuides() const;
    bool grayScale() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void applyBackdrop();
    void refreshPaperSummary();

    QComboBox* m_printerCombo = nullptr;
    QSpinBox* m_resolutionSpin = nullptr;
    QSpinBox* m_copiesSpin = nullptr;
    QCheckBox* m_includeGuides = nullptr;
    QCheckBox* m_grayScale = nullptr;
    QLabel* m_paperSummary = nullptr;
    double m_paperWidthMm = 0.0;
    double m_paperHeightMm = 0.0;
    bool m_backdropApplied = false;
};

#endif
