#include "mainwindow.h"

#include "imageprocessor.h"
#include "mainwindow_support.h"
#include "constants.h"

#include <QFileInfo>
#include <QInputDialog>
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
    b.widthMm = m_appSettings.defaultBadgeSizeMm;
    b.heightMm = m_appSettings.defaultBadgeSizeMm;
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

void MainWindow::onAddTextBadge() {
    bool accepted = false;
    const QString text = QInputDialog::getText(this,
                                               QStringLiteral("テキスト追加"),
                                               QStringLiteral("バッジに表示する文字:"),
                                               QLineEdit::Normal,
                                               QString(),
                                               &accepted).trimmed();
    if (!accepted || text.isEmpty()) {
        return;
    }

    const auto before = currentDesignerBadges();
    BadgeItem badge;
    badge.widthMm = m_appSettings.defaultBadgeSizeMm;
    badge.heightMm = m_appSettings.defaultBadgeSizeMm;
    badge.clipToCircle = true;
    badge.displayText = text;
    const QPointF center = m_designer->mapToScene(m_designer->viewport()->rect().center());
    const double mmToPx = Constants::kMmToPx;
    badge.xMm = center.x() / mmToPx - badge.widthMm / 2.0;
    badge.yMm = center.y() / mmToPx - badge.heightMm / 2.0;

    auto after = before;
    after.append(badge);
    const int addedIndex = static_cast<int>(after.size() - 1);
    pushBadgeChange(QStringLiteral("テキスト追加"), before, {}, after, {addedIndex});
    appendLog(QStringLiteral("文字入りバッジを追加しました"));
}

void MainWindow::onImageDropped(const QString& filePath) {
    const QPointF center = m_designer->mapToScene(m_designer->viewport()->rect().center());
    onImageDroppedAt(filePath, center);
}

void MainWindow::onImageDroppedAt(const QString& filePath, const QPointF& scenePosition) {
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
    b.widthMm = m_appSettings.defaultBadgeSizeMm;
    b.heightMm = m_appSettings.defaultBadgeSizeMm;
    const double mmToPx = Constants::kMmToPx;
    b.xMm = scenePosition.x() / mmToPx - b.widthMm * 0.5;
    b.yMm = scenePosition.y() / mmToPx - b.heightMm * 0.5;
    b.layers.append(layerFromImagePath(filePath));
    b.clipToCircle = true;
    b.label = QFileInfo(filePath).baseName();
    auto after = before;
    after.append(b);
    const QString label = QStringLiteral("画像ドロップ");
    pushBadgeChange(label, before, QList<int>{}, after, QList<int>{static_cast<int>(after.size() - 1)});
    appendLog(QStringLiteral("%1を履歴に追加しました: %2").arg(label, filePath));
}
