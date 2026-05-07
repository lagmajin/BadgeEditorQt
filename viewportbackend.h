#ifndef VIEWPORTBACKEND_H
#define VIEWPORTBACKEND_H

class QGraphicsView;

namespace viewportbackend {

bool experimentalGpuViewportEnabled();
bool resolvedGpuViewportEnabled(bool settingsEnabled);
void applySceneViewportProfile(QGraphicsView* view, bool experimentalGpuViewport);

}

#endif
