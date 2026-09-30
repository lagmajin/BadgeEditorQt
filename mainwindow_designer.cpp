#include "mainwindow.h"

#include "imageprocessor.h"
#include "mainwindow_support.h"
#include "constants.h"

#include <QFileInfo>
#include <QPointF>
#include <algorithm>

void MainWindow::refreshDocumentFromDesigner() {
    if (m_designer) {
        m_badges = m_designer->badgeItems();
    } else {
        m_badges.clear();
    }
}

void MainWindow::onDuplicate() {
    const auto before = currentDesignerBadges();
    auto selected = selectedBadgeIndices();
    if (selected.isEmpty()) return;
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
    QList<BadgeItem> after;
    after.reserve(before.size() + selected.size());
    QList<int> afterSelection;
    afterSelection.reserve(selected.size());
    const double duplicateOffsetMm = std::max(0.0, m_appSettings.duplicateOffsetMm);
    int selectedCursor = 0;
    for (int index = 0; index < before.size(); ++index) {
        const BadgeItem& source = before[index];
        after.append(source);
        if (selectedCursor >= selected.size() || selected[selectedCursor] != index) continue;
        BadgeItem duplicate = source;
        duplicate.xMm += duplicateOffsetMm;
        duplicate.yMm += duplicateOffsetMm;
        duplicate.isSelected = false;
        after.append(duplicate);
        afterSelection.append(after.size() - 1);
        ++selectedCursor;
    }
    if (afterSelection.isEmpty()) return;
    const QString label = afterSelection.size() > 1
        ? QStringLiteral("複製 (%1件)").arg(afterSelection.size())
        : QStringLiteral("複製");
    pushBadgeChange(label, before, selected, after, afterSelection);
    appendLog(QStringLiteral("%1を履歴に追加しました").arg(label));
}

void MainWindow::onGuideToggle() {
    m_designer->setBleedVisible(m_chkBleed->isChecked());
    m_designer->setVisibleVisible(m_chkVisible->isChecked());
    if (!m_isDesigner) requestLayoutRefresh("guide toggle");
}

void MainWindow::onLightingToggle(bool on) { m_designer->setLightingEnabled(on); }
void MainWindow::onGlitterToggle(bool on) { m_designer->setGlitterEnabled(on); }
void MainWindow::onGlitterPatternChanged(int idx) { m_designer->setGlitterPattern(idx); }
void MainWindow::onLightingSlider() {
    m_designer->setLightAngle(m_sliderLightAngle->value());
    m_designer->setLightIntensity(m_sliderLightIntensity->value() / 100.0);
}

void MainWindow::onDelete() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.isEmpty()) return;
    QList<BadgeItem> after;
    after.reserve(before.size() - selected.size());
    for (int i = 0; i < before.size(); ++i) {
        if (!selected.contains(i)) after.append(before[i]);
    }
    const QString label = selected.size() > 1
        ? QStringLiteral("削除 (%1件)").arg(selected.size())
        : QStringLiteral("削除");
    pushBadgeChange(label, before, selected, after, {});
    appendLog(QStringLiteral("%1を履歴に追加しました").arg(label));
}

void MainWindow::onAddBadge() {
    const auto before = currentDesignerBadges();
    BadgeItem b;
    b.clipToCircle = true;
    const QPointF center = m_designer->mapToScene(m_designer->viewport()->rect().center());
    const double mmToPx = Constants::kMmToPx;
    b.xMm = center.x() / mmToPx - b.widthMm / 2;
    b.yMm = center.y() / mmToPx - b.heightMm / 2;
    auto after = before;
    after.append(b);
    const QString label = QStringLiteral("追加");
    pushBadgeChange(label, before, QList<int>{}, after, QList<int>{static_cast<int>(after.size() - 1)});
    appendLog(QStringLiteral("%1を履歴に追加しました").arg(label));
}

void MainWindow::onImageDropped(const QString& filePath) {
    if (ImageProcessor::loadImage(filePath, nullptr).isNull()) {
        showOperationWarning(this,
                             QStringLiteral("画像ドロップ"),
                             QStringLiteral("画像の読み込み"),
                             filePath,
                             QStringLiteral("壊れた画像、または未対応形式の可能性があります"));
        return;
    }
    const auto before = currentDesignerBadges();
    BadgeItem b;
    b.layers.append(layerFromImagePath(filePath));
    b.clipToCircle = true;
    b.label = QFileInfo(filePath).baseName();
    auto after = before;
    after.append(b);
    const QString label = QStringLiteral("画像ドロップ");
    pushBadgeChange(label, before, QList<int>{}, after, QList<int>{static_cast<int>(after.size() - 1)});
    appendLog(QStringLiteral("%1を履歴に追加しました: %2").arg(label, filePath));
}
