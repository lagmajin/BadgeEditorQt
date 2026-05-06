#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;

class ExportDialog : public QDialog {
public:
    enum class Format {
        Pdf,
        Png,
    };

    explicit ExportDialog(Format format, const QString& defaultPath, QWidget* parent = nullptr);

    QString filePath() const;
    int dpi() const;
    bool whiteBackground() const;
    int pdfColorModelIndex() const;

private:
    Format m_format;
    QLineEdit* m_pathEdit = nullptr;
    QSpinBox* m_dpiSpin = nullptr;
    QCheckBox* m_whiteBackground = nullptr;
    QComboBox* m_pdfPreset = nullptr;
    QComboBox* m_pdfColorModel = nullptr;
    QLabel* m_pdfPresetHelp = nullptr;
};

#endif
