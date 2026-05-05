#include "layoutworkspacewidget.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QVBoxLayout>
#include <QPainter>
#include <algorithm>

import badge.document;

struct LayoutWorkspaceWidget::Impl {
    badge::DocumentData document;
};

LayoutWorkspaceWidget::LayoutWorkspaceWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing);
    m_view->setBackgroundBrush(Qt::darkGray);
    layout->addWidget(m_view);

    m_impl = new Impl;
    updateSceneRect();
}

LayoutWorkspaceWidget::~LayoutWorkspaceWidget() {
    delete m_impl;
}

void LayoutWorkspaceWidget::setDocument(const badge::DocumentData& document) {
    m_impl->document = document;
    rebuildScene();
}

void LayoutWorkspaceWidget::refresh() {
    rebuildScene();
}

void LayoutWorkspaceWidget::updateSceneRect() {
    const double mmToPx = 96.0 / 25.4;
    m_scene->setSceneRect(-50, -50,
                          m_impl->document.paper.widthMm * mmToPx + 100,
                          m_impl->document.paper.heightMm * mmToPx + 100);
}

void LayoutWorkspaceWidget::rebuildScene() {
    const double mmToPx = 96.0 / 25.4;
    m_scene->clear();
    updateSceneRect();

    m_scene->addRect(0, 0,
                     m_impl->document.paper.widthMm * mmToPx,
                     m_impl->document.paper.heightMm * mmToPx,
                     QPen(Qt::black), QBrush(Qt::white));

    QPen safePen(Qt::red, 1, Qt::DashLine);
    safePen.setDashPattern({5, 5});
    const double marginPx = m_impl->document.paper.marginMm * mmToPx;
    const double safeWidthPx = std::max(0.0, m_impl->document.paper.widthMm - m_impl->document.paper.marginMm * 2.0) * mmToPx;
    const double safeHeightPx = std::max(0.0, m_impl->document.paper.heightMm - m_impl->document.paper.marginMm * 2.0) * mmToPx;
    m_scene->addRect(marginPx, marginPx,
                     safeWidthPx,
                     safeHeightPx,
                     safePen, Qt::NoBrush);

    for (const auto& b : m_impl->document.badges) {
        auto* rect = m_scene->addRect(0, 0,
                                      b.widthMm * mmToPx,
                                      b.heightMm * mmToPx,
                                      QPen(Qt::black), QBrush(Qt::cyan));
        rect->setPos(b.xMm * mmToPx, b.yMm * mmToPx);
    }
}
