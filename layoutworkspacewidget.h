#ifndef LAYOUTWORKSPACEWIDGET_H
#define LAYOUTWORKSPACEWIDGET_H

#include <QWidget>

class QGraphicsView;
class QGraphicsScene;

namespace badge {
struct DocumentData;
}

class LayoutWorkspaceWidget : public QWidget {
public:
    explicit LayoutWorkspaceWidget(QWidget* parent = nullptr);
    ~LayoutWorkspaceWidget() override;

    void setDocument(const badge::DocumentData& document);
    void refresh();

private:
    QGraphicsView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    struct Impl;
    Impl* m_impl = nullptr;

    void rebuildScene();
    void updateSceneRect();
};

#endif
