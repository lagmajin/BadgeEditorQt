#ifndef DOCKSTYLEMANAGER_H
#define DOCKSTYLEMANAGER_H

#include <QObject>
#include <QPalette>
#include <QPointer>
#include <wobjectdefs.h>

class QProxyStyle;
class QWidget;
class QEvent;

namespace ads {
class CDockManager;
class CDockWidget;
class CDockWidgetTab;
}

class DockStyleManager : public QObject {
    W_OBJECT(DockStyleManager)
public:
    explicit DockStyleManager(ads::CDockManager* dockManager, QObject* parent = nullptr);
    ~DockStyleManager() override;

    void applyTheme(const QPalette& palette);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    static bool isDockRelatedObject(QObject* watched, ads::CDockManager* dockManager);
    static ads::CDockWidget* dockFromObject(QObject* object);
    static void repolishWidget(QWidget* widget);
    static void applyWidgetPalette(QWidget* widget, const QPalette& palette);
    void scheduleRefresh();
    void refreshDockDecorations();

    QPointer<ads::CDockManager> m_dockManager;
    QProxyStyle* m_proxyStyle = nullptr;
    QPalette m_palette;
    bool m_refreshScheduled = false;
};

#endif
