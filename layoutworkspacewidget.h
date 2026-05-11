#ifndef LAYOUTWORKSPACEWIDGET_H
#define LAYOUTWORKSPACEWIDGET_H

#include <QWidget>
#include <QPdfWriter>
#include <QPalette>
#include <QList>
#include <QPixmap>

#include "badgeitem.h"

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

    void applyThemePalette(const QPalette& palette);
    void setDocument(const badge::DocumentData& document);
    void refresh();
    void setExperimentalGpuViewport(bool on);
    QString lastError() const;
    QPixmap renderPageThumbnail(const QList<BadgeItem>& pages, int sizePx = 96, bool includeGuides = false) const;
    bool exportPdf(const QString& filePath, int dpi = 300, QPdfWriter::ColorModel colorModel = QPdfWriter::ColorModel::RGB) const;
    bool exportPdf(const QList<QList<BadgeItem>>& pages, const QString& filePath, int dpi = 300, QPdfWriter::ColorModel colorModel = QPdfWriter::ColorModel::RGB) const;
    bool exportPng(const QString& filePath, int dpi = 300, bool whiteBackground = true) const;
    bool print(QPrinter* printer, bool includeGuides = false) const;
    bool print(QPrinter* printer, const QList<QList<BadgeItem>>& pages, bool includeGuides = false) const;

private:
    QGraphicsView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    struct Impl;
    Impl* m_impl = nullptr;

    void rebuildScene();
    void updateSceneRect();
};

#endif
