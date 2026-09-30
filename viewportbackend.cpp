#include "viewportbackend.h"

#include <QGraphicsView>
#include <QtCore/qglobal.h>
#include <QtOpenGLWidgets/QOpenGLWidget>

namespace viewportbackend {

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

    if (experimentalGpuViewport) {
        view->setViewport(new QOpenGLWidget);
        view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    } else {
        view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    }
    view->setCacheMode(QGraphicsView::CacheNone);
}

} // namespace viewportbackend
