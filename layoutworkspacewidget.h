#ifndef LAYOUTWORKSPACEWIDGET_H
#define LAYOUTWORKSPACEWIDGET_H

#include <QWidget>
#include <QPdfWriter>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QPrinter;
class QString;

namespace badge {
struct DocumentData;
}

class LayoutWorkspaceWidget : public QWidget {
public:
    explicit LayoutWorkspaceWidget(QWidget* parent = nullptr);
    ~LayoutWorkspaceWidget() override;

    void setDocument(const badge::DocumentData& document);
    void refresh();
    bool exportPdf(const QString& filePath, int dpi = 300, QPdfWriter::ColorModel colorModel = QPdfWriter::ColorModel::RGB) const;
    bool exportPng(const QString& filePath, int dpi = 300, bool whiteBackground = true) const;
    bool print(QPrinter* printer) const;

private:
    QGraphicsView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    struct Impl;
    Impl* m_impl = nullptr;

    void rebuildScene();
    void updateSceneRect();
};

#endif
