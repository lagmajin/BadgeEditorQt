#include "mainwindow.h"
#include "batchlayoutdialog.h"
#include "mixedlayoutdialog.h"
#include "projectsync.h"
#include "layoutengine.h"

#include <algorithm>
#include <cmath>
#include <QInputDialog>
#include <QLineEdit>
#include <QDialog>
#include <QMessageBox>

import badge.document;

namespace {
double layoutBadgeGuideSizeMm(const BadgeItem& badge) {
    const double widthMm = std::max(0.1, badge.widthMm);
    const double heightMm = std::max(0.1, badge.heightMm);
    return std::abs(widthMm - heightMm) > 0.1 ? std::min(widthMm, heightMm) : widthMm;
}

QList<BadgeItem> shiftedLayoutPage(const QList<BadgeItem>& page, double dxMm, double dyMm) {
    QList<BadgeItem> shifted = page;
    for (auto& badge : shifted) {
        badge.xMm += dxMm;
        badge.yMm += dyMm;
    }
    return shifted;
}

double layoutFootprintScore(const BadgeItem& badge) {
    const double maxSide = std::max(badge.widthMm, badge.heightMm);
    const double width = badge.clipToCircle ? maxSide + 3.0 : badge.widthMm;
    const double height = badge.clipToCircle ? maxSide + 3.0 : badge.heightMm;
    return width * height;
}
}

void MainWindow::onAutoLayout() {
    onSendToLayout();
}

void MainWindow::onMixedLayout() {
    QList<BadgeItem> sourceBadges;
    const auto current = currentDesignerBadges();
    if (!m_selected.isEmpty()) {
        const auto selected = selectedBadgeIndices();
        sourceBadges.reserve(selected.size());
        for (int index : selected) if (index >= 0 && index < current.size()) sourceBadges.append(current[index]);
    } else {
        sourceBadges = current;
    }
    if (sourceBadges.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("混在面付け"), QStringLiteral("面付けするテンプレートがありません。"));
        return;
    }
    MixedLayoutDialog dlg(sourceBadges, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QList<BadgeItem> mixed = dlg.expandedTemplates();
    if (mixed.isEmpty()) return;
    if (dlg.sortBySize()) {
        std::stable_sort(mixed.begin(), mixed.end(), [](const BadgeItem& a, const BadgeItem& b) {
            return layoutFootprintScore(a) > layoutFootprintScore(b);
        });
        const badge::DocumentData paperDocument = projectsync::currentDocument(mixed, *m_comboPaperSize, *m_chkLandscape,
                                                                                *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
        m_layoutPages = LayoutEngine::packMixedPages(mixed, paperDocument.paper);
        m_layoutPageNames = QList<QString>(m_layoutPages.size());
        m_layoutPageIndex = 0;
        m_layoutPreviewMode = LayoutPreviewMode::PackedMixedPages;
    } else {
        m_layoutPages.clear();
        m_layoutPageNames.clear();
        m_layoutPageIndex = 0;
        m_layoutPreviewMode = LayoutPreviewMode::AutoLayoutAll;
    }
    m_layoutBadges = mixed;
    requestLayoutRefresh("mixed layout");
    flushInternalEvents();
    m_skipNextLayoutSync = true;
    openLayoutPerspective();
    appendLog(QStringLiteral("混在面付けを準備しました: %1 種類 / %2 枚").arg(sourceBadges.size()).arg(mixed.size()));
    if (m_layoutPageCount > 1) appendLog(QStringLiteral("レイアウトを %1 ページに分割しました").arg(QString::number(m_layoutPageCount)));
}

void MainWindow::onBatchAdd() {
    BatchLayoutDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QList<BadgeItem> batchBadges;
    batchBadges.reserve(dlg.rows() * dlg.cols());
    for (int r = 0; r < dlg.rows(); ++r) {
        for (int c = 0; c < dlg.cols(); ++c) {
            BadgeItem b;
            b.widthMm = dlg.badgeWidth();
            b.heightMm = dlg.badgeHeight();
            b.xMm = 10 + c * (b.widthMm + 1);
            b.yMm = 10 + r * (b.heightMm + 1);
            b.clipToCircle = dlg.clipCircle();
            batchBadges.append(b);
        }
    }
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    m_layoutBadges = batchBadges;
    requestLayoutRefresh("batch add");
    flushInternalEvents();
    openLayoutPerspective();
}

void MainWindow::onClearLayout() {
    m_layoutBadges.clear();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    requestLayoutRefresh("layout cleared");
    flushInternalEvents();
    m_skipNextLayoutSync = true;
    openLayoutPerspective();
}

double MainWindow::activeGuideSizeMm() const {
    if (!m_selected.isEmpty()) {
        return layoutBadgeGuideSizeMm(m_selected.first()->badge());
    }
    return m_lastGuideSizeMm;
}

QString MainWindow::layoutPageTitle(int index) const {
    const int pageNumber = index + 1;
    QString title = QStringLiteral("ページ %1").arg(pageNumber);
    if (index >= 0 && index < m_layoutPageNames.size()) {
        const QString customName = m_layoutPageNames[index].trimmed();
        if (!customName.isEmpty()) {
            title += QStringLiteral(": %1").arg(customName);
        }
    }
    return title;
}

void MainWindow::onLayoutPagePrevious() {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
        return;
    }
    if (m_layoutPageIndex <= 0) {
        return;
    }
    --m_layoutPageIndex;
    requestLayoutRefresh("page previous");
}

void MainWindow::onLayoutPageNext() {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
        return;
    }
    if (m_layoutPageIndex + 1 >= m_layoutPages.size()) {
        return;
    }
    ++m_layoutPageIndex;
    requestLayoutRefresh("page next");
}

void MainWindow::onLayoutPageSelected(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
        return;
    }
    if (index < 0 || index >= m_layoutPages.size() || index == m_layoutPageIndex) {
        return;
    }
    m_layoutPageIndex = index;
    requestLayoutRefresh("page selected");
}

void MainWindow::duplicateLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size()) return;
    const QList<BadgeItem> duplicated = shiftedLayoutPage(m_layoutPages[index], 2.0, 2.0);
    m_layoutPages.insert(index + 1, duplicated);
    const QString sourceName = index < m_layoutPageNames.size() ? m_layoutPageNames[index].trimmed() : QString();
    m_layoutPageNames.insert(index + 1, sourceName.isEmpty() ? QStringLiteral("複製ページ") : QStringLiteral("%1（複製）").arg(sourceName));
    m_layoutPageIndex = index + 1;
    requestLayoutRefresh("page duplicated");
    appendLog(QStringLiteral("ページ %1 を複製しました（少しずらしました）").arg(index + 1));
}

void MainWindow::deleteLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size() || m_layoutPages.size() <= 1) return;
    m_layoutPages.removeAt(index);
    if (index < m_layoutPageNames.size()) m_layoutPageNames.removeAt(index);
    m_layoutPageIndex = std::clamp(index, 0, static_cast<int>(m_layoutPages.size()) - 1);
    requestLayoutRefresh("page deleted");
    appendLog(QStringLiteral("ページ %1 を削除しました").arg(index + 1));
}

void MainWindow::moveLayoutPageToFront(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index <= 0 || index >= m_layoutPages.size()) return;
    const QList<BadgeItem> page = m_layoutPages.takeAt(index);
    m_layoutPages.prepend(page);
    if (index < m_layoutPageNames.size()) m_layoutPageNames.prepend(m_layoutPageNames.takeAt(index));
    m_layoutPageIndex = 0;
    requestLayoutRefresh("page moved front");
    appendLog(QStringLiteral("ページ %1 を先頭へ移動しました").arg(index + 1));
}

void MainWindow::moveLayoutPageToBack(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size() - 1) return;
    const QList<BadgeItem> page = m_layoutPages.takeAt(index);
    m_layoutPages.append(page);
    if (index < m_layoutPageNames.size()) m_layoutPageNames.append(m_layoutPageNames.takeAt(index));
    m_layoutPageIndex = m_layoutPages.size() - 1;
    requestLayoutRefresh("page moved back");
    appendLog(QStringLiteral("ページ %1 を末尾へ移動しました").arg(index + 1));
}

void MainWindow::renameLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size()) return;
    const QString currentName = index < m_layoutPageNames.size() ? m_layoutPageNames[index].trimmed() : QString();
    bool ok = false;
    const QString entered = QInputDialog::getText(this, QStringLiteral("ページ名変更"), QStringLiteral("ページ名:"), QLineEdit::Normal,
                                                  currentName.isEmpty() ? layoutPageTitle(index) : currentName, &ok);
    if (!ok) return;
    if (index >= m_layoutPageNames.size()) m_layoutPageNames.resize(index + 1);
    m_layoutPageNames[index] = entered.trimmed();
    requestLayoutRefresh("page renamed");
    appendLog(QStringLiteral("ページ %1 の名前を変更しました").arg(index + 1));
}
