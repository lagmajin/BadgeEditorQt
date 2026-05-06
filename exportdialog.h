#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QSpinBox;
class QCheckBox;

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

private:
    Format m_format;
    QLineEdit* m_pathEdit = nullptr;
    QSpinBox* m_dpiSpin = nullptr;
    QCheckBox* m_whiteBackground = nullptr;
};

#endif
