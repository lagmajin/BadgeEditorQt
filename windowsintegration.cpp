#include "windowsintegration.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QFont>
#include <QMainWindow>
#include <QPointer>
#include <QStatusBar>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QStyle>
#include <QWindow>
#include <QTimer>
#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#endif

namespace {

QStringList trimRecentFiles(QStringList files) {
    QStringList normalized;
    QSet<QString> seen;
    for (const auto& file : files) {
        const QString cleaned = QFileInfo(file).absoluteFilePath();
        if (cleaned.isEmpty() || seen.contains(cleaned.toLower())) {
            continue;
        }
        seen.insert(cleaned.toLower());
        normalized.append(cleaned);
        if (normalized.size() >= 8) {
            break;
        }
    }
    return normalized;
}

QString toastAccent(WindowsIntegration::ToastKind kind) {
    switch (kind) {
    case WindowsIntegration::ToastKind::Success:
        return QStringLiteral("#3CB371");
    case WindowsIntegration::ToastKind::Warning:
        return QStringLiteral("#E6A23C");
    case WindowsIntegration::ToastKind::Error:
        return QStringLiteral("#E85D5D");
    case WindowsIntegration::ToastKind::Info:
    default:
        return QStringLiteral("#7AA2F7");
    }
}

#ifdef Q_OS_WIN
struct ScopedComInit {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
    ~ScopedComInit() {
        if (hr == S_OK || hr == S_FALSE) {
            CoUninitialize();
        }
    }
};

template <typename T>
void releaseCom(T*& ptr) {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

QString quoteShellArgument(const QString& arg) {
    QString escaped = arg;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

IShellLinkW* createShellLink(const QString& appPath, const QString& arguments, const QString& description) {
    IShellLinkW* link = nullptr;
    const HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr) || !link) {
        return nullptr;
    }

    const std::wstring wideAppPath = QDir::toNativeSeparators(appPath).toStdWString();
    const std::wstring wideArgs = arguments.toStdWString();
    const std::wstring wideDesc = description.toStdWString();
    link->SetPath(wideAppPath.c_str());
    link->SetArguments(wideArgs.c_str());
    link->SetDescription(wideDesc.c_str());
    link->SetIconLocation(wideAppPath.c_str(), 0);
    return link;
}

bool addLinkToCollection(IObjectCollection* collection, IShellLinkW* link) {
    if (!collection || !link) {
        return false;
    }
    return SUCCEEDED(collection->AddObject(link));
}

void addRecentDoc(const QString& path) {
    const std::wstring widePath = QDir::toNativeSeparators(path).toStdWString();
    SHAddToRecentDocs(SHARD_PATHW, widePath.c_str());
}
#endif

}

WindowsIntegration::WindowsIntegration(QWidget* anchor)
    : m_anchor(anchor) {
}

void WindowsIntegration::setAnchor(QWidget* anchor) {
    m_anchor = anchor;
}

void WindowsIntegration::initialize(const QString& appId) {
    m_appId = appId.trimmed();
#ifdef Q_OS_WIN
    if (!m_appId.isEmpty()) {
        SetCurrentProcessExplicitAppUserModelID(m_appId.toStdWString().c_str());
    }
#endif
    const QSettings settings;
    m_recentFiles = normalizeRecentFiles(settings.value(QStringLiteral("windows/recentFiles")).toStringList());
    refreshJumpList();
}

QStringList WindowsIntegration::normalizeRecentFiles(const QStringList& files) {
    return trimRecentFiles(files);
}

QStringList WindowsIntegration::recentFiles() const {
    return m_recentFiles;
}

void WindowsIntegration::setRecentFiles(const QStringList& files) {
    m_recentFiles = normalizeRecentFiles(files);
    QSettings settings;
    settings.setValue(QStringLiteral("windows/recentFiles"), m_recentFiles);
    refreshJumpList();
}

void WindowsIntegration::rememberFile(const QString& filePath) {
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    QStringList updated = m_recentFiles;
    const QString normalized = QFileInfo(filePath).absoluteFilePath();
    updated.removeAll(normalized);
    updated.prepend(normalized);
    setRecentFiles(updated);
    syncShellRecentDocs();
}

void WindowsIntegration::showToast(const QString& title,
                                   const QString& message,
                                   ToastKind kind,
                                   int durationMs) {
    if (auto* mw = qobject_cast<QMainWindow*>(m_anchor)) {
        mw->statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), durationMs);
    }

    auto* toast = new QFrame(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    toast->setAttribute(Qt::WA_ShowWithoutActivating);
    toast->setAttribute(Qt::WA_TranslucentBackground);
    toast->setObjectName(QStringLiteral("windowsToast"));
    toast->setStyleSheet(QStringLiteral(
        "QFrame#windowsToast {"
        "  background-color: rgba(28, 30, 36, 235);"
        "  border: 1px solid rgba(255, 255, 255, 35);"
        "  border-left: 4px solid %1;"
        "  border-radius: 10px;"
        "}"
        "QLabel { color: white; }").arg(toastAccent(kind)));

    auto* root = new QVBoxLayout(toast);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(4);

    auto* header = new QLabel(title, toast);
    QFont titleFont = header->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    titleFont.setBold(true);
    header->setFont(titleFont);
    root->addWidget(header);

    auto* body = new QLabel(message, toast);
    body->setWordWrap(true);
    body->setMaximumWidth(340);
    root->addWidget(body);

    toast->adjustSize();

    QRect anchorRect;
    if (m_anchor && m_anchor->windowHandle()) {
        anchorRect = m_anchor->windowHandle()->screen()
            ? m_anchor->windowHandle()->screen()->availableGeometry()
            : m_anchor->frameGeometry();
    } else if (m_anchor) {
        anchorRect = m_anchor->frameGeometry();
    } else if (auto* screen = QGuiApplication::primaryScreen()) {
        anchorRect = screen->availableGeometry();
    }
    if (anchorRect.isNull()) {
        anchorRect = QRect(0, 0, 1280, 720);
    }
    const QSize size = toast->sizeHint();
    const QPoint pos(anchorRect.right() - size.width() - 24,
                     anchorRect.bottom() - size.height() - 24);
    toast->move(pos);
    toast->show();

    QTimer::singleShot(std::max(1000, durationMs), toast, &QWidget::close);
}

void WindowsIntegration::refreshJumpList() {
#ifdef Q_OS_WIN
    if (m_appId.isEmpty()) {
        return;
    }

    ScopedComInit com;
    if (!com.ok()) {
        return;
    }

    ICustomDestinationList* destinationList = nullptr;
    IObjectCollection* tasks = nullptr;
    IObjectCollection* recentCollection = nullptr;
    IObjectArray* recentArray = nullptr;
    IObjectArray* removed = nullptr;

    const QString appPath = QCoreApplication::applicationFilePath();
    const std::wstring wideAppId = m_appId.toStdWString();
    const std::wstring wideAppPath = QDir::toNativeSeparators(appPath).toStdWString();

    do {
        if (FAILED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&destinationList))) || !destinationList) {
            break;
        }
        if (FAILED(destinationList->SetAppID(wideAppId.c_str()))) {
            break;
        }
        UINT maxSlots = 0;
        if (FAILED(destinationList->BeginList(&maxSlots, IID_PPV_ARGS(&removed)))) {
            break;
        }

        if (SUCCEEDED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&tasks))) && tasks) {
            if (IShellLinkW* newLink = createShellLink(appPath, QStringLiteral("--new"), QStringLiteral("新規プロジェクト"))) {
                addLinkToCollection(tasks, newLink);
                newLink->Release();
            }
            if (IShellLinkW* openLink = createShellLink(appPath, QStringLiteral("--open"), QStringLiteral("プロジェクトを開く"))) {
                addLinkToCollection(tasks, openLink);
                openLink->Release();
            }
            if (IObjectArray* taskArray = nullptr; SUCCEEDED(tasks->QueryInterface(IID_PPV_ARGS(&taskArray))) && taskArray) {
                destinationList->AddUserTasks(taskArray);
                releaseCom(taskArray);
            }
        }

        if (!m_recentFiles.isEmpty() &&
            SUCCEEDED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&recentCollection))) &&
            recentCollection) {
            for (const auto& file : m_recentFiles) {
                const QString args = QStringLiteral("--open %1").arg(quoteShellArgument(file));
                if (IShellLinkW* recentLink = createShellLink(appPath, args, QFileInfo(file).fileName())) {
                    addLinkToCollection(recentCollection, recentLink);
                    recentLink->Release();
                }
            }
            if (SUCCEEDED(recentCollection->QueryInterface(IID_PPV_ARGS(&recentArray))) && recentArray) {
                destinationList->AppendCategory(QStringLiteral("最近のプロジェクト").toStdWString().c_str(), recentArray);
            }
        }

        destinationList->CommitList();
    } while (false);

    releaseCom(recentArray);
    releaseCom(recentCollection);
    releaseCom(tasks);
    releaseCom(removed);
    releaseCom(destinationList);
#else
    Q_UNUSED(this);
#endif
}

void WindowsIntegration::syncShellRecentDocs() {
#ifdef Q_OS_WIN
    for (const auto& file : m_recentFiles) {
        addRecentDoc(file);
    }
#else
    Q_UNUSED(this);
#endif
}
