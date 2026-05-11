module;

#include <QGraphicsView>
#include <QtCore/qglobal.h>
#include <QtOpenGLWidgets/QOpenGLWidget>

export module viewportbackend;

export namespace viewportbackend {

bool experimentalGpuViewportEnabled() {
    return qEnvironmentVariableIntValue("BADGEEDITOR_EXPERIMENTAL_GPU_VIEWPORT") != 0;
}

bool resolvedGpuViewportEnabled(bool settingsEnabled) {
    return settingsEnabled || experimentalGpuViewportEnabled();
}

void applySceneViewportProfile(QGraphicsView* view, bool experimentalGpuViewport) {
    if (!view) {
        return;
    }

    // This stays deliberately small: it gives us a single switch point for
    // future QRhi-backed paths without changing the widget logic again.
    if (experimentalGpuViewport) {
        view->setViewport(new QOpenGLWidget);
        view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    } else {
        view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    }
    view->setCacheMode(QGraphicsView::CacheNone);
}

} // namespace viewportbackend
