#ifndef WINDOWSINTEGRATION_H
#define WINDOWSINTEGRATION_H

#include <QString>
#include <QStringList>

class QWidget;

class WindowsIntegration {
public:
    enum class ToastKind {
        Info,
        Success,
        Warning,
        Error,
    };

    explicit WindowsIntegration(QWidget* anchor = nullptr);

    void setAnchor(QWidget* anchor);
    void initialize(const QString& appId);
    void setRecentFiles(const QStringList& files);
    void rememberFile(const QString& filePath);
    QStringList recentFiles() const;
    void showToast(const QString& title,
                   const QString& message,
                   ToastKind kind = ToastKind::Info,
                   int durationMs = 3200);

private:
    QWidget* m_anchor = nullptr;
    QString m_appId;
    QStringList m_recentFiles;

    static QStringList normalizeRecentFiles(const QStringList& files);
    void refreshJumpList();
    void syncShellRecentDocs();
};

#endif
