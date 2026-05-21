#include "mainwindow.h"
#include "appsettingsdialog.h"
#include "badgegraphicitem.h"
#include "batchlayoutdialog.h"
#include "exportdialog.h"
#include "imageprocessor.h"
#include "layoutengine.h"
#include "mixedlayoutdialog.h"
#include "printdialog.h"
#include "projectsync.h"
#include "transferdebugdialog.h"
#include "windowsintegration.h"
import viewportbackend;
#include "constants.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSizePolicy>
#include <QListWidget>
#include <QApplication>
#include <QClipboard>
#include <QBrush>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSize>
#include <QStyle>
#include <QInputDialog>
#include <QTimer>
#include <QSettings>
#include <QShowEvent>
#include <QTime>
#include <QWindow>
#include <QPageSize>
#include <QPageLayout>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QTemporaryDir>
#include <QDir>
#include <QDockWidget>
#include <QTabWidget>
#include <QSignalBlocker>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QListWidgetItem>
#include <QPixmap>
#include <algorithm>
#include <iterator>
#include <functional>
#include <limits>
#include <cmath>
#include <string>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif
#include <QFont>
#include <DockManager.h>
#include <DockAreaWidget.h>
#include <DockWidget.h>
#include <QFileInfo>
#include <QPalette>
#include <QStringList>
#ifdef BADGEEDITOR_HAS_QTSVG
#include <QSvgRenderer>
#endif
#include <wobjectimpl.h>

import badge.documentio;
import badge.document;
import badge.qtbridge;

namespace {

#ifdef Q_OS_WIN
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW static_cast<DWM_SYSTEMBACKDROP_TYPE>(2)
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND static_cast<DWM_WINDOW_CORNER_PREFERENCE>(2)
#endif
#endif

QRectF fitRectInside(const QRectF& target, const QSizeF& sourceSize) {
    if (target.isEmpty() || sourceSize.width() <= 0.0 || sourceSize.height() <= 0.0) {
        return target;
    }

    const qreal scale = std::min(target.width() / sourceSize.width(),
                                 target.height() / sourceSize.height());
    const QSizeF fittedSize(sourceSize.width() * scale, sourceSize.height() * scale);
    return QRectF(QPointF(target.center().x() - fittedSize.width() * 0.5,
                          target.center().y() - fittedSize.height() * 0.5),
                  fittedSize);
}

void showOperationWarning(QWidget* parent,
                          const QString& title,
                          const QString& action,
                          const QString& path = QString(),
                          const QString& detail = QString()) {
    QStringList lines;
    lines.append(QStringLiteral("%1に失敗しました").arg(action));
    if (!path.isEmpty()) {
        lines.append(QDir::toNativeSeparators(path));
    }
    if (!detail.isEmpty()) {
        lines.append(detail);
    }
    QMessageBox::warning(parent, title, lines.join(QStringLiteral("\n")));
}

QColor blend(const QColor& a, const QColor& b, qreal ratio) {
    const qreal clamped = qBound<qreal>(0.0, ratio, 1.0);
    return QColor::fromRgbF(
        a.redF()   * (1.0 - clamped) + b.redF()   * clamped,
        a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
        a.blueF()  * (1.0 - clamped) + b.blueF()  * clamped,
        a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped
    );
}

enum class StatusLevel {
    Good,
    Warning,
    Neutral,
};

QColor statusColor(StatusLevel level) {
    switch (level) {
    case StatusLevel::Good:
        return QColor(74, 179, 92);
    case StatusLevel::Warning:
        return QColor(230, 140, 43);
    case StatusLevel::Neutral:
    default:
        return QColor(140, 145, 155);
    }
}

QString statusText(StatusLevel level) {
    switch (level) {
    case StatusLevel::Good:
        return QStringLiteral("OK");
    case StatusLevel::Warning:
        return QStringLiteral("要確認");
    case StatusLevel::Neutral:
    default:
        return QStringLiteral("未作成");
    }
}

QPalette makeCinema4DDarkPalette() {
    const QColor window(0x29, 0x2B, 0x31);
    const QColor panel(0x1D, 0x1F, 0x24);
    const QColor button(0x33, 0x36, 0x3C);
    const QColor text(0xF1, 0xF1, 0xF1);
    const QColor border(0x48, 0x4C, 0x55);
    const QColor highlight(0xE0, 0x8C, 0x2B);
    const QColor highlightText(0x19, 0x1A, 0x1E);

    QPalette pal;
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, panel);
    pal.setColor(QPalette::AlternateBase, blend(panel, window, 0.42));
    pal.setColor(QPalette::ToolTipBase, blend(window, panel, 0.15));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, button);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, QColor(0x90, 0xBC, 0xFF));
    pal.setColor(QPalette::LinkVisited, QColor(0xB8, 0x94, 0xD8));
    pal.setColor(QPalette::Highlight, highlight);
    pal.setColor(QPalette::HighlightedText, highlightText);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Dark, blend(panel, Qt::black, 0.42));
    pal.setColor(QPalette::Light, blend(button, Qt::white, 0.14));
    pal.setColor(QPalette::Midlight, blend(button, Qt::white, 0.08));
    pal.setColor(QPalette::Shadow, blend(panel, Qt::black, 0.68));
    return pal;
}

QIcon standardToolbarIcon(QStyle::StandardPixmap pixmap) {
    return QApplication::style() ? QApplication::style()->standardIcon(pixmap) : QIcon();
}

void configureToolbar(QToolBar* toolbar, Qt::ToolButtonStyle style) {
    if (!toolbar) {
        return;
    }
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(style);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setContentsMargins(6, 4, 6, 4);
    if (auto* layout = toolbar->layout()) {
        layout->setSpacing(6);
    }
}

void setToolbarIcon(QAction* action, QStyle::StandardPixmap pixmap) {
    if (action) {
        action->setIcon(standardToolbarIcon(pixmap));
    }
}

QString materialIconAssetPath(const QString& name) {
    const QString sourceDir = QString::fromUtf8(BADGEEDITOR_SOURCE_DIR);
    return QDir(sourceDir).filePath(QStringLiteral("assets/material-icons/%1.svg").arg(name));
}

QString materialIconResourcePath(const QString& name) {
    return QStringLiteral(":/material-icons/%1.svg").arg(name);
}

QIcon loadMaterialIcon(const QString& name) {
    static QHash<QString, QIcon> cache;
    const auto it = cache.constFind(name);
    if (it != cache.cend()) {
        return *it;
    }

    QIcon icon;
    const QString resourcePath = materialIconResourcePath(name);
    const QString sourcePath = materialIconAssetPath(name);
    const auto tryLoadSvg = [&icon](const QString& path) {
        if (!QFile::exists(path)) {
            return false;
        }
#ifdef BADGEEDITOR_HAS_QTSVG
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            constexpr int kIconSizePx = 128;
            QPixmap pixmap(kIconSizePx, kIconSizePx);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            icon = QIcon(pixmap);
            return true;
        }
#else
        icon = QIcon(path);
        return !icon.isNull();
#endif
        return false;
    };

    if (!tryLoadSvg(resourcePath)) {
        tryLoadSvg(sourcePath);
    }

    if (icon.isNull()) {
        icon = standardToolbarIcon(QStyle::SP_FileIcon);
    }
    cache.insert(name, icon);
    return icon;
}

void setMaterialIcon(QAction* action, const QString& name) {
    if (action) {
        action->setIcon(loadMaterialIcon(name));
        action->setIconVisibleInMenu(true);
    }
}

class DocumentSnapshotCommand final : public QUndoCommand {
public:
    static constexpr int kCommandId = 0x42454745;

    DocumentSnapshotCommand(QString label,
                            QList<BadgeItem> beforeBadges,
                            QList<int> beforeSelection,
                            QList<BadgeItem> afterBadges,
                            QList<int> afterSelection,
                            bool mergeable,
                            std::function<void(const QList<BadgeItem>&, const QList<int>&)> apply)
        : m_beforeBadges(std::move(beforeBadges)),
          m_beforeSelection(std::move(beforeSelection)),
          m_afterBadges(std::move(afterBadges)),
          m_afterSelection(std::move(afterSelection)),
          m_mergeable(mergeable),
          m_apply(std::move(apply)) {
        setText(std::move(label));
    }

    int id() const override {
        return m_mergeable ? kCommandId : -1;
    }

    bool mergeWith(const QUndoCommand* other) override {
        if (!m_mergeable) {
            return false;
        }
        const auto* command = dynamic_cast<const DocumentSnapshotCommand*>(other);
        if (!command || !command->m_mergeable) {
            return false;
        }
        if (text() != command->text()) {
            return false;
        }
        if (m_afterSelection != command->m_beforeSelection || !badgeListEqualsLocal(m_afterBadges, command->m_beforeBadges)) {
            return false;
        }

        m_afterBadges = command->m_afterBadges;
        m_afterSelection = command->m_afterSelection;
        return true;
    }

    void undo() override {
        m_apply(m_beforeBadges, m_beforeSelection);
    }

    void redo() override {
        m_apply(m_afterBadges, m_afterSelection);
    }

private:
    static bool layerEqualsLocal(const LayerItem& a, const LayerItem& b) {
        return a.imagePath == b.imagePath
            && a.name == b.name
            && a.opacity == b.opacity
            && a.visible == b.visible
            && a.offsetX == b.offsetX
            && a.offsetY == b.offsetY
            && a.blendMode == b.blendMode
            && a.fillColor == b.fillColor;
    }

    static bool guideEqualsLocal(const GuideItemData& a, const GuideItemData& b) {
        return a.shape == b.shape
            && a.bleedMm == b.bleedMm
            && a.safeInsetMm == b.safeInsetMm
            && a.cornerRadiusMm == b.cornerRadiusMm;
    }

    static bool badgeEqualsLocal(const BadgeItem& a, const BadgeItem& b) {
        return a.productMode == b.productMode
            && guideEqualsLocal(a.guide, b.guide)
            && a.widthMm == b.widthMm
            && a.heightMm == b.heightMm
            && a.imageScale == b.imageScale
            && a.materialPreset == b.materialPreset
            && a.specularStrength == b.specularStrength
            && a.envReflectionStrength == b.envReflectionStrength
            && a.glitterStrength == b.glitterStrength
            && a.xMm == b.xMm
            && a.yMm == b.yMm
            && a.rotation == b.rotation
            && a.label == b.label
            && a.imagePath == b.imagePath
            && a.displayText == b.displayText
            && a.clipToCircle == b.clipToCircle
            && a.brightness == b.brightness
            && a.contrast == b.contrast
            && a.saturation == b.saturation
            && a.flattenedForLayoutTransfer == b.flattenedForLayoutTransfer
            && a.isSelected == b.isSelected
            && a.layers.size() == b.layers.size()
            && std::equal(a.layers.begin(), a.layers.end(), b.layers.begin(), layerEqualsLocal);
    }

    static bool badgeListEqualsLocal(const QList<BadgeItem>& a, const QList<BadgeItem>& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (int i = 0; i < a.size(); ++i) {
            if (!badgeEqualsLocal(a[i], b[i])) {
                return false;
            }
        }
        return true;
    }

    QList<BadgeItem> m_beforeBadges;
    QList<int> m_beforeSelection;
    QList<BadgeItem> m_afterBadges;
    QList<int> m_afterSelection;
    bool m_mergeable = false;
    std::function<void(const QList<BadgeItem>&, const QList<int>&)> m_apply;
};

QList<int> indicesForSelection(const QList<BadgeGraphicItem*>& selected, const QList<BadgeGraphicItem*>& all) {
    QList<int> indices;
    for (auto* item : selected) {
        const int idx = all.indexOf(item);
        if (idx >= 0) {
            indices.append(idx);
        }
    }
    return indices;
}

bool badgeLayerEquals(const LayerItem& a, const LayerItem& b) {
    return a.imagePath == b.imagePath
        && a.name == b.name
        && a.opacity == b.opacity
        && a.visible == b.visible
        && a.offsetX == b.offsetX
        && a.offsetY == b.offsetY
        && a.blendMode == b.blendMode
        && a.fillColor == b.fillColor;
}

bool badgeGuideEquals(const GuideItemData& a, const GuideItemData& b) {
    return a.shape == b.shape
        && a.bleedMm == b.bleedMm
        && a.safeInsetMm == b.safeInsetMm
        && a.cornerRadiusMm == b.cornerRadiusMm;
}

bool badgeEquals(const BadgeItem& a, const BadgeItem& b) {
    return a.productMode == b.productMode
        && badgeGuideEquals(a.guide, b.guide)
        && a.widthMm == b.widthMm
        && a.heightMm == b.heightMm
        && a.imageScale == b.imageScale
        && a.materialPreset == b.materialPreset
        && a.specularStrength == b.specularStrength
        && a.envReflectionStrength == b.envReflectionStrength
        && a.glitterStrength == b.glitterStrength
        && a.xMm == b.xMm
        && a.yMm == b.yMm
        && a.rotation == b.rotation
        && a.label == b.label
        && a.imagePath == b.imagePath
        && a.displayText == b.displayText
        && a.clipToCircle == b.clipToCircle
        && a.brightness == b.brightness
        && a.contrast == b.contrast
        && a.saturation == b.saturation
        && a.flattenedForLayoutTransfer == b.flattenedForLayoutTransfer
        && a.isSelected == b.isSelected
        && a.layers.size() == b.layers.size()
        && std::equal(a.layers.begin(), a.layers.end(), b.layers.begin(), badgeLayerEquals);
}

bool badgeListEquals(const QList<BadgeItem>& a, const QList<BadgeItem>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i) {
        if (!badgeEquals(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

BadgeItem badgeForLayoutPreview(BadgeItem badge) {
    // The current Designer guides describe a circular badge with bleed outside the finish size.
    badge.clipToCircle = true;
    return badge;
}

double badgeFootprintScore(const BadgeItem& badge) {
    const double maxSide = std::max(badge.widthMm, badge.heightMm);
    const double width = badge.clipToCircle ? maxSide + 3.0 : badge.widthMm;
    const double height = badge.clipToCircle ? maxSide + 3.0 : badge.heightMm;
    return width * height;
}

BadgeItem badgeForLayoutTransfer(BadgeItem badge) {
    // Layout transfer must carry only the raw badge content, not the Designer preview effects.
    badge.clipToCircle = true;
    return badge;
}

QRectF badgeContentRectPx(const BadgeItem& badge) {
    const double pxPerMm = Constants::kMmToPx;
    const double margin = badge.isSelected ? 3.0 : 2.0;
    return QRectF(margin, margin, badge.widthMm * pxPerMm, badge.heightMm * pxPerMm);
}

QRectF badgePrimaryImageRectPx(const BadgeItem& badge, const QSizeF& sourceSize) {
    const QRectF content = badgeContentRectPx(badge);
    const double scale = std::max(0.1, badge.imageScale);
    const QRectF targetRect(content.center().x() - content.width() * scale * 0.5,
                            content.center().y() - content.height() * scale * 0.5,
                            content.width() * scale,
                            content.height() * scale);
    QRectF imageRect = fitRectInside(targetRect, sourceSize);
    if (!badge.layers.isEmpty()) {
        imageRect.translate(badge.layers.first().offsetX * Constants::kMmToPx,
                            badge.layers.first().offsetY * Constants::kMmToPx);
    }
    return imageRect;
}

QPixmap correctedPixmapForBadge(const BadgeItem& badge, const QString& path) {
    if (path.isEmpty()) {
        return {};
    }
    QString colorSpaceLabel;
    const QImage loaded = ImageProcessor::loadImage(path, &colorSpaceLabel);
    if (loaded.isNull()) {
        return {};
    }
    QPixmap pixmap = QPixmap::fromImage(loaded);
    if (badge.brightness != 0.0 || badge.contrast != 0.0 || badge.saturation != 0.0) {
        pixmap = ImageProcessor::applyCorrection(pixmap, badge.brightness, badge.contrast, badge.saturation);
    }
    return pixmap;
}

QPixmap applyLayerFillColor(QPixmap pixmap, const QColor& fillColor) {
    if (pixmap.isNull() || !fillColor.isValid()) {
        return pixmap;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return pixmap;
    }

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), fillColor);
    painter.end();
    return QPixmap::fromImage(image);
}

QPixmap renderedLayerPixmap(const BadgeItem& badge, const LayerItem& layer) {
    return applyLayerFillColor(correctedPixmapForBadge(badge, layer.imagePath), layer.fillColor);
}

QPainter::CompositionMode compositionModeForLayer(LayerBlendMode mode) {
    switch (mode) {
    case LayerBlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case LayerBlendMode::Screen: return QPainter::CompositionMode_Screen;
    case LayerBlendMode::Overlay: return QPainter::CompositionMode_Overlay;
    case LayerBlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
    case LayerBlendMode::Add: return QPainter::CompositionMode_Plus;
    case LayerBlendMode::Normal:
    default:
        return QPainter::CompositionMode_SourceOver;
    }
}

QString layerBlendModeText(LayerBlendMode mode) {
    switch (mode) {
    case LayerBlendMode::Multiply: return QStringLiteral("Multiply");
    case LayerBlendMode::Screen: return QStringLiteral("Screen");
    case LayerBlendMode::Overlay: return QStringLiteral("Overlay");
    case LayerBlendMode::SoftLight: return QStringLiteral("Soft Light");
    case LayerBlendMode::Add: return QStringLiteral("Add");
    case LayerBlendMode::Normal:
    default:
        return QStringLiteral("Normal");
    }
}

QString layerItemSummary(const LayerItem& layer) {
    QString summary = QStringLiteral("%1  [%2, %3%]")
        .arg(layer.name.isEmpty() ? QFileInfo(layer.imagePath).baseName() : layer.name,
             layerBlendModeText(layer.blendMode),
             QString::number(int(std::round(std::clamp(layer.opacity, 0.0, 1.0) * 100.0))));
    if (layer.fillColor.isValid()) {
        summary += QStringLiteral(" %1").arg(layer.fillColor.name(QColor::HexArgb).toUpper());
    }
    return summary;
}

QString badgeSizeText(const BadgeItem& badge) {
    return QStringLiteral("%1 × %2 mm")
        .arg(QString::number(std::max(0.0, badge.widthMm), 'f', 1),
             QString::number(std::max(0.0, badge.heightMm), 'f', 1));
}

QString layoutOverflowSummary(const QList<BadgeItem>& sourceBadges,
                              int placedCount,
                              const PaperConfig& paper,
                              bool autoLayoutMode) {
    const int overflowCount = std::max(0, int(sourceBadges.size()) - placedCount);
    if (overflowCount <= 0) {
        return QString();
    }

    const double innerWidth = std::max(0.0, paper.widthMm - paper.marginMm * 2.0);
    const double innerHeight = std::max(0.0, paper.heightMm - paper.marginMm * 2.0);
    if (sourceBadges.size() == 1 && placedCount == 0) {
        return QStringLiteral("テンプレート %1 が用紙内寸 %2 × %3 mm に収まりませんでした")
            .arg(badgeSizeText(sourceBadges.first()),
                 QString::number(innerWidth, 'f', 1),
                 QString::number(innerHeight, 'f', 1));
    }

    const BadgeItem& firstOverflow = sourceBadges[placedCount];
    const QString extra = autoLayoutMode
        ? QStringLiteral("自動配置の結果、%1 件が収まりませんでした")
        : QStringLiteral("配置結果の一部が収まりませんでした");
    return QStringLiteral("%1。最初の未配置サイズ: %2")
        .arg(extra.arg(overflowCount), badgeSizeText(firstOverflow));
}

LayerBlendMode inferLayerBlendMode(const QString& path, const QString& name) {
    const QString haystack = (name + QLatin1Char(' ') + QFileInfo(path).baseName()).toLower();
    if (haystack.contains("shadow") || haystack.contains("shade") || haystack.contains("dark")
        || haystack.contains("mask") || haystack.contains("ink") || haystack.contains("line")) {
        return LayerBlendMode::Multiply;
    }
    if (haystack.contains("spark") || haystack.contains("flare") || haystack.contains("add")) {
        return LayerBlendMode::Add;
    }
    if (haystack.contains("glow") || haystack.contains("light") || haystack.contains("shine")
        || haystack.contains("highlight") || haystack.contains("specular")) {
        return LayerBlendMode::Screen;
    }

    const QImage image = ImageProcessor::loadImage(path, nullptr);
    if (image.isNull()) {
        return LayerBlendMode::Normal;
    }

    const QImage sample = image.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    double luminanceSum = 0.0;
    double alphaSum = 0.0;
    const int pixelCount = std::max(1, sample.width() * sample.height());
    for (int y = 0; y < sample.height(); ++y) {
        for (int x = 0; x < sample.width(); ++x) {
            const QColor color = sample.pixelColor(x, y);
            const double alpha = color.alphaF();
            luminanceSum += color.redF() * 0.2126 + color.greenF() * 0.7152 + color.blueF() * 0.0722;
            alphaSum += alpha;
        }
    }
    const double avgLum = luminanceSum / pixelCount;
    const double avgAlpha = alphaSum / pixelCount;
    if (avgAlpha > 0.25 && avgLum < 0.35) {
        return LayerBlendMode::Multiply;
    }
    if (avgLum > 0.78) {
        return LayerBlendMode::Screen;
    }
    return LayerBlendMode::Normal;
}

QPixmap renderLayerPreviewPixmap(const BadgeItem& badge, int layerIndex, const QPalette& palette, int sizePx = 128) {
    QPixmap preview(sizePx, sizePx);
    preview.fill(Qt::transparent);

    QPainter painter(&preview);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QColor windowColor = palette.color(QPalette::Window);
    const QColor baseColor = palette.color(QPalette::Base);
    const QColor a = blend(windowColor, baseColor, 0.15);
    const QColor b = blend(windowColor, baseColor, 0.28);
    const int tile = 12;
    for (int y = 0; y < sizePx; y += tile) {
        for (int x = 0; x < sizePx; x += tile) {
            painter.fillRect(QRect(x, y, tile, tile), ((x / tile + y / tile) & 1) ? b : a);
        }
    }

    if (layerIndex < 0 || layerIndex >= badge.layers.size()) {
        painter.setPen(palette.color(QPalette::WindowText));
        painter.drawText(preview.rect(), Qt::AlignCenter, QStringLiteral("No Preview"));
        return preview;
    }

    BadgeItem previewBadge = badge;
    previewBadge.isSelected = false;

    const QRectF contentRect = badgeContentRectPx(previewBadge);
    const QRectF targetBounds = preview.rect().adjusted(8, 8, -8, -8);
    const qreal scale = std::min(targetBounds.width() / std::max(1.0, contentRect.width()),
                                 targetBounds.height() / std::max(1.0, contentRect.height()));
    const QSizeF scaledSize(contentRect.width() * scale, contentRect.height() * scale);
    const QRectF targetRect(QPointF(targetBounds.center().x() - scaledSize.width() * 0.5,
                                    targetBounds.center().y() - scaledSize.height() * 0.5),
                            scaledSize);

    painter.save();
    painter.translate(targetRect.left(), targetRect.top());
    painter.scale(scale, scale);
    painter.translate(-contentRect.left(), -contentRect.top());

    if (previewBadge.clipToCircle) {
        QPainterPath path;
        path.addEllipse(contentRect);
        painter.setClipPath(path);
    }

    const LayerItem* primaryLayer = previewBadge.layers.isEmpty() ? nullptr : &previewBadge.layers.first();
    const QPixmap basePixmap = primaryLayer
        ? renderedLayerPixmap(previewBadge, *primaryLayer)
        : correctedPixmapForBadge(previewBadge, previewBadge.imagePath);
    if (!basePixmap.isNull() && (!primaryLayer || primaryLayer->visible)) {
        painter.save();
        painter.setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
        painter.setCompositionMode(primaryLayer ? compositionModeForLayer(primaryLayer->blendMode) : QPainter::CompositionMode_SourceOver);
        const QRectF imageRect = badgePrimaryImageRectPx(previewBadge, basePixmap.size());
        painter.drawPixmap(imageRect, basePixmap, QRectF(basePixmap.rect()));
        painter.restore();
    }

    const int startLayer = previewBadge.layers.isEmpty() ? 0 : 1;
    for (int i = startLayer; i < previewBadge.layers.size(); ++i) {
        const auto& layer = previewBadge.layers[i];
        if (!layer.visible) {
            continue;
        }
        const QPixmap layerPixmap = renderedLayerPixmap(previewBadge, layer);
        if (layerPixmap.isNull()) {
            continue;
        }
        painter.save();
        painter.setOpacity(layer.opacity);
        painter.setCompositionMode(compositionModeForLayer(layer.blendMode));
        const QRectF layerRect = fitRectInside(contentRect.translated(layer.offsetX * Constants::kMmToPx,
                                                                      layer.offsetY * Constants::kMmToPx),
                                               layerPixmap.size());
        painter.drawPixmap(layerRect, layerPixmap, QRectF(layerPixmap.rect()));
        painter.restore();
    }

    painter.setOpacity(1.0);
    if (!previewBadge.displayText.isEmpty()) {
        QFont font("Arial", 10);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(contentRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, previewBadge.displayText);
        painter.setPen(Qt::black);
        painter.drawText(contentRect, Qt::AlignCenter, previewBadge.displayText);
    }

    const LayerItem& selectedLayer = previewBadge.layers[layerIndex];
    const QString labelText = QStringLiteral("%1\n%2")
        .arg(selectedLayer.name.isEmpty() ? QFileInfo(selectedLayer.imagePath).baseName() : selectedLayer.name,
             layerItemSummary(selectedLayer));
    const QFont labelFont = painter.font();
    const QFontMetrics metrics(labelFont);
    const QRect textBounds = metrics.boundingRect(QRect(0, 0, sizePx - 20, sizePx), Qt::TextWordWrap, labelText);
    const QRect chipRect(8, 8, textBounds.width() + 14, textBounds.height() + 12);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 22, 170));
    painter.drawRoundedRect(chipRect, 8, 8);
    painter.setPen(QColor(255, 255, 255, 230));
    painter.drawText(chipRect.adjusted(7, 6, -7, -6), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, labelText);
    painter.restore();
    painter.end();
    return preview;
}

void paintTransferBadgeContent(QPainter& painter, const BadgeItem& badge) {
    const QRectF contentRect = badgeContentRectPx(badge);
    const LayerItem* primaryLayer = badge.layers.isEmpty() ? nullptr : &badge.layers.first();
    const QPixmap basePixmap = primaryLayer
        ? renderedLayerPixmap(badge, *primaryLayer)
        : correctedPixmapForBadge(badge, badge.imagePath);
    if (!basePixmap.isNull() && (!primaryLayer || primaryLayer->visible)) {
        painter.save();
        painter.setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
        painter.setCompositionMode(primaryLayer ? compositionModeForLayer(primaryLayer->blendMode) : QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(badgePrimaryImageRectPx(badge, basePixmap.size()), basePixmap, QRectF(basePixmap.rect()));
        painter.restore();
    }

    const int startLayer = badge.layers.isEmpty() ? 0 : 1;
    for (int i = startLayer; i < badge.layers.size(); ++i) {
        const auto& layer = badge.layers[i];
        if (!layer.visible) {
            continue;
        }
        const QPixmap layerPixmap = renderedLayerPixmap(badge, layer);
        if (layerPixmap.isNull()) {
            continue;
        }
        painter.save();
        painter.setOpacity(layer.opacity);
        painter.setCompositionMode(compositionModeForLayer(layer.blendMode));
        const QRectF layerRect = fitRectInside(contentRect.translated(layer.offsetX * Constants::kMmToPx,
                                                                      layer.offsetY * Constants::kMmToPx),
                                               layerPixmap.size());
        painter.drawPixmap(layerRect, layerPixmap, QRectF(layerPixmap.rect()));
        painter.restore();
    }

    painter.setOpacity(1.0);
    if (!badge.displayText.isEmpty()) {
        QFont font("Arial", 10);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(contentRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, badge.displayText);
        painter.setPen(Qt::black);
        painter.drawText(contentRect, Qt::AlignCenter, badge.displayText);
    }
}

QImage renderTransferCropImage(DesignerWidget& designer, BadgeGraphicItem& item, const QPointF& guideCenterScene, double guideSizeMm) {
    Q_UNUSED(designer);
    const double pxPerMm = Constants::kMmToPx;
    const double transferDiameterMm = std::max(0.1, guideSizeMm);
    const int transferDiameterPx = std::max(1, int(std::round(transferDiameterMm * pxPerMm)));
    const QRectF transferRectScene(guideCenterScene.x() - transferDiameterPx * 0.5,
                                   guideCenterScene.y() - transferDiameterPx * 0.5,
                                   transferDiameterPx,
                                   transferDiameterPx);

    QImage image(QSize(transferDiameterPx, transferDiameterPx), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip;
    clip.addEllipse(QRectF(0.0, 0.0, transferDiameterPx, transferDiameterPx));
    painter.setClipPath(clip);
    QTransform world;
    world.translate(-transferRectScene.left(), -transferRectScene.top());
    world *= item.sceneTransform();
    painter.setWorldTransform(world);
    paintTransferBadgeContent(painter, item.badge());
    painter.end();
    return image;
}

QString writeLayoutTransferImage(const QImage& image, const QString& stem) {
    static QTemporaryDir transferDir(QDir::tempPath() + "/BadgeEditorQt-transfer-XXXXXX");
    if (!transferDir.isValid()) {
        return {};
    }
    static int counter = 0;
    QString safeStem = stem.isEmpty() ? QStringLiteral("badge") : stem;
    for (QChar& ch : safeStem) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    const QString path = transferDir.path() + QStringLiteral("/%1_%2.png")
                             .arg(safeStem)
                             .arg(++counter);
    if (!image.save(path)) {
        return {};
    }
    return path;
}

QImage renderLayoutDebugImage(const BadgeItem& badge, int targetPx) {
    if (targetPx <= 0) {
        return {};
    }

    QImage canvas(QSize(targetPx, targetPx), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(badge.flattenedForLayoutTransfer ? Qt::transparent : Qt::white);

    const LayerItem* primaryLayer = badge.layers.isEmpty() ? nullptr : &badge.layers.first();
    const QString primaryPath = primaryLayer ? primaryLayer->imagePath : badge.imagePath;
    if (primaryPath.isEmpty()) {
        return canvas;
    }

    QString colorSpaceLabel;
    const QImage source = ImageProcessor::loadImage(primaryPath, &colorSpaceLabel);
    if (source.isNull()) {
        return canvas;
    }

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPixmap sourcePixmap = primaryLayer
        ? renderedLayerPixmap(badge, *primaryLayer)
        : QPixmap::fromImage(source);
    if (sourcePixmap.isNull()) {
        return canvas;
    }

    painter.drawImage(fitRectInside(QRectF(0.0, 0.0, targetPx, targetPx), sourcePixmap.size()),
                      sourcePixmap.toImage(),
                      QRectF(sourcePixmap.rect()));
    painter.end();
    return canvas;
}

BadgeItem makeLayoutTransferBadge(const BadgeItem& badge, const QImage& crop, double guideSizeMm) {
    BadgeItem out = badgeForLayoutTransfer(badge);
    out.widthMm = std::max(0.1, guideSizeMm);
    out.heightMm = std::max(0.1, guideSizeMm);
    out.xMm = 0.0;
    out.yMm = 0.0;
    out.imageScale = 1.0;
    out.displayText.clear();
    out.brightness = 0.0;
    out.contrast = 0.0;
    out.saturation = 0.0;
    out.flattenedForLayoutTransfer = true;
    out.layers.clear();
    out.imagePath.clear();

    const QString tempPath = writeLayoutTransferImage(crop, badge.label.isEmpty() ? QStringLiteral("layout") : badge.label);
    if (!tempPath.isEmpty()) {
        out.layers.append(layerFromImagePath(tempPath));
    }
    return out;
}

void setPrimaryImageLayer(BadgeItem& badge, const QString& path) {
    const LayerItem layer = layerFromImagePath(path);
    if (badge.layers.isEmpty()) {
        badge.layers.append(layer);
    } else {
        badge.layers[0] = layer;
    }
    badge.imagePath.clear();
}

int badgeSizePresetIndex(double widthMm, double heightMm) {
    if (std::abs(widthMm - heightMm) > 0.1) {
        return 0;
    }

    struct Preset {
        double sizeMm;
        int index;
    };
    constexpr Preset presets[] = {
        {25.0, 1},
        {32.0, 2},
        {44.0, 3},
        {57.0, 4},
        {65.0, 5},
        {76.0, 6},
        {100.0, 7},
    };

    for (const auto& preset : presets) {
        if (std::abs(widthMm - preset.sizeMm) <= 0.1) {
            return preset.index;
        }
    }
    return 0;
}

double badgeGuideSizeMm(const BadgeItem& badge) {
    const double widthMm = std::max(0.1, badge.widthMm);
    const double heightMm = std::max(0.1, badge.heightMm);
    if (std::abs(widthMm - heightMm) > 0.1) {
        return std::min(widthMm, heightMm);
    }
    return widthMm;
}

double activeGuideSizeMmFromBadges(const QList<BadgeItem>& badges, const QList<BadgeGraphicItem*>& selected) {
    if (!selected.isEmpty() && selected.first()) {
        return badgeGuideSizeMm(selected.first()->badge());
    }
    if (!badges.isEmpty()) {
        return badgeGuideSizeMm(badges.first());
    }
    return 32.0;
}

bool badgeInsideCentralGuide(const BadgeItem& badge, double guideSizeMm) {
    const double visibleRadiusMm = std::max(0.0, guideSizeMm - 4.0) * 0.5;
    if (visibleRadiusMm <= 0.0) {
        return true;
    }
    const double contentW = std::max(0.1, badge.widthMm * std::max(0.1, badge.imageScale));
    const double contentH = std::max(0.1, badge.heightMm * std::max(0.1, badge.imageScale));
    const double left = badge.xMm;
    const double top = badge.yMm;
    const double right = left + contentW;
    const double bottom = top + contentH;

    const double closestX = std::clamp(0.0, left, right);
    const double closestY = std::clamp(0.0, top, bottom);
    return (closestX * closestX + closestY * closestY) <= (visibleRadiusMm * visibleRadiusMm);
}

QList<BadgeItem> badgesInsideCentralGuide(const QList<BadgeItem>& badges, double guideSizeMm) {
    QList<BadgeItem> filtered;
    for (const auto& badge : badges) {
        if (badgeInsideCentralGuide(badge, guideSizeMm)) {
            filtered.append(badge);
        }
    }
    return filtered;
}

QList<BadgeItem> offsetLayoutPage(const QList<BadgeItem>& page, double dxMm, double dyMm) {
    QList<BadgeItem> shifted = page;
    for (auto& badge : shifted) {
        badge.xMm += dxMm;
        badge.yMm += dyMm;
    }
    return shifted;
}

struct MaterialPresetDefaults {
    int specular;
    int env;
    int glitter;
    bool glitterEnabled;
};

MaterialPresetDefaults materialDefaults(int preset) {
    switch (preset) {
    case 1: return {22, 10, 0, false};   // matte
    case 2: return {42, 28, 8, false};   // hologram
    case 3: return {32, 18, 0, false};   // pearl
    case 4: return {38, 22, 18, false};  // glitter
    default: return {48, 24, 6, false};  // gloss
    }
}

void configurePrinterForDocument(QPrinter& printer, const badge::DocumentData& document, int resolution) {
    printer.setResolution(std::max(72, resolution));
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QSizeF(document.paper.widthMm, document.paper.heightMm), QPageSize::Millimeter));
    printer.setPageOrientation(document.paper.widthMm >= document.paper.heightMm
                                   ? QPageLayout::Landscape
                                   : QPageLayout::Portrait);
}

}

#ifdef Q_OS_WIN
static void applyWinBackdrop(HWND hwnd, bool darkMode) {
    if (!hwnd) {
        return;
    }

    const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    const BOOL dark = darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Badge Editor Pro");
    resize(1300, 900);
    setAcceptDrops(true);
    m_undoStack = new QUndoStack(this);
    m_windowsIntegration = new WindowsIntegration(this);
    m_windowsIntegration->initialize(QStringLiteral("BadgeEditorQt.BadgeEditorQt"));
    QFont appFont = QApplication::font();
    appFont.setPointSize(appFont.pointSize() + 1);
    QApplication::setFont(appFont);

    // --- Menu ---
    auto* fileMenu = menuBar()->addMenu("ファイル(&F)");
    auto* actNew = fileMenu->addAction("新規(&N)", this, &MainWindow::onNew, QKeySequence::New);
    auto* actOpen = fileMenu->addAction("開く(&O)...", this, &MainWindow::onOpen, QKeySequence::Open);
    auto* actSave = fileMenu->addAction("保存(&S)", this, &MainWindow::onSave, QKeySequence::Save);
    auto* actSaveAs = fileMenu->addAction("名前を付けて保存(&A)...", this, &MainWindow::onSaveAs, QKeySequence("Ctrl+Shift+S"));
    fileMenu->addSeparator();
    auto* actExportPdf = fileMenu->addAction("PDF出力...", this, &MainWindow::onExportPdf);
    auto* actExportPng = fileMenu->addAction("画像出力...", this, &MainWindow::onExportPng);
    auto* actPrintPreview = fileMenu->addAction("印刷プレビュー...", this, &MainWindow::onPrintPreview, QKeySequence("Ctrl+Shift+P"));
    auto* actPrint = fileMenu->addAction("印刷...", this, &MainWindow::onPrint, QKeySequence::Print);
    setMaterialIcon(actNew, QStringLiteral("add_circle"));
    setMaterialIcon(actOpen, QStringLiteral("folder_open"));
    setMaterialIcon(actSave, QStringLiteral("save"));
    setMaterialIcon(actSaveAs, QStringLiteral("save"));
    setMaterialIcon(actExportPdf, QStringLiteral("picture_as_pdf"));
    setMaterialIcon(actExportPng, QStringLiteral("image"));
    setMaterialIcon(actPrintPreview, QStringLiteral("print"));
    setMaterialIcon(actPrint, QStringLiteral("print"));

    auto* editMenu = menuBar()->addMenu("編集(&E)");
    auto* actUndo = editMenu->addAction("元に戻す(&U)", this, &MainWindow::onUndo, QKeySequence::Undo);
    auto* actRedo = editMenu->addAction("やり直し(&R)", this, &MainWindow::onRedo, QKeySequence::Redo);
    editMenu->addSeparator();
    auto* actDelete = editMenu->addAction("削除", this, &MainWindow::onDelete, QKeySequence::Delete);
    editMenu->addSeparator();
    auto* actAlignLeft = editMenu->addAction("左揃え", this, &MainWindow::onAlignLeft, QKeySequence("Ctrl+Alt+Left"));
    auto* actAlignHCenter = editMenu->addAction("中央揃え(横)", this, &MainWindow::onAlignHCenter, QKeySequence("Ctrl+Alt+H"));
    auto* actAlignRight = editMenu->addAction("右揃え", this, &MainWindow::onAlignRight, QKeySequence("Ctrl+Alt+Right"));
    auto* actAlignTop = editMenu->addAction("上揃え", this, &MainWindow::onAlignTop, QKeySequence("Ctrl+Alt+Up"));
    auto* actAlignVCenter = editMenu->addAction("中央揃え(縦)", this, &MainWindow::onAlignVCenter, QKeySequence("Ctrl+Alt+V"));
    auto* actAlignBottom = editMenu->addAction("下揃え", this, &MainWindow::onAlignBottom, QKeySequence("Ctrl+Alt+Down"));
    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, [this]{ onDelete(); });
    auto* layoutHomeShortcut = new QShortcut(QKeySequence(Qt::Key_Home), this);
    connect(layoutHomeShortcut, &QShortcut::activated, this, [this]{
        if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
            return;
        }
        m_layoutPageIndex = 0;
        requestLayoutRefresh("page selected");
        flushInternalEvents();
    });
    auto* layoutEndShortcut = new QShortcut(QKeySequence(Qt::Key_End), this);
    connect(layoutEndShortcut, &QShortcut::activated, this, [this]{
        if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
            return;
        }
        m_layoutPageIndex = m_layoutPages.size() - 1;
        requestLayoutRefresh("page selected");
        flushInternalEvents();
    });
    setMaterialIcon(actUndo, QStringLiteral("undo"));
    setMaterialIcon(actRedo, QStringLiteral("redo"));
    setMaterialIcon(actDelete, QStringLiteral("delete"));
    setMaterialIcon(actAlignLeft, QStringLiteral("format_align_left"));
    setMaterialIcon(actAlignHCenter, QStringLiteral("format_align_center"));
    setMaterialIcon(actAlignRight, QStringLiteral("format_align_right"));
    setMaterialIcon(actAlignTop, QStringLiteral("vertical_align_top"));
    setMaterialIcon(actAlignVCenter, QStringLiteral("vertical_align_center"));
    setMaterialIcon(actAlignBottom, QStringLiteral("vertical_align_bottom"));

    auto* viewMenu = menuBar()->addMenu("表示(&V)");
    auto* actViewZoomIn = viewMenu->addAction("ズームイン", this, [this]{ m_zoomScale *= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    auto* actViewZoomOut = viewMenu->addAction("ズームアウト", this, [this]{ m_zoomScale /= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    viewMenu->addSeparator();
    m_actTheme = viewMenu->addAction("テーマ切替", this, &MainWindow::onToggleTheme);
    m_actAppSettings = viewMenu->addAction("アプリ設定...", this, &MainWindow::onAppSettings, QKeySequence("Ctrl+,"));
    m_actGridVisible = viewMenu->addAction("グリッド表示");
    m_actGridVisible->setCheckable(true);
    m_actGridVisible->setChecked(true);
    connect(m_actGridVisible, &QAction::toggled, this, [this](bool on){ onToggleGrid(on); });
    m_actSnapToGrid = viewMenu->addAction("グリッドにスナップ");
    m_actSnapToGrid->setCheckable(true);
    m_actSnapToGrid->setChecked(true);
    connect(m_actSnapToGrid, &QAction::toggled, this, [this](bool on){ onToggleSnapToGrid(on); });
    m_actTransferDebug = viewMenu->addAction("転送デバッグ", this, &MainWindow::onShowTransferDebug);
    viewMenu->addAction("ドック配置を初期化", this, &MainWindow::resetDockState);
    viewMenu->addAction("診断パネルを表示", this, [this] {
        if (m_logDock) {
            m_logDock->setVisible(true);
            m_logDock->setAsCurrentTab();
        }
    });
    auto* perspectiveMenu = viewMenu->addMenu("Perspective");
    m_perspectiveMenu = perspectiveMenu;
    auto* actSaveDesignerPerspective = perspectiveMenu->addAction("現在を編集として保存", this, &MainWindow::saveDesignerPerspective);
    auto* actSaveLayoutPerspective = perspectiveMenu->addAction("現在を配置確認として保存", this, &MainWindow::saveLayoutPerspective);
    auto* actSavePerspectiveAs = perspectiveMenu->addAction("名前を付けて保存...", this, &MainWindow::savePerspectiveAs);
    perspectiveMenu->addSeparator();
    auto* actOpenDesignerPerspectiveMenu = perspectiveMenu->addAction("編集を開く", this, &MainWindow::openDesignerPerspective);
    auto* actOpenLayoutPerspectiveMenu = perspectiveMenu->addAction("配置確認を開く", this, &MainWindow::openLayoutPerspective);
    setMaterialIcon(actViewZoomIn, QStringLiteral("zoom_in"));
    setMaterialIcon(actViewZoomOut, QStringLiteral("zoom_out"));
    setMaterialIcon(m_actTheme, QStringLiteral("dark_mode"));
    setMaterialIcon(m_actAppSettings, QStringLiteral("settings"));
    setMaterialIcon(m_actGridVisible, QStringLiteral("grid_on"));
    setMaterialIcon(m_actSnapToGrid, QStringLiteral("center_focus_strong"));
    setMaterialIcon(m_actTransferDebug, QStringLiteral("transfer_debug"));
    setMaterialIcon(actSaveDesignerPerspective, QStringLiteral("save"));
    setMaterialIcon(actSaveLayoutPerspective, QStringLiteral("save"));
    setMaterialIcon(actSavePerspectiveAs, QStringLiteral("save"));
    setMaterialIcon(actOpenDesignerPerspectiveMenu, QStringLiteral("edit"));
    setMaterialIcon(actOpenLayoutPerspectiveMenu, QStringLiteral("grid_view"));

    // --- Toolbars ---
    const QFont uiFont = QApplication::font();
    menuBar()->setFont(uiFont);

    m_commonToolbar = addToolBar("共通");
    m_commonToolbar->setFont(uiFont);
    configureToolbar(m_commonToolbar, Qt::ToolButtonTextBesideIcon);
    m_actDesigner = m_commonToolbar->addAction("DESIGNER");
    m_actDesigner->setCheckable(true);
    m_actDesigner->setChecked(true);
    setMaterialIcon(m_actDesigner, QStringLiteral("edit"));
    m_actLayout = m_commonToolbar->addAction("LAYOUT");
    m_actLayout->setCheckable(true);
    setMaterialIcon(m_actLayout, QStringLiteral("grid_view"));
    connect(m_actDesigner, &QAction::triggered, this, [this]{ onModeChanged(true); });
    connect(m_actLayout, &QAction::triggered, this, [this]{ onModeChanged(false); });
    m_commonToolbar->addSeparator();
    m_commonToolbar->addAction(m_actTheme);
    m_commonToolbar->addAction(m_actAppSettings);
    setMaterialIcon(m_actTheme, QStringLiteral("dark_mode"));
    setMaterialIcon(m_actAppSettings, QStringLiteral("settings"));
    m_commonToolbar->addSeparator();
    m_actZoomIn = m_commonToolbar->addAction("ズームイン", this, [this]{ m_zoomScale *= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    setMaterialIcon(m_actZoomIn, QStringLiteral("zoom_in"));
    m_actZoomOut = m_commonToolbar->addAction("ズームアウト", this, [this]{ m_zoomScale /= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    setMaterialIcon(m_actZoomOut, QStringLiteral("zoom_out"));
    m_commonToolbar->addSeparator();
    m_zoomLabel = new QLabel("100%");
    m_commonToolbar->addWidget(m_zoomLabel);

    m_designerToolbar = addToolBar("Designer");
    m_designerToolbar->setFont(uiFont);
    configureToolbar(m_designerToolbar, Qt::ToolButtonTextUnderIcon);
    m_actAddBadge = m_designerToolbar->addAction("＋");
    connect(m_actAddBadge, &QAction::triggered, this, [this]{ onAddBadge(); });
    setMaterialIcon(m_actAddBadge, QStringLiteral("add_circle"));
    m_actBatchAdd = m_designerToolbar->addAction("一括");
    connect(m_actBatchAdd, &QAction::triggered, this, [this]{ onBatchAdd(); });
    setMaterialIcon(m_actBatchAdd, QStringLiteral("library_add"));
    m_actMixedLayout = m_designerToolbar->addAction("面付け");
    connect(m_actMixedLayout, &QAction::triggered, this, [this]{ onMixedLayout(); });
    setMaterialIcon(m_actMixedLayout, QStringLiteral("view_quilt"));
    m_designerToolbar->addSeparator();
    m_actSendToLayout = m_designerToolbar->addAction("レイアウトへ送る");
    connect(m_actSendToLayout, &QAction::triggered, this, [this]{ onSendToLayout(); });
    setMaterialIcon(m_actSendToLayout, QStringLiteral("send"));
    m_designerToolbar->addSeparator();
    m_actOpenDesignerPerspective = m_designerToolbar->addAction("編集");
    connect(m_actOpenDesignerPerspective, &QAction::triggered, this, [this]{ openDesignerPerspective(); });
    m_actOpenDesignerPerspective->setShortcut(QKeySequence("Ctrl+E"));
    setMaterialIcon(m_actOpenDesignerPerspective, QStringLiteral("edit"));

    m_layoutToolbar = addToolBar("Layout");
    m_layoutToolbar->setFont(uiFont);
    configureToolbar(m_layoutToolbar, Qt::ToolButtonTextUnderIcon);
    m_actOpenLayoutPerspective = m_layoutToolbar->addAction("配置確認");
    connect(m_actOpenLayoutPerspective, &QAction::triggered, this, [this]{ openLayoutPerspective(); });
    m_actOpenLayoutPerspective->setShortcut(QKeySequence("Ctrl+L"));
    setMaterialIcon(m_actOpenLayoutPerspective, QStringLiteral("grid_view"));
    m_layoutToolbar->addSeparator();
    auto* backToDesigner = m_layoutToolbar->addAction("編集に戻る", this, [this]{ openDesignerPerspective(); });
    setMaterialIcon(backToDesigner, QStringLiteral("arrow_back"));
    auto* resendLayout = m_layoutToolbar->addAction("送信し直す", this, [this]{ onSendToLayout(); });
    setMaterialIcon(resendLayout, QStringLiteral("refresh"));
    m_layoutToolbar->addSeparator();
    m_actLayoutPagePrev = m_layoutToolbar->addAction("前のページ", this, [this]{ onLayoutPagePrevious(); });
    setMaterialIcon(m_actLayoutPagePrev, QStringLiteral("chevron_left"));
    m_actLayoutPagePrev->setShortcut(QKeySequence(Qt::Key_PageUp));
    m_layoutPageLabel = new QLabel(QStringLiteral("1 / 1"));
    m_layoutPageLabel->setMinimumWidth(72);
    m_layoutPageLabel->setAlignment(Qt::AlignCenter);
    m_layoutToolbar->addWidget(m_layoutPageLabel);
    m_layoutPageCombo = new QComboBox;
    m_layoutPageCombo->setMinimumWidth(120);
    m_layoutPageCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_layoutPageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        onLayoutPageSelected(index);
    });
    m_layoutToolbar->addWidget(m_layoutPageCombo);
    m_layoutPageThumbList = new QListWidget;
    m_layoutPageThumbList->setViewMode(QListView::IconMode);
    m_layoutPageThumbList->setFlow(QListView::LeftToRight);
    m_layoutPageThumbList->setWrapping(false);
    m_layoutPageThumbList->setDragDropMode(QAbstractItemView::InternalMove);
    m_layoutPageThumbList->setDefaultDropAction(Qt::MoveAction);
    m_layoutPageThumbList->setResizeMode(QListView::Adjust);
    m_layoutPageThumbList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layoutPageThumbList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_layoutPageThumbList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layoutPageThumbList->setIconSize(QSize(72, 72));
    m_layoutPageThumbList->setFixedHeight(92);
    m_layoutPageThumbList->setMinimumWidth(220);
    m_layoutPageThumbList->setSpacing(4);
    connect(m_layoutPageThumbList, &QListWidget::currentRowChanged, this, [this](int row) {
        onLayoutPageSelected(row);
    });
    connect(m_layoutPageThumbList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || m_layoutPages.isEmpty()) {
            return;
        }
        const QPoint globalPos = m_layoutPageThumbList->mapToGlobal(pos);
        const QPoint viewportPos = m_layoutPageThumbList->viewport()->mapFromGlobal(globalPos);
        const QModelIndex index = m_layoutPageThumbList->indexAt(viewportPos);
        const int pageIndex = index.isValid() ? index.row() : m_layoutPageThumbList->currentRow();
        if (pageIndex < 0 || pageIndex >= m_layoutPages.size()) {
            return;
        }

        QMenu menu(this);
        QAction* actRename = menu.addAction(QStringLiteral("ページ名変更"));
        menu.addSeparator();
        QAction* actDuplicate = menu.addAction(QStringLiteral("このページを複製"));
        QAction* actDelete = menu.addAction(QStringLiteral("このページを削除"));
        QAction* actMoveFront = menu.addAction(QStringLiteral("先頭へ移動"));
        QAction* actMoveBack = menu.addAction(QStringLiteral("末尾へ移動"));
        actDelete->setEnabled(m_layoutPages.size() > 1);
        actMoveFront->setEnabled(pageIndex > 0);
        actMoveBack->setEnabled(pageIndex + 1 < m_layoutPages.size());
        QAction* chosen = menu.exec(globalPos);
        if (chosen == actRename) {
            renameLayoutPageAt(pageIndex);
        } else if (chosen == actDuplicate) {
            duplicateLayoutPageAt(pageIndex);
        } else if (chosen == actDelete) {
            deleteLayoutPageAt(pageIndex);
        } else if (chosen == actMoveFront) {
            moveLayoutPageToFront(pageIndex);
        } else if (chosen == actMoveBack) {
            moveLayoutPageToBack(pageIndex);
        }
    });
    connect(m_layoutPageThumbList->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex&, int, int, const QModelIndex&, int) {
                reorderLayoutPagesFromThumbList();
            });
    m_layoutPageThumbList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_layoutToolbar->addWidget(m_layoutPageThumbList);
    m_actLayoutPageNext = m_layoutToolbar->addAction("次のページ", this, [this]{ onLayoutPageNext(); });
    setMaterialIcon(m_actLayoutPageNext, QStringLiteral("chevron_right"));
    m_actLayoutPageNext->setShortcut(QKeySequence(Qt::Key_PageDown));
    m_layoutToolbar->addSeparator();
    m_actClearLayout = m_layoutToolbar->addAction("レイアウトをクリア", this, &MainWindow::onClearLayout);
    setMaterialIcon(m_actClearLayout, QStringLiteral("delete_forever"));

    // Designer
    m_designer = new DesignerWidget;
    connect(m_designer, &DesignerWidget::badgeSelected, this, [this](BadgeGraphicItem* item){ onBadgeSelected(item); });
    connect(m_designer, &DesignerWidget::selectionChanged, this, [this]{ onSelectionChanged(); });
    connect(m_designer, &DesignerWidget::badgeDeselected, this, [this]{ onBadgeDeselected(); });
    connect(m_designer, &DesignerWidget::badgeDoubleClicked, this, [this](BadgeGraphicItem*){ onSetImage(); });
    connect(m_designer, &DesignerWidget::badgeMoved, this, [this](BadgeGraphicItem* item){ onBadgeMoved(item); });
    connect(m_designer, &DesignerWidget::badgeEditStarted, this, [this](BadgeGraphicItem* item){ onBadgeEditStarted(item); });
    connect(m_designer, &DesignerWidget::badgeEditFinished, this, [this](BadgeGraphicItem* item){ onBadgeEditFinished(item); });
    connect(m_designer, &DesignerWidget::nudgeRequested, this, [this](double dxMm, double dyMm){ onNudgeRequested(dxMm, dyMm); });
    connect(m_designer, &DesignerWidget::eyedropperColorPicked, this, [this](const QColor& color) { onEyedropperColorPicked(color); });
    connect(m_designer, &DesignerWidget::eyedropperModeChanged, this, [this](bool active) {
        if (!m_btnEyedropper) {
            return;
        }
        const QSignalBlocker blocker(m_btnEyedropper);
        m_btnEyedropper->setChecked(active);
    });
    connect(m_btnEyedropper, &QPushButton::toggled, this, [this](bool on) {
        if (m_designer) {
            m_designer->setEyedropperActive(on);
        }
        if (on) {
            appendLog(QStringLiteral("スポイトを有効化しました。デザイナー上をクリックして色を拾います"));
        }
    });
    m_designer->updateGuides(32);
    updateSafetyGuideHud();

    // Layout
    m_layoutWorkspace = new LayoutWorkspaceWidget;

    // --- Inspector ---
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(280);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_inspector = new QWidget;
    m_inspector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* inspLayout = new QVBoxLayout(m_inspector);

    auto* checklistGroup = new QGroupBox("手順チェック");
    m_checklistGroup = checklistGroup;
    auto* checklistLayout = new QVBoxLayout(checklistGroup);
    m_checklistDesign = new QLabel;
    m_checklistLayout = new QLabel;
    m_checklistColor = new QLabel;
    for (auto* label : {m_checklistDesign, m_checklistLayout, m_checklistColor}) {
        if (!label) {
            continue;
        }
        label->setWordWrap(true);
        label->setMargin(7);
        label->setAutoFillBackground(true);
        checklistLayout->addWidget(label);
    }
    inspLayout->addWidget(checklistGroup);

    auto* safetyGroup = new QGroupBox("安全ガイド");
    m_safetyGuideGroup = safetyGroup;
    auto* safetyLayout = new QVBoxLayout(safetyGroup);
    m_safetyGuideDesign = new QLabel;
    m_safetyGuideLayout = new QLabel;
    m_safetyGuideColor = new QLabel;
    for (auto* label : {m_safetyGuideDesign, m_safetyGuideLayout, m_safetyGuideColor}) {
        if (!label) {
            continue;
        }
        label->setWordWrap(true);
        label->setMargin(8);
        label->setAutoFillBackground(true);
        safetyLayout->addWidget(label);
    }
    inspLayout->addWidget(safetyGroup);

    // Properties group
    auto* propGroup = new QGroupBox("オブジェクト情報");
    m_propGroup = propGroup;
    auto* propForm = new QFormLayout(propGroup);
    propForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_propX = new QDoubleSpinBox; m_propX->setSuffix(" mm"); m_propX->setRange(0, 2000);
    m_propY = new QDoubleSpinBox; m_propY->setSuffix(" mm"); m_propY->setRange(0, 2000);
    m_propW = new QDoubleSpinBox; m_propW->setSuffix(" mm"); m_propW->setRange(2, 200);
    m_propH = new QDoubleSpinBox; m_propH->setSuffix(" mm"); m_propH->setRange(2, 200);
    m_propImageScale = new QDoubleSpinBox; m_propImageScale->setSuffix(" %"); m_propImageScale->setRange(10, 500); m_propImageScale->setSingleStep(5); m_propImageScale->setValue(100);
    m_propRotation = new QSlider(Qt::Horizontal); m_propRotation->setRange(0, 360);
    m_propText = new QLineEdit;
    propForm->addRow("X:", m_propX); propForm->addRow("Y:", m_propY);
    propForm->addRow("幅:", m_propW); propForm->addRow("高さ:", m_propH);
    propForm->addRow("画像倍率:", m_propImageScale);
    // Size preset
    m_sizePreset = new QComboBox;
    m_sizePreset->addItems({
        "カスタム",
        "25mm",
        "32mm",
        "44mm",
        "57mm",
        "65mm",
        "76mm",
        "100mm"
    });
    propForm->addRow("プリセット:", m_sizePreset);
    connect(m_sizePreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        static const double sizes[] = {0, 25, 32, 44, 57, 65, 76, 100};
        if (idx > 0 && idx < static_cast<int>(std::size(sizes))) {
            m_propW->setValue(sizes[idx]);
            m_propH->setValue(sizes[idx]);
        }
    });
    m_sizePreset->setCurrentIndex(badgeSizePresetIndex(32.0, 32.0));
    propForm->addRow("回転:", m_propRotation); propForm->addRow("テキスト:", m_propText);
    m_propClipCircle = new QCheckBox("円形クリップ");
    m_propClipCircle->setChecked(true);
    propForm->addRow(m_propClipCircle);
    auto* imgBtn = new QPushButton("画像を変更...");
    connect(imgBtn, &QPushButton::clicked, this, [this]{ onSetImage(); });
    propForm->addRow(imgBtn);
    m_propColorSpace = new QLineEdit;
    m_propColorSpace->setReadOnly(true);
    propForm->addRow("色空間:", m_propColorSpace);
    inspLayout->addWidget(propGroup);

    connect(m_propX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ onInspectorChanged(); });
    connect(m_propY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ onInspectorChanged(); });
    connect(m_propW, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ onInspectorChanged(); });
    connect(m_propH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ onInspectorChanged(); });
    connect(m_propImageScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ onInspectorChanged(); });
    connect(m_propRotation, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propText, &QLineEdit::textChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propClipCircle, &QCheckBox::toggled, this, [this]{ onInspectorChanged(); });

    // Color correction group
    auto* colorGroup = new QGroupBox("色補正");
    m_colorGroup = colorGroup;
    auto* colorForm = new QFormLayout(colorGroup);
    colorForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_propBrightness = new QSlider(Qt::Horizontal); m_propBrightness->setRange(-100, 100);
    m_propContrast = new QSlider(Qt::Horizontal); m_propContrast->setRange(-100, 100);
    m_propSaturation = new QSlider(Qt::Horizontal); m_propSaturation->setRange(-100, 100);
    colorForm->addRow("明るさ:", m_propBrightness);
    colorForm->addRow("コントラスト:", m_propContrast);
    colorForm->addRow("彩度:", m_propSaturation);
    auto* resetBtn = new QPushButton("リセット");
    connect(resetBtn, &QPushButton::clicked, this, [this]{ m_propBrightness->setValue(0); m_propContrast->setValue(0); m_propSaturation->setValue(0); onInspectorChanged(); });
    colorForm->addRow(resetBtn);
    auto* pickRow = new QWidget(colorGroup);
    auto* pickLayout = new QHBoxLayout(pickRow);
    pickLayout->setContentsMargins(0, 0, 0, 0);
    pickLayout->setSpacing(8);
    m_btnEyedropper = new QPushButton(QStringLiteral("スポイト"));
    m_btnEyedropper->setCheckable(true);
    m_btnEyedropper->setToolTip(QStringLiteral("デザイナー上をクリックしてレイヤー塗り色を取得します"));
    m_btnEyedropper->setIcon(loadMaterialIcon(QStringLiteral("eyedropper")));
    m_btnEyedropper->setIconSize(QSize(18, 18));
    m_pickedColorSwatch = new QLabel;
    m_pickedColorSwatch->setFixedSize(36, 20);
    m_pickedColorSwatch->setAutoFillBackground(true);
    m_propPickedColor = new QLineEdit;
    m_propPickedColor->setReadOnly(true);
    m_propPickedColor->setPlaceholderText(QStringLiteral("未選択"));
    pickLayout->addWidget(m_btnEyedropper);
    pickLayout->addWidget(m_pickedColorSwatch);
    pickLayout->addWidget(m_propPickedColor, 1);
    colorForm->addRow(QStringLiteral("レイヤー塗り色:"), pickRow);
    updateLayerFillUi();
    inspLayout->addWidget(colorGroup);

    connect(m_propBrightness, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propContrast, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propSaturation, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });

    // Layer group
    auto* layerGroup = new QGroupBox("レイヤー");
    m_layerGroup = layerGroup;
    auto* layerLayout = new QVBoxLayout(layerGroup);
    m_layerList = new QListWidget;
    m_layerList->setMaximumHeight(120);
    m_layerList->setDragEnabled(true);
    m_layerList->setAcceptDrops(true);
    m_layerList->setDropIndicatorShown(true);
    m_layerList->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerList->setDefaultDropAction(Qt::MoveAction);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    layerLayout->addWidget(m_layerList);
    m_layerPreviewLabel = new QLabel;
    m_layerPreviewLabel->setFixedSize(128, 128);
    m_layerPreviewLabel->setAlignment(Qt::AlignCenter);
    m_layerPreviewLabel->setAutoFillBackground(true);
    layerLayout->addWidget(m_layerPreviewLabel);
    auto* layerOpacityLabel = new QLabel("不透明度");
    layerLayout->addWidget(layerOpacityLabel);
    m_sliderLayerOpacity = new QSlider(Qt::Horizontal);
    m_sliderLayerOpacity->setRange(0, 100);
    m_sliderLayerOpacity->setValue(100);
    layerLayout->addWidget(m_sliderLayerOpacity);
    m_comboLayerBlendMode = new QComboBox;
    m_comboLayerBlendMode->addItems({"Normal", "Multiply", "Screen", "Overlay", "Soft Light", "Add"});
    layerLayout->addWidget(m_comboLayerBlendMode);
    auto* layerBtnRow = new QHBoxLayout;
    auto* btnAddLayer = new QPushButton("＋");
    auto* btnDelLayer = new QPushButton("－");
    auto* btnUpLayer = new QPushButton("▲");
    auto* btnDownLayer = new QPushButton("▼");
    btnAddLayer->setFixedWidth(30); btnDelLayer->setFixedWidth(30);
    btnUpLayer->setFixedWidth(30); btnDownLayer->setFixedWidth(30);
    layerBtnRow->addWidget(btnAddLayer);
    layerBtnRow->addWidget(btnDelLayer);
    layerBtnRow->addWidget(btnUpLayer);
    layerBtnRow->addWidget(btnDownLayer);
    layerBtnRow->addStretch();
    layerLayout->addLayout(layerBtnRow);
    inspLayout->addWidget(layerGroup);

    connect(m_layerList, &QListWidget::currentRowChanged, this, [this](int) {
        updateLayerBlendModeUi();
        updateLayerOpacityUi();
        updateLayerPreviewUi();
        updateLayerFillUi();
    });
    connect(m_comboLayerBlendMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ onInspectorChanged(); });
    connect(m_sliderLayerOpacity, &QSlider::valueChanged, this, [this](int){ onInspectorChanged(); });

    connect(m_layerList->model(), &QAbstractItemModel::rowsAboutToBeMoved, this,
            [this](const QModelIndex&, int start, int end, const QModelIndex&, int destinationRow) {
                if (start != end || !m_selected.size()) {
                    return;
                }
                m_pendingLayerReorderActive = true;
                m_pendingLayerReorderBeforeBadges = currentDesignerBadges();
                m_pendingLayerReorderBeforeSelection = selectedBadgeIndices();
                Q_UNUSED(destinationRow);
            });
    connect(m_layerList->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex&, int start, int end, const QModelIndex&, int destinationRow) {
                if (!m_pendingLayerReorderActive || m_pendingLayerReorderBeforeBadges.isEmpty() || m_pendingLayerReorderBeforeSelection.isEmpty()) {
                    return;
                }
                if (start != end) {
                    m_pendingLayerReorderActive = false;
                    m_pendingLayerReorderBeforeBadges.clear();
                    m_pendingLayerReorderBeforeSelection.clear();
                    return;
                }
                auto before = m_pendingLayerReorderBeforeBadges;
                auto after = before;
                const int badgeIndex = m_pendingLayerReorderBeforeSelection.first();
                if (badgeIndex < 0 || badgeIndex >= after.size()) {
                    m_pendingLayerReorderActive = false;
                    m_pendingLayerReorderBeforeBadges.clear();
                    m_pendingLayerReorderBeforeSelection.clear();
                    return;
                }
                auto& layers = after[badgeIndex].layers;
                if (start < 0 || start >= layers.size()) {
                    m_pendingLayerReorderActive = false;
                    m_pendingLayerReorderBeforeBadges.clear();
                    m_pendingLayerReorderBeforeSelection.clear();
                    return;
                }
                LayerItem moving = layers.takeAt(start);
                int insertAt = destinationRow;
                if (insertAt > start) {
                    --insertAt;
                }
                insertAt = std::clamp(insertAt, 0, int(layers.size()));
                layers.insert(insertAt, moving);

                m_pendingLayerReorderActive = false;
                const auto selected = m_pendingLayerReorderBeforeSelection;
                m_pendingLayerReorderBeforeBadges.clear();
                m_pendingLayerReorderBeforeSelection.clear();
                pushBadgeChange(QStringLiteral("レイヤー並べ替え"), before, selected, after, selected);
                refreshLayerList();
                updateLayerPreviewUi();
            });

    connect(btnAddLayer, &QPushButton::clicked, this, [this]{
        if (m_selected.isEmpty()) return;
        const QString path = QFileDialog::getOpenFileName(this, "レイヤー画像を選択", QString(), "すべての画像 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.tif *.svg *.ico);;PNG (*.png);;JPEG (*.jpg *.jpeg)");
        if (path.isEmpty()) return;
        if (ImageProcessor::loadImage(path, nullptr).isNull()) {
            showOperationWarning(this,
                                 QStringLiteral("レイヤー追加"),
                                 QStringLiteral("画像の読み込み"),
                                 path,
                                 QStringLiteral("壊れた画像、または未対応形式の可能性があります"));
            return;
        }
        const auto before = currentDesignerBadges();
        const auto selected = selectedBadgeIndices();
        if (selected.isEmpty()) {
            return;
        }
        auto after = before;
        LayerItem layer;
        layer.imagePath = path;
        layer.name = QFileInfo(path).baseName();
        layer.blendMode = inferLayerBlendMode(path, layer.name);
        after[selected.first()].layers.append(layer);
        pushBadgeChange("レイヤー追加", before, selected, after, selected);
        appendLog(QStringLiteral("レイヤーを追加しました: %1").arg(path));
    });
    connect(btnDelLayer, &QPushButton::clicked, this, [this]{
        if (!m_layerList || m_selected.isEmpty()) return;
        int row = m_layerList->currentRow();
        if (row < 0 || m_selected.isEmpty()) return;
        const auto before = currentDesignerBadges();
        const auto selected = selectedBadgeIndices();
        if (selected.isEmpty()) {
            return;
        }
        auto after = before;
        auto& layers = after[selected.first()].layers;
        if (row < layers.size()) {
            layers.removeAt(row);
            pushBadgeChange("レイヤー削除", before, selected, after, selected);
            appendLog(QStringLiteral("レイヤー %1 を削除しました").arg(row + 1));
        }
    });
    connect(btnUpLayer, &QPushButton::clicked, this, [this]{
        if (!m_layerList || m_selected.isEmpty()) return;
        int row = m_layerList->currentRow();
        if (row <= 0 || m_selected.isEmpty()) return;
        const auto before = currentDesignerBadges();
        const auto selected = selectedBadgeIndices();
        if (selected.isEmpty()) {
            return;
        }
        auto after = before;
        auto& layers = after[selected.first()].layers;
        if (row < layers.size()) {
            layers.swapItemsAt(row, row - 1);
            pushBadgeChange("レイヤー上へ移動", before, selected, after, selected);
            appendLog(QStringLiteral("レイヤー %1 を上へ移動しました").arg(row + 1));
        }
    });
    connect(btnDownLayer, &QPushButton::clicked, this, [this]{
        if (!m_layerList || m_selected.isEmpty()) return;
        int row = m_layerList->currentRow();
        if (row < 0 || m_selected.isEmpty()) return;
        const auto before = currentDesignerBadges();
        const auto selected = selectedBadgeIndices();
        if (selected.isEmpty()) {
            return;
        }
        auto after = before;
        auto& layers = after[selected.first()].layers;
        if (row + 1 < layers.size()) {
            layers.swapItemsAt(row, row + 1);
            pushBadgeChange("レイヤー下へ移動", before, selected, after, selected);
            appendLog(QStringLiteral("レイヤー %1 を下へ移動しました").arg(row + 1));
        }
    });

    // Guide group
    auto* guideGroup = new QGroupBox("ガイド表示");
    m_guideGroup = guideGroup;
    auto* guideLayout = new QVBoxLayout(guideGroup);
    m_chkBleed = new QCheckBox("巻き込みエリア (塗り足し)"); m_chkBleed->setChecked(true);
    m_chkVisible = new QCheckBox("可視エリア (安全圏)"); m_chkVisible->setChecked(true);
    guideLayout->addWidget(m_chkBleed);
    guideLayout->addWidget(m_chkVisible);
    inspLayout->addWidget(guideGroup);
    connect(m_chkBleed, &QCheckBox::toggled, this, [this](bool){ onGuideToggle(); });
    connect(m_chkVisible, &QCheckBox::toggled, this, [this](bool){ onGuideToggle(); });

    // Effects group
    auto* effectGroup = new QGroupBox("エフェクト");
    m_effectGroup = effectGroup;
    auto* effectLayout = new QVBoxLayout(effectGroup);
    auto* lightGroup = new QGroupBox("光のプレビュー");
    auto* lightForm = new QFormLayout(lightGroup);
    lightForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_chkLighting = new QCheckBox("有効にする");
    m_sliderLightAngle = new QSlider(Qt::Horizontal); m_sliderLightAngle->setRange(0, 360); m_sliderLightAngle->setValue(315);
    m_sliderLightIntensity = new QSlider(Qt::Horizontal); m_sliderLightIntensity->setRange(0, 100); m_sliderLightIntensity->setValue(45);
    lightForm->addRow(m_chkLighting);
    lightForm->addRow("向き:", m_sliderLightAngle);
    lightForm->addRow("強さ:", m_sliderLightIntensity);
    effectLayout->addWidget(lightGroup);

    auto* glitterGroup = new QGroupBox("ラメ表示");
    auto* glitterForm = new QFormLayout(glitterGroup);
    glitterForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_chkGlitter = new QCheckBox("有効にする");
    m_comboGlitter = new QComboBox;
    m_comboGlitter->addItems({"全面輝き", "星型", "雪型"});
    m_comboGlitter->setEnabled(false);
    glitterForm->addRow(m_chkGlitter);
    glitterForm->addRow("パターン:", m_comboGlitter);
    effectLayout->addWidget(glitterGroup);
    inspLayout->addWidget(effectGroup);

    connect(m_chkLighting, &QCheckBox::toggled, this, [this](bool on){ onLightingToggle(on); });
    connect(m_chkGlitter, &QCheckBox::toggled, this, [this](bool on){
        if (m_comboGlitter) {
            m_comboGlitter->setEnabled(on);
        }
        onGlitterToggle(on);
    });
    connect(m_comboGlitter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){ onGlitterPatternChanged(idx); });
    connect(m_sliderLightAngle, &QSlider::valueChanged, this, [this](int){ onLightingSlider(); });
    connect(m_sliderLightIntensity, &QSlider::valueChanged, this, [this](int){ onLightingSlider(); });

    // Material group
    auto* materialGroup = new QGroupBox("素材");
    auto* materialForm = new QFormLayout(materialGroup);
    materialForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_comboMaterial = new QComboBox;
    m_comboMaterial->addItems({"標準", "マット", "ホログラム", "パール", "ラメ"});
    m_sliderSpecular = new QSlider(Qt::Horizontal); m_sliderSpecular->setRange(0, 100);
    m_sliderEnvReflection = new QSlider(Qt::Horizontal); m_sliderEnvReflection->setRange(0, 100);
    m_sliderGlitterStrength = new QSlider(Qt::Horizontal); m_sliderGlitterStrength->setRange(0, 100);
    materialForm->addRow("プリセット:", m_comboMaterial);
    materialForm->addRow("ツヤ:", m_sliderSpecular);
    materialForm->addRow("反射:", m_sliderEnvReflection);
    materialForm->addRow("ラメ感:", m_sliderGlitterStrength);
    inspLayout->addWidget(materialGroup);

    auto applyMaterialPreset = [this](int preset) {
        const auto defs = materialDefaults(preset);
        const QSignalBlocker b1(m_sliderSpecular);
        const QSignalBlocker b2(m_sliderEnvReflection);
        const QSignalBlocker b3(m_sliderGlitterStrength);
        m_sliderSpecular->setValue(defs.specular);
        m_sliderEnvReflection->setValue(defs.env);
        m_sliderGlitterStrength->setValue(defs.glitter);
    };
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, applyMaterialPreset](int idx) {
        applyMaterialPreset(idx);
        onInspectorChanged();
    });
    connect(m_sliderSpecular, &QSlider::valueChanged, this, [this](int){ onInspectorChanged(); });
    connect(m_sliderEnvReflection, &QSlider::valueChanged, this, [this](int){ onInspectorChanged(); });
    connect(m_sliderGlitterStrength, &QSlider::valueChanged, this, [this](int){ onInspectorChanged(); });
    applyMaterialPreset(0);

    // Layout config group
    auto* layoutGroup = new QGroupBox("用紙設定");
    m_layoutGroup = layoutGroup;
    auto* layoutForm = new QFormLayout(layoutGroup);
    layoutForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_comboPaperSize = new QComboBox;
    m_comboPaperSize->addItems({
        "A3 (297 x 420 mm)",
        "A4 (210 x 297 mm)",
        "A5 (148 x 210 mm)",
        "B4 (257 x 364 mm)",
        "B5 (182 x 257 mm)",
        "Letter (216 x 279 mm)",
        "Legal (216 x 356 mm)"
    });
    m_comboPaperSize->setCurrentIndex(1);
    m_chkLandscape = new QCheckBox("横向き");
    m_spinPaperMargin = new QDoubleSpinBox;
    m_spinPaperMargin->setSuffix(" mm");
    m_spinPaperMargin->setRange(0.0, 100.0);
    m_spinPaperMargin->setValue(5.0);
    m_spinPaperSpacing = new QDoubleSpinBox;
    m_spinPaperSpacing->setSuffix(" mm");
    m_spinPaperSpacing->setRange(0.0, 50.0);
    m_spinPaperSpacing->setValue(1.0);
    m_chkCutFriendlyLayout = new QCheckBox("切りやすい整列配置");
    layoutForm->addRow("用紙:", m_comboPaperSize);
    layoutForm->addRow(m_chkLandscape);
    layoutForm->addRow("余白:", m_spinPaperMargin);
    layoutForm->addRow("間隔:", m_spinPaperSpacing);
    layoutForm->addRow(m_chkCutFriendlyLayout);
    inspLayout->addWidget(layoutGroup);

    connect(m_comboPaperSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ if (!m_isDesigner) requestLayoutRefresh("paper size changed"); });
    connect(m_chkLandscape, &QCheckBox::toggled, this, [this]{ if (!m_isDesigner) requestLayoutRefresh("orientation changed"); });
    connect(m_spinPaperMargin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]{ if (!m_isDesigner) requestLayoutRefresh("margin changed"); });
    connect(m_spinPaperSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]{ if (!m_isDesigner) requestLayoutRefresh("spacing changed"); });
    connect(m_chkCutFriendlyLayout, &QCheckBox::toggled, this, [this]{ if (!m_isDesigner) requestLayoutRefresh("cut friendly changed"); });

    inspLayout->addStretch();
    scroll->setWidget(m_inspector);

    // --- Docking System ---
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    m_dockManager = new ads::CDockManager(this);
    connect(m_dockManager, &ads::CDockManager::perspectiveOpened, this, [this](const QString& name){ syncPerspectiveUi(name); });
    m_dockStyleManager = new DockStyleManager(m_dockManager, this);

    m_designerDock = new ads::CDockWidget("編集");
    m_designerDock->setWidget(m_designer);
    m_designerDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

    m_layoutDock = new ads::CDockWidget("配置確認");
    m_layoutDock->setWidget(m_layoutWorkspace);
    m_layoutDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

    m_inspectorDock = new ads::CDockWidget("インスペクター");
    m_inspectorDock->setWidget(scroll);
    m_inspectorDock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    m_inspectorDock->resize(300, 800);

    auto* logTabs = new QTabWidget;
    m_logList = new QListWidget;
    m_issueList = new QListWidget;
    m_linkList = new QListWidget;
    logTabs->addTab(m_logList, "操作ログ");
    logTabs->addTab(m_issueList, "エラー");
    logTabs->addTab(m_linkList, "リンク切れ");

    m_logDock = new ads::CDockWidget("診断");
    m_logDock->setWidget(logTabs);
    m_logDock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    m_logDock->resize(420, 320);

    m_dockArea = m_dockManager->setCentralWidget(m_designerDock);
    m_dockManager->addDockWidgetTabToArea(m_layoutDock, m_dockArea);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_inspectorDock, m_dockArea);
    m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_logDock, m_dockArea);
    m_defaultDockState = m_dockManager->saveState(1);
    m_dockManager->addPerspective("designer");
    m_layoutDock->setAsCurrentTab();
    m_dockManager->addPerspective("layout");
    m_designerDock->setAsCurrentTab();
    loadDockState();
    refreshPerspectiveMenu();

    loadAppSettings();
    updateInspectorMode();
    openDesignerPerspective();
    updateLayoutPageUi();
    updateTitle();
}

QList<BadgeItem> MainWindow::currentDesignerBadges() const {
    if (!m_designer) {
        return {};
    }
    return m_designer->badgeItems();
}

QList<int> MainWindow::selectedBadgeIndices() const {
    if (!m_designer) {
        return {};
    }
    return indicesForSelection(m_designer->selectedGraphics(), m_designer->graphicItems());
}

void MainWindow::applyDesignerBadges(const QList<BadgeItem>& badges, const QList<int>& selectedIndices) {
    if (!m_designer) {
        return;
    }
    m_designer->setBadgeItems(badges, selectedIndices);
    refreshDocumentFromDesigner();
    if (!m_isDesigner) {
        requestLayoutRefresh("designer badges applied");
    } else {
        requestBadgeEdited("designer badges applied");
    }
    updateSafetyGuideHud();
}

void MainWindow::pushBadgeChange(const QString& label,
                                 const QList<BadgeItem>& beforeBadges,
                                 const QList<int>& beforeSelection,
                                 const QList<BadgeItem>& afterBadges,
                                 const QList<int>& afterSelection,
                                 bool mergeable) {
    if (!m_undoStack) {
        applyDesignerBadges(afterBadges, afterSelection);
        return;
    }

    m_undoStack->push(new DocumentSnapshotCommand(
        label,
        beforeBadges,
        beforeSelection,
        afterBadges,
        afterSelection,
        mergeable,
        [this](const QList<BadgeItem>& badges, const QList<int>& selection) {
            applyDesignerBadges(badges, selection);
        }));
}

void MainWindow::onBadgeEditStarted(BadgeGraphicItem* item) {
    if (m_pendingEditActive || !item || !m_designer) {
        return;
    }

    m_pendingEditActive = true;
    m_pendingEditItem = item;
    m_pendingEditBeforeBadges = currentDesignerBadges();
    m_pendingEditBeforeSelection = selectedBadgeIndices();
}

void MainWindow::onBadgeEditFinished(BadgeGraphicItem* item) {
    if (!m_pendingEditActive || !m_designer) {
        return;
    }
    if (m_pendingEditItem && item && m_pendingEditItem != item) {
        return;
    }

    m_pendingBadgeMoveItem = nullptr;

    const auto beforeBadges = m_pendingEditBeforeBadges;
    const auto beforeSelection = m_pendingEditBeforeSelection;
    const auto afterBadges = currentDesignerBadges();
    const auto afterSelection = selectedBadgeIndices();

    m_pendingEditActive = false;
    m_pendingEditItem = nullptr;
    m_pendingEditBeforeBadges.clear();
    m_pendingEditBeforeSelection.clear();

    if (badgeListEquals(beforeBadges, afterBadges) && beforeSelection == afterSelection) {
        return;
    }

    QTimer::singleShot(0, this, [this,
                                 beforeBadges,
                                 beforeSelection,
                                 afterBadges,
                                 afterSelection]() {
        pushBadgeChange(QStringLiteral("編集"), beforeBadges, beforeSelection, afterBadges, afterSelection);
        appendLog(QStringLiteral("編集内容を履歴に追加しました"));
        requestBadgeEdited("badge edit finished");
    });
}

void MainWindow::onEyedropperColorPicked(const QColor& color) {
    if (!color.isValid()) {
        return;
    }

    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const auto selected = selectedBadgeIndices();
    if (selected.isEmpty() || row < 0) {
        if (m_btnEyedropper) {
            const QSignalBlocker blocker(m_btnEyedropper);
            m_btnEyedropper->setChecked(false);
        }
        return;
    }

    const auto before = currentDesignerBadges();
    auto after = before;
    bool changed = false;
    for (int index : selected) {
        if (index < 0 || index >= after.size()) {
            continue;
        }
        auto& badge = after[index];
        if (row >= 0 && row < badge.layers.size()) {
            badge.layers[row].fillColor = color;
            changed = true;
        }
    }
    if (!changed) {
        if (m_btnEyedropper) {
            const QSignalBlocker blocker(m_btnEyedropper);
            m_btnEyedropper->setChecked(false);
        }
        return;
    }

    pushBadgeChange(QStringLiteral("レイヤー塗り色"), before, selected, after, selected);
    updateLayerFillUi();
    updateLayerPreviewUi();

    const QString hex = color.alpha() == 255
        ? color.name().toUpper()
        : color.name(QColor::HexArgb).toUpper();
    if (QClipboard* clipboard = QApplication::clipboard()) {
        clipboard->setText(hex);
    }
    appendLog(QStringLiteral("スポイト: %1 (%2, %3, %4%5)")
                  .arg(hex)
                  .arg(color.red())
                  .arg(color.green())
                  .arg(color.blue())
                  .arg(color.alpha() == 255 ? QString() : QStringLiteral(", %1").arg(color.alpha())));
}

void MainWindow::appendLog(const QString& message) {
    if (!m_logList) {
        return;
    }
    auto* item = new QListWidgetItem(QTime::currentTime().toString("hh:mm:ss") + "  " + message);
    m_logList->addItem(item);
    while (m_logList->count() > 200) {
        delete m_logList->takeItem(0);
    }
    m_logList->scrollToBottom();
}

void MainWindow::refreshDiagnostics() {
    auto buildFingerprint = [](const QList<BadgeItem>& badges, int layoutPageCount, const QString& overflowSummary) {
        QString key;
        key.reserve(256);
        key += QString::number(badges.size());
        key += QChar('|');
        for (const auto& badge : badges) {
            key += badge.imagePath;
            key += QChar('|');
            key += QString::number(badge.layers.size());
            key += QChar('|');
            for (const auto& layer : badge.layers) {
                key += layer.imagePath;
                key += QChar('|');
            }
            key += QChar(';');
        }
        key += QString::number(layoutPageCount);
        key += QChar('|');
        key += overflowSummary;
        return key;
    };

    auto populateMissing = [](QListWidget* list, const QList<BadgeItem>& badges) -> int {
        if (!list) {
            return 0;
        }
        list->clear();
        int missingCount = 0;
        for (int i = 0; i < badges.size(); ++i) {
            const auto& badge = badges[i];
            const auto addMissing = [&](const QString& path, const QString& kind) {
                if (path.isEmpty()) {
                    return;
                }
                if (QFileInfo::exists(path)) {
                    return;
                }
                auto* item = new QListWidgetItem(QStringLiteral("%1: %2 -> %3")
                                                     .arg(kind)
                                                     .arg(QString::number(i + 1))
                                                     .arg(path));
                item->setForeground(Qt::red);
                list->addItem(item);
                ++missingCount;
            };
            addMissing(badge.imagePath, QStringLiteral("バッジ画像"));
            for (int layerIndex = 0; layerIndex < badge.layers.size(); ++layerIndex) {
                const auto& layer = badge.layers[layerIndex];
                addMissing(layer.imagePath, QStringLiteral("レイヤー %1").arg(layerIndex + 1));
            }
        }
        if (list->count() == 0) {
            list->addItem(QStringLiteral("問題は見つかりませんでした"));
        }
        return missingCount;
    };

    const auto& badges = m_badges;
    const QString diagnosticsFingerprint = buildFingerprint(badges, m_layoutPageCount, m_layoutOverflowSummary);
    const bool widgetsReady = m_issueList && m_linkList;
    if (widgetsReady && diagnosticsFingerprint == m_lastDiagnosticsFingerprint) {
        return;
    }

    if (m_issueList) {
        m_issueList->clear();
    }

    const int missingCount = populateMissing(m_linkList, badges);

    if (m_issueList) {
        if (badges.isEmpty()) {
            m_issueList->addItem(QStringLiteral("編集対象がありません。画像を追加するか、プロジェクトを開いてください"));
        } else {
            m_issueList->addItem(missingCount == 0
                                     ? QStringLiteral("リンク切れ候補: なし")
                                     : QStringLiteral("リンク切れ候補: %1 件").arg(missingCount));
        }
        if (m_layoutPageCount > 1) {
            m_issueList->addItem(QStringLiteral("レイアウトは %1 ページに分割されました").arg(QString::number(m_layoutPageCount)));
        }
        if (!m_layoutOverflowSummary.isEmpty()) {
            m_issueList->addItem(m_layoutOverflowSummary);
        }
    }

    if (widgetsReady) {
        m_lastDiagnosticsFingerprint = diagnosticsFingerprint;
    }
}

void MainWindow::requestDiagnosticsRefresh(const char* reason) {
    if (m_internalEventQueue.containsKind(badge::AppEventKind::DiagnosticsDirty)) {
        scheduleInternalEventFlush();
        return;
    }
    m_internalEventQueue.postDirty(badge::AppEventKind::DiagnosticsDirty,
                                   reason ? std::string(reason) : std::string{});
    scheduleInternalEventFlush();
}

void MainWindow::requestBadgeEdited(const char* reason) {
    if (m_internalEventQueue.containsKind(badge::AppEventKind::BadgeEdited)) {
        scheduleInternalEventFlush();
        return;
    }
    m_internalEventQueue.postBadgeEdited(reason ? std::string(reason) : std::string{});
    scheduleInternalEventFlush();
}

void MainWindow::requestLayoutRefresh(const char* reason) {
    if (m_internalEventQueue.containsKind(badge::AppEventKind::LayoutDirty)) {
        scheduleInternalEventFlush();
        return;
    }
    m_internalEventQueue.postDirty(badge::AppEventKind::LayoutDirty,
                                   reason ? std::string(reason) : std::string{});
    scheduleInternalEventFlush();
}

void MainWindow::scheduleInternalEventFlush() {
    if (m_internalEventFlushScheduled) {
        return;
    }

    m_internalEventFlushScheduled = true;
    QTimer::singleShot(16, this, [this] {
        m_internalEventFlushScheduled = false;
        flushInternalEvents();
    });
}

W_OBJECT_IMPL(MainWindow)

// --- File slots ---
void MainWindow::onNew() {
    m_currentFile.clear();
    m_badges.clear();
    m_layoutBadges.clear();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    m_designer->clearBadges();
    BadgeItem blank;
    blank.clipToCircle = true;
    m_designer->addBadge(blank);
    m_designer->updateGuides(32);
    if (!m_isDesigner) {
        requestLayoutRefresh("new document");
        flushInternalEvents();
    }
    refreshDocumentFromDesigner();
    appendLog("新規プロジェクトを作成しました");
    updateTitle();
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, "開く", QString(), "バッジエディタファイル (*.bge *.json)");
    if (path.isEmpty()) return;
    openProjectPath(path);
}

void MainWindow::openProjectPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        showOperationWarning(this, QStringLiteral("開く"), QStringLiteral("ファイルの読み込み"), path, f.errorString());
        return;
    }
    const auto loaded = badge::loadDocumentFromJson(f.readAll());
    if (!loaded.ok) {
        showOperationWarning(this,
                             QStringLiteral("開く"),
                             QStringLiteral("ファイルの読み込み"),
                             path,
                             loaded.errorMessage.isEmpty() ? QStringLiteral("JSON の形式を確認してください") : loaded.errorMessage);
        return;
    }
    onNew();
    projectsync::applyDocument(*m_designer, *m_layoutWorkspace, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, loaded.document);
    refreshDocumentFromDesigner();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutBadges = m_badges;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    m_currentFile = path;
    if (!m_isDesigner) {
        requestLayoutRefresh("project opened");
        flushInternalEvents();
    }
    appendLog(QStringLiteral("開きました: %1").arg(path));
    updateTitle();
    if (m_windowsIntegration) {
        m_windowsIntegration->rememberFile(path);
        m_windowsIntegration->showToast(QStringLiteral("プロジェクトを開きました"),
                                        QFileInfo(path).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onSave() {
    if (m_currentFile.isEmpty()) { onSaveAs(); return; }
    QFile f(m_currentFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(badge::saveDocumentToJson(projectsync::currentDocument(m_badges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile)));
        appendLog(QStringLiteral("保存しました: %1").arg(m_currentFile));
        requestDiagnosticsRefresh("project saved");
        if (m_windowsIntegration) {
            m_windowsIntegration->rememberFile(m_currentFile);
            m_windowsIntegration->showToast(QStringLiteral("保存しました"),
                                            QFileInfo(m_currentFile).fileName(),
                                            WindowsIntegration::ToastKind::Success);
        }
    } else {
        showOperationWarning(this, QStringLiteral("保存"), QStringLiteral("ファイルの保存"), m_currentFile, f.errorString());
    }
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, "保存", "badges.bge", "バッジエディタファイル (*.bge)");
    if (path.isEmpty()) return;
    m_currentFile = path;
    onSave();
    appendLog(QStringLiteral("名前を付けて保存: %1").arg(path));
    updateTitle();
}

void MainWindow::onExportPdf() {
    requestLayoutRefresh("export pdf");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const QString defaultName = m_currentFile.isEmpty()
        ? QStringLiteral("layout.pdf")
        : QFileInfo(m_currentFile).completeBaseName() + QStringLiteral("_layout.pdf");
    ExportDialog dlg(ExportDialog::Format::Pdf, defaultName, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    QString outPath = dlg.filePath();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "PDF出力", "保存先を指定してください");
        return;
    }
    if (!outPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        outPath += ".pdf";
    }
    QPdfWriter::ColorModel colorModel = QPdfWriter::ColorModel::RGB;
    switch (dlg.pdfColorModelIndex()) {
    case 1:
        colorModel = QPdfWriter::ColorModel::CMYK;
        break;
    case 2:
        colorModel = QPdfWriter::ColorModel::Grayscale;
        break;
    default:
        break;
    }
    if (!m_layoutWorkspace->exportPdf(pages, outPath, dlg.dpi(), colorModel, dlg.includeGuides())) {
        showOperationWarning(this,
                             QStringLiteral("PDF出力"),
                             QStringLiteral("PDFの書き出し"),
                             outPath,
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
        return;
    }
    if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("PDFを書き出しました"),
                                        QFileInfo(outPath).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onExportPng() {
    requestLayoutRefresh("export png");
    flushInternalEvents();
    const QString defaultName = m_currentFile.isEmpty()
        ? QStringLiteral("layout.png")
        : QFileInfo(m_currentFile).completeBaseName() + QStringLiteral("_layout.png");
    ExportDialog dlg(ExportDialog::Format::Png, defaultName, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    QString outPath = dlg.filePath();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "画像出力", "保存先を指定してください");
        return;
    }
    if (!outPath.endsWith(".png", Qt::CaseInsensitive)) {
        outPath += ".png";
    }
    if (!m_layoutWorkspace->exportPng(outPath, dlg.dpi(), dlg.whiteBackground(), dlg.includeGuides())) {
        showOperationWarning(this,
                             QStringLiteral("画像出力"),
                             QStringLiteral("PNGの書き出し"),
                             outPath,
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
        return;
    }
    if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("PNGを書き出しました"),
                                        QFileInfo(outPath).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onPrintPreview() {
    requestLayoutRefresh("print preview");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const badge::DocumentData document = projectsync::currentDocument(m_layoutBadges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    QPrinter printer(QPrinter::HighResolution);
    configurePrinterForDocument(printer, document, m_appSettings.printResolution);
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QStringLiteral("印刷プレビュー"));
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter* previewPrinter) {
        if (m_layoutWorkspace) {
            m_layoutWorkspace->print(previewPrinter, currentLayoutPages());
        }
    });
    if (preview.exec() == QDialog::Accepted) {
        m_appSettings.printResolution = std::max(72, printer.resolution());
        saveAppSettings();
    }
}

void MainWindow::onPrint() {
    requestLayoutRefresh("print");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const badge::DocumentData document = projectsync::currentDocument(m_layoutBadges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    PrintDialog dlg(document.paper.widthMm, document.paper.heightMm, m_appSettings.printResolution, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    QPrinter printer(QPrinter::HighResolution);
    const QString printerName = dlg.printerName();
    if (!printerName.isEmpty()) {
        printer.setPrinterName(printerName);
    }
    configurePrinterForDocument(printer, document, dlg.resolution());
    printer.setColorMode(dlg.grayScale() ? QPrinter::GrayScale : QPrinter::Color);
    printer.setCopyCount(std::max(1, dlg.copies()));
    m_appSettings.printResolution = std::max(72, dlg.resolution());
    saveAppSettings();
    if (!m_layoutWorkspace->print(&printer, pages, dlg.includeGuides())) {
        showOperationWarning(this,
                             QStringLiteral("印刷"),
                             QStringLiteral("印刷"),
                             QString(),
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
    } else if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("印刷を開始しました"),
                                        printer.printerName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onUndo() {
    if (m_undoStack) {
        m_undoStack->undo();
        appendLog("元に戻しました");
    }
}

void MainWindow::onRedo() {
    if (m_undoStack) {
        m_undoStack->redo();
        appendLog("やり直しました");
    }
}

void MainWindow::onDelete() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.isEmpty()) {
        return;
    }

    QList<BadgeItem> after;
    after.reserve(before.size() - selected.size());
    for (int i = 0; i < before.size(); ++i) {
        if (!selected.contains(i)) {
            after.append(before[i]);
        }
    }

    pushBadgeChange("削除", before, selected, after, {});
    appendLog(QStringLiteral("選択中の %1 個を削除しました").arg(selected.size()));
}

void MainWindow::onToggleTheme() {
    m_appSettings.darkTheme = !m_isDark;
    applyTheme(m_appSettings.darkTheme);
    saveAppSettings();
}

void MainWindow::onAppSettings() {
    AppSettingsDialog dlg(m_appSettings, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    applyAppSettings(dlg.settings());
    saveAppSettings();
}
void MainWindow::onToggleGrid(bool on) {
    if (m_designer) {
        m_designer->setGridVisible(on);
    }
    m_appSettings.gridVisible = on;
    saveAppSettings();
}

void MainWindow::onToggleSnapToGrid(bool on) {
    if (m_designer) {
        m_designer->setSnapToGrid(on);
    }
    m_appSettings.snapToGrid = on;
    saveAppSettings();
}

void MainWindow::onModeChanged(bool designer) {
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    updateInspectorMode();
    updateToolbarsForMode();
    if (designer) {
        openDesignerPerspective();
    } else {
        requestLayoutRefresh("mode changed to layout");
        flushInternalEvents();
        openLayoutPerspective();
    }
}

// --- Inspector ---
void MainWindow::onBadgeSelected(BadgeGraphicItem*) {
    onSelectionChanged();
}

void MainWindow::onSelectionChanged() {
    if (m_updatingUI || !m_designer) {
        return;
    }
    m_selected = m_designer->selectedGraphics();
    if (m_selected.isEmpty()) {
        onBadgeDeselected();
        return;
    }

    setInspectorControlsEnabled(true);
    m_updatingUI = true;
    BadgeItem& b = m_selected.first()->badge();
    m_propX->setValue(b.xMm); m_propY->setValue(b.yMm);
    m_propW->setValue(b.widthMm); m_propH->setValue(b.heightMm);
    if (m_propImageScale) {
        m_propImageScale->setValue(b.imageScale * 100.0);
    }
    if (m_sizePreset) {
        m_sizePreset->setCurrentIndex(badgeSizePresetIndex(b.widthMm, b.heightMm));
    }
    m_propRotation->setValue(int(b.rotation));
    m_propClipCircle->setChecked(b.clipToCircle);
    if (m_selected.size() > 1) {
        m_propText->setPlaceholderText(QStringLiteral("複数選択"));
        m_propText->setText(b.displayText);
    } else {
        m_propText->setPlaceholderText(QString());
        m_propText->setText(b.displayText);
    }
    m_propColorSpace->setText(m_selected.first()->colorSpaceLabel());
    m_propBrightness->setValue(int(b.brightness));
    m_propContrast->setValue(int(b.contrast));
    m_propSaturation->setValue(int(b.saturation));
    if (m_comboMaterial) {
        m_comboMaterial->setCurrentIndex(std::clamp(b.materialPreset, 0, 4));
    }
    if (m_sliderSpecular) {
        m_sliderSpecular->setValue(int(std::round(b.specularStrength * 100.0)));
    }
    if (m_sliderEnvReflection) {
        m_sliderEnvReflection->setValue(int(std::round(b.envReflectionStrength * 100.0)));
    }
    if (m_sliderGlitterStrength) {
        m_sliderGlitterStrength->setValue(int(std::round(b.glitterStrength * 100.0)));
    }
    m_lastGuideSizeMm = badgeGuideSizeMm(b);
    m_designer->updateGuides(badgeGuideSizeMm(b));
    m_updatingUI = false;
    refreshLayerList();
}

void MainWindow::onBadgeDeselected() {
    m_selected.clear();
    m_pendingBadgeMoveItem = nullptr;
    m_internalEventQueue.clear();
    m_internalEventFlushScheduled = false;
    setInspectorControlsEnabled(false);
    m_updatingUI = true;
    m_propX->setValue(0); m_propY->setValue(0);
    m_propW->setValue(32); m_propH->setValue(32);
    if (m_propImageScale) {
        m_propImageScale->setValue(100.0);
    }
    if (m_sizePreset) {
        m_sizePreset->setCurrentIndex(badgeSizePresetIndex(32.0, 32.0));
    }
    m_propRotation->setValue(0);
    m_propText->clear();
    m_propText->setPlaceholderText(QString());
    if (m_propClipCircle) {
        m_propClipCircle->setChecked(true);
    }
    m_propColorSpace->setText("未選択");
    if (m_comboMaterial) {
        m_comboMaterial->setCurrentIndex(0);
    }
    if (m_sliderSpecular) {
        m_sliderSpecular->setValue(48);
    }
    if (m_sliderEnvReflection) {
        m_sliderEnvReflection->setValue(24);
    }
    if (m_sliderGlitterStrength) {
        m_sliderGlitterStrength->setValue(6);
    }
    if (m_chkLighting) {
        m_chkLighting->setChecked(false);
    }
    if (m_chkGlitter) {
        m_chkGlitter->setChecked(false);
    }
    if (m_comboGlitter) {
        m_comboGlitter->setEnabled(false);
    }
    m_designer->updateGuides(m_lastGuideSizeMm);
    m_updatingUI = false;
    if (m_layerList) {
        m_layerList->clear();
    }
    updateLayerBlendModeUi();
    updateLayerPreviewUi();
    updateLayerFillUi();
}

void MainWindow::onBadgeMoved(BadgeGraphicItem* item) {
    if (m_selected.isEmpty() || !m_selected.contains(item)) return;
    m_pendingBadgeMoveItem = item;
    const int badgeIndex = m_designer ? m_designer->graphicItems().indexOf(item) : -1;
    const BadgeItem& badge = item->badge();
    m_internalEventQueue.postBadgeMoved(badgeIndex, badge.xMm, badge.yMm);
    scheduleInternalEventFlush();
    if (!m_isDesigner) {
        requestLayoutRefresh("badge moved in layout view");
    }
}

void MainWindow::flushInternalEvents() {
    const auto events = m_internalEventQueue.drain();
    bool sawBadgeMove = false;
    bool sawLayoutDirty = false;
    bool needsDiagnostics = false;
    for (const auto& event : events) {
        switch (event.kind) {
        case badge::AppEventKind::BadgeMoved:
            sawBadgeMove = true;
            break;
        case badge::AppEventKind::BadgeEdited:
            needsDiagnostics = true;
            break;
        case badge::AppEventKind::LayoutDirty:
            sawLayoutDirty = true;
            needsDiagnostics = true;
            break;
        case badge::AppEventKind::DiagnosticsDirty:
            needsDiagnostics = true;
            break;
        }
    }

    if (sawLayoutDirty) {
        if (sawBadgeMove && m_pendingBadgeMoveItem && !m_selected.isEmpty() && m_selected.contains(m_pendingBadgeMoveItem)) {
            const BadgeItem& b = m_pendingBadgeMoveItem->badge();
            m_updatingUI = true;
            if (m_propX) m_propX->setValue(b.xMm);
            if (m_propY) m_propY->setValue(b.yMm);
            m_updatingUI = false;
        }
        syncLayoutWorkspace(false);
    }

    if (sawBadgeMove && m_pendingBadgeMoveItem && !m_selected.isEmpty() && m_selected.contains(m_pendingBadgeMoveItem)) {
        const BadgeItem& b = m_pendingBadgeMoveItem->badge();
        m_updatingUI = true;
        if (m_propX) m_propX->setValue(b.xMm);
        if (m_propY) m_propY->setValue(b.yMm);
        m_updatingUI = false;
    }

    if (needsDiagnostics) {
        refreshDiagnostics();
    }
}

void MainWindow::onInspectorChanged() {
    if (m_updatingUI || m_selected.isEmpty()) return;

    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.isEmpty()) {
        return;
    }

    const QObject* source = sender();
    const bool geometrySource = source == m_propX
        || source == m_propY
        || source == m_propW
        || source == m_propH
        || source == m_sizePreset;
    const bool appearanceSource = source == m_propImageScale
        || source == m_propRotation
        || source == m_propText
        || source == m_propClipCircle
        || source == m_propBrightness
        || source == m_propContrast
        || source == m_propSaturation
        || source == m_comboMaterial
        || source == m_sliderSpecular
        || source == m_sliderEnvReflection
        || source == m_sliderGlitterStrength
        || source == m_comboLayerBlendMode
        || source == m_sliderLayerOpacity;
    const bool layerSource = source == m_comboLayerBlendMode || source == m_sliderLayerOpacity;

    auto after = before;
    for (int index : selected) {
        if (index < 0 || index >= after.size()) {
            continue;
        }
        const auto& prior = before[index];
        auto& b = after[index];
        if (geometrySource || !appearanceSource) {
            const bool sizeChanged = !qFuzzyCompare(prior.widthMm, m_propW->value())
                || !qFuzzyCompare(prior.heightMm, m_propH->value());
            b.xMm = m_propX->value();
            b.yMm = m_propY->value();
            b.widthMm = m_propW->value();
            b.heightMm = m_propH->value();
            if (sizeChanged) {
                const double centerX = prior.xMm + prior.widthMm * 0.5;
                const double centerY = prior.yMm + prior.heightMm * 0.5;
                b.xMm = centerX - b.widthMm * 0.5;
                b.yMm = centerY - b.heightMm * 0.5;
            }
        }
        if (appearanceSource || !geometrySource) {
            b.imageScale = m_propImageScale ? (m_propImageScale->value() / 100.0) : b.imageScale;
            b.materialPreset = m_comboMaterial ? m_comboMaterial->currentIndex() : b.materialPreset;
            b.specularStrength = m_sliderSpecular ? (m_sliderSpecular->value() / 100.0) : b.specularStrength;
            b.envReflectionStrength = m_sliderEnvReflection ? (m_sliderEnvReflection->value() / 100.0) : b.envReflectionStrength;
            b.glitterStrength = m_sliderGlitterStrength ? (m_sliderGlitterStrength->value() / 100.0) : b.glitterStrength;
            b.rotation = m_propRotation->value();
            b.displayText = m_propText->text();
            b.clipToCircle = m_propClipCircle ? m_propClipCircle->isChecked() : b.clipToCircle;
            b.brightness = m_propBrightness->value();
            b.contrast = m_propContrast->value();
            b.saturation = m_propSaturation->value();
        }
        if (layerSource && m_layerList) {
            const int row = m_layerList->currentRow();
            if (row >= 0 && row < b.layers.size()) {
                if (m_comboLayerBlendMode) {
                    b.layers[row].blendMode = layerBlendModeFromInt(m_comboLayerBlendMode->currentIndex());
                }
                if (m_sliderLayerOpacity) {
                    b.layers[row].opacity = std::clamp(m_sliderLayerOpacity->value() / 100.0, 0.0, 1.0);
                }
            }
        }
    }

    if (!selected.isEmpty() && selected.first() >= 0 && selected.first() < after.size()) {
        m_lastGuideSizeMm = badgeGuideSizeMm(after[selected.first()]);
        m_designer->updateGuides(badgeGuideSizeMm(after[selected.first()]));
    }

    pushBadgeChange("プロパティ変更", before, selected, after, selected, true);
    appendLog(QStringLiteral("オブジェクト情報を更新しました"));
}

void MainWindow::setInspectorControlsEnabled(bool on) {
    if (m_propGroup) m_propGroup->setEnabled(on);
    if (m_checklistGroup) m_checklistGroup->setEnabled(true);
    if (m_colorGroup) m_colorGroup->setEnabled(on);
    if (m_layerGroup) m_layerGroup->setEnabled(on);
    if (m_guideGroup) m_guideGroup->setEnabled(on);
    if (m_effectGroup) m_effectGroup->setEnabled(on);
}

double MainWindow::activeGuideSizeMm() const {
    if (!m_selected.isEmpty()) {
        return badgeGuideSizeMm(m_selected.first()->badge());
    }
    return m_lastGuideSizeMm;
}

QList<QList<BadgeItem>> MainWindow::currentLayoutPages() const {
    if (!m_layoutPages.isEmpty()) {
        return m_layoutPages;
    }
    return QList<QList<BadgeItem>>{m_layoutBadges};
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

void MainWindow::updateLayoutPageUi() {
    const bool packedLayout = m_layoutPreviewMode == LayoutPreviewMode::PackedMixedPages && m_layoutPageCount > 0;
    const int pageCount = std::max(1, m_layoutPageCount);
    const int pageIndex = std::clamp(m_layoutPageIndex, 0, pageCount - 1);
    if (m_layoutPageLabel) {
        m_layoutPageLabel->setText(QStringLiteral("%1 / %2")
                                       .arg(layoutPageTitle(pageIndex),
                                            QString::number(pageCount)));
    }
    if (m_actLayoutPagePrev) {
        m_actLayoutPagePrev->setEnabled(packedLayout && pageIndex > 0);
    }
    if (m_actLayoutPageNext) {
        m_actLayoutPageNext->setEnabled(packedLayout && pageIndex + 1 < pageCount);
    }
    if (m_layoutPageCombo) {
        const QSignalBlocker blocker(m_layoutPageCombo);
        m_layoutPageCombo->clear();
        if (packedLayout) {
            for (int i = 0; i < pageCount; ++i) {
                m_layoutPageCombo->addItem(QStringLiteral("%1 / %2")
                                               .arg(layoutPageTitle(i),
                                                    QString::number(pageCount)));
            }
            m_layoutPageCombo->setCurrentIndex(pageIndex);
            m_layoutPageCombo->setEnabled(true);
        } else {
            m_layoutPageCombo->addItem(layoutPageTitle(0));
            m_layoutPageCombo->setCurrentIndex(0);
            m_layoutPageCombo->setEnabled(false);
        }
    }
    if (m_layoutPageThumbList) {
        const QSignalBlocker blocker(m_layoutPageThumbList);
        m_layoutPageThumbList->clear();
        const auto pages = currentLayoutPages();
        for (int i = 0; i < pages.size(); ++i) {
            auto* item = new QListWidgetItem(layoutPageTitle(i));
            item->setToolTip(QStringLiteral("%1 / %2")
                                 .arg(layoutPageTitle(i),
                                      QString::number(std::max(1, m_layoutPageCount))));
            if (m_layoutWorkspace) {
                const QPixmap thumb = m_layoutWorkspace->renderPageThumbnail(pages[i], 96, false);
                if (!thumb.isNull()) {
                    item->setIcon(QIcon(thumb));
                }
            }
            item->setData(Qt::UserRole, i);
            QFont font = item->font();
            font.setBold(i == pageIndex);
            item->setFont(font);
            if (i == pageIndex) {
                item->setBackground(QBrush(QColor(86, 120, 255, 48)));
                item->setForeground(QBrush(QColor(40, 60, 130)));
            } else {
                item->setBackground(QBrush(Qt::transparent));
                item->setForeground(QBrush());
            }
            m_layoutPageThumbList->addItem(item);
        }
        m_layoutPageThumbList->setCurrentRow(pageIndex);
        m_layoutPageThumbList->setEnabled(packedLayout);
    }
}

void MainWindow::reorderLayoutPagesFromThumbList() {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || !m_layoutPageThumbList) {
        return;
    }
    const int pageCount = m_layoutPages.size();
    if (pageCount <= 1) {
        return;
    }

    QList<QList<BadgeItem>> reordered;
    reordered.reserve(pageCount);
    for (int row = 0; row < m_layoutPageThumbList->count(); ++row) {
        const auto* item = m_layoutPageThumbList->item(row);
        if (!item) {
            continue;
        }
        const int originalIndex = item->data(Qt::UserRole).toInt();
        if (originalIndex >= 0 && originalIndex < m_layoutPages.size()) {
            reordered.append(m_layoutPages[originalIndex]);
        }
    }
    if (reordered.size() != pageCount) {
        return;
    }

    m_layoutPages = reordered;
    if (m_layoutPageNames.size() == pageCount) {
        QList<QString> reorderedNames;
        reorderedNames.reserve(pageCount);
        for (int row = 0; row < m_layoutPageThumbList->count(); ++row) {
            const auto* item = m_layoutPageThumbList->item(row);
            if (!item) {
                continue;
            }
            const int originalIndex = item->data(Qt::UserRole).toInt();
            if (originalIndex >= 0 && originalIndex < m_layoutPageNames.size()) {
                reorderedNames.append(m_layoutPageNames[originalIndex]);
            }
        }
        if (reorderedNames.size() == pageCount) {
            m_layoutPageNames = reorderedNames;
        }
    }
    m_layoutPageIndex = std::clamp(m_layoutPageThumbList->currentRow(), 0, static_cast<int>(m_layoutPages.size()) - 1);
    requestLayoutRefresh("pages reordered");
}

void MainWindow::duplicateLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size()) {
        return;
    }
    constexpr double kDuplicateOffsetMm = 2.0;
    const QList<BadgeItem> duplicated = offsetLayoutPage(m_layoutPages[index], kDuplicateOffsetMm, kDuplicateOffsetMm);
    m_layoutPages.insert(index + 1, duplicated);
    const QString sourceName = index >= 0 && index < m_layoutPageNames.size() ? m_layoutPageNames[index].trimmed() : QString();
    m_layoutPageNames.insert(index + 1, sourceName.isEmpty()
                                            ? QStringLiteral("複製ページ")
                                            : QStringLiteral("%1（複製）").arg(sourceName));
    m_layoutPageIndex = index + 1;
    requestLayoutRefresh("page duplicated");
    appendLog(QStringLiteral("ページ %1 を複製しました（少しずらしました）").arg(index + 1));
}

void MainWindow::deleteLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size()) {
        return;
    }
    if (m_layoutPages.size() <= 1) {
        return;
    }
    m_layoutPages.removeAt(index);
    if (index >= 0 && index < m_layoutPageNames.size()) {
        m_layoutPageNames.removeAt(index);
    }
    m_layoutPageIndex = std::clamp(index, 0, static_cast<int>(m_layoutPages.size()) - 1);
    requestLayoutRefresh("page deleted");
    appendLog(QStringLiteral("ページ %1 を削除しました").arg(index + 1));
}

void MainWindow::moveLayoutPageToFront(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index <= 0 || index >= m_layoutPages.size()) {
        return;
    }
    const QList<BadgeItem> page = m_layoutPages.takeAt(index);
    m_layoutPages.prepend(page);
    if (index >= 0 && index < m_layoutPageNames.size()) {
        const QString name = m_layoutPageNames.takeAt(index);
        m_layoutPageNames.prepend(name);
    }
    m_layoutPageIndex = 0;
    requestLayoutRefresh("page moved front");
    appendLog(QStringLiteral("ページ %1 を先頭へ移動しました").arg(index + 1));
}

void MainWindow::moveLayoutPageToBack(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size() - 1) {
        return;
    }
    const QList<BadgeItem> page = m_layoutPages.takeAt(index);
    m_layoutPages.append(page);
    if (index >= 0 && index < m_layoutPageNames.size()) {
        const QString name = m_layoutPageNames.takeAt(index);
        m_layoutPageNames.append(name);
    }
    m_layoutPageIndex = m_layoutPages.size() - 1;
    requestLayoutRefresh("page moved back");
    appendLog(QStringLiteral("ページ %1 を末尾へ移動しました").arg(index + 1));
}

void MainWindow::renameLayoutPageAt(int index) {
    if (m_layoutPreviewMode != LayoutPreviewMode::PackedMixedPages || index < 0 || index >= m_layoutPages.size()) {
        return;
    }
    const QString currentName = index < m_layoutPageNames.size() ? m_layoutPageNames[index].trimmed() : QString();
    bool ok = false;
    const QString entered = QInputDialog::getText(this,
                                                  QStringLiteral("ページ名変更"),
                                                  QStringLiteral("ページ名:"),
                                                  QLineEdit::Normal,
                                                  currentName.isEmpty() ? layoutPageTitle(index) : currentName,
                                                  &ok);
    if (!ok) {
        return;
    }
    if (index >= m_layoutPageNames.size()) {
        m_layoutPageNames.resize(index + 1);
    }
    m_layoutPageNames[index] = entered.trimmed();
    requestLayoutRefresh("page renamed");
    appendLog(QStringLiteral("ページ %1 の名前を変更しました").arg(index + 1));
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

void MainWindow::onSetImage() {
    if (m_selected.isEmpty()) return;
    QString path = QFileDialog::getOpenFileName(this, "画像を選択", QString(), "すべての画像 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.tif *.svg *.ico);;PNG (*.png);;JPEG (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    if (ImageProcessor::loadImage(path, nullptr).isNull()) {
        showOperationWarning(this,
                             QStringLiteral("画像レイヤー変更"),
                             QStringLiteral("画像の読み込み"),
                             path,
                             QStringLiteral("壊れた画像、または未対応形式の可能性があります"));
        return;
    }
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    auto after = before;
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            setPrimaryImageLayer(after[index], path);
        }
    }
    pushBadgeChange("画像レイヤー変更", before, selected, after, selected);
    appendLog(QStringLiteral("画像レイヤーを更新しました: %1").arg(path));
}

void MainWindow::onNudgeRequested(double dxMm, double dyMm) {
    if (m_selected.isEmpty()) {
        return;
    }
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    auto after = before;
    for (int index : selected) {
        if (index < 0 || index >= after.size()) {
            continue;
        }
        after[index].xMm += dxMm;
        after[index].yMm += dyMm;
    }
    pushBadgeChange("移動", before, selected, after, selected, true);
    appendLog(QStringLiteral("移動: %1 mm, %2 mm").arg(dxMm).arg(dyMm));
}

void MainWindow::onAlignLeft() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double left = std::numeric_limits<double>::max();
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            left = std::min(left, after[index].xMm);
        }
    }
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].xMm = left;
        }
    }
    pushBadgeChange("左揃え", before, selected, after, selected);
    appendLog("左揃えを実行しました");
}

void MainWindow::onAlignHCenter() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double sum = 0.0;
    int count = 0;
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            sum += after[index].xMm + after[index].widthMm * 0.5;
            ++count;
        }
    }
    const double center = count > 0 ? sum / count : 0.0;
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].xMm = center - after[index].widthMm * 0.5;
        }
    }
    pushBadgeChange("中央揃え(横)", before, selected, after, selected);
    appendLog("中央揃え(横)を実行しました");
}

void MainWindow::onAlignRight() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double right = std::numeric_limits<double>::lowest();
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            right = std::max(right, after[index].xMm + after[index].widthMm);
        }
    }
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].xMm = right - after[index].widthMm;
        }
    }
    pushBadgeChange("右揃え", before, selected, after, selected);
    appendLog("右揃えを実行しました");
}

void MainWindow::onAlignTop() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double top = std::numeric_limits<double>::max();
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            top = std::min(top, after[index].yMm);
        }
    }
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].yMm = top;
        }
    }
    pushBadgeChange("上揃え", before, selected, after, selected);
    appendLog("上揃えを実行しました");
}

void MainWindow::onAlignVCenter() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double sum = 0.0;
    int count = 0;
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            sum += after[index].yMm + after[index].heightMm * 0.5;
            ++count;
        }
    }
    const double center = count > 0 ? sum / count : 0.0;
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].yMm = center - after[index].heightMm * 0.5;
        }
    }
    pushBadgeChange("中央揃え(縦)", before, selected, after, selected);
    appendLog("中央揃え(縦)を実行しました");
}

void MainWindow::onAlignBottom() {
    const auto before = currentDesignerBadges();
    const auto selected = selectedBadgeIndices();
    if (selected.size() < 2) {
        return;
    }
    auto after = before;
    double bottom = std::numeric_limits<double>::lowest();
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            bottom = std::max(bottom, after[index].yMm + after[index].heightMm);
        }
    }
    for (int index : selected) {
        if (index >= 0 && index < after.size()) {
            after[index].yMm = bottom - after[index].heightMm;
        }
    }
    pushBadgeChange("下揃え", before, selected, after, selected);
    appendLog("下揃えを実行しました");
}

// --- Effects ---
void MainWindow::onGuideToggle() {
    m_designer->setBleedVisible(m_chkBleed->isChecked());
    m_designer->setVisibleVisible(m_chkVisible->isChecked());
    if (!m_isDesigner) {
        requestLayoutRefresh("guide toggle");
    }
}

void MainWindow::onLightingToggle(bool on) { m_designer->setLightingEnabled(on); }
void MainWindow::onGlitterToggle(bool on) { m_designer->setGlitterEnabled(on); }
void MainWindow::onGlitterPatternChanged(int idx) { m_designer->setGlitterPattern(idx); }
void MainWindow::onLightingSlider() {
    m_designer->setLightAngle(m_sliderLightAngle->value());
    m_designer->setLightIntensity(m_sliderLightIntensity->value() / 100.0);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!m_backdropApplied) {
        applyWindowsBackdrop();
        m_backdropApplied = true;
    }
}

void MainWindow::refreshLayerList() {
    if (!m_layerList) {
        return;
    }
    const int previousRow = m_layerList->currentRow();
    m_layerList->clear();
    if (m_selected.isEmpty()) {
        m_lastLayerRow = -1;
        return;
    }
    const auto& layers = m_selected.first()->badge().layers;
    for (const auto& layer : layers) {
        auto* item = new QListWidgetItem(layerItemSummary(layer));
        m_layerList->addItem(item);
    }
    if (!layers.isEmpty()) {
        const int upper = static_cast<int>(layers.size()) - 1;
        const int candidate = previousRow >= 0 ? previousRow : m_lastLayerRow;
        const int restoredRow = std::clamp(candidate, 0, upper);
        m_layerList->setCurrentRow(restoredRow);
        m_lastLayerRow = restoredRow;
    } else {
        m_lastLayerRow = -1;
    }
    updateLayerBlendModeUi();
    updateLayerOpacityUi();
    updateLayerFillUi();
}

void MainWindow::updateLayerBlendModeUi() {
    if (!m_comboLayerBlendMode) {
        return;
    }
    const bool hasSelection = !m_selected.isEmpty();
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool valid = hasSelection && row >= 0 && row < m_selected.first()->badge().layers.size();
    const QSignalBlocker blocker(m_comboLayerBlendMode);
    m_comboLayerBlendMode->setEnabled(valid);
    if (!valid) {
        m_comboLayerBlendMode->setCurrentIndex(0);
        updateLayerPreviewUi();
        updateLayerFillUi();
        return;
    }
    const auto& layer = m_selected.first()->badge().layers[row];
    m_comboLayerBlendMode->setCurrentIndex(layerBlendModeToInt(layer.blendMode));
    updateLayerPreviewUi();
    updateLayerFillUi();
}

void MainWindow::updateLayerOpacityUi() {
    if (!m_sliderLayerOpacity) {
        return;
    }
    const bool hasSelection = !m_selected.isEmpty();
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool valid = hasSelection && row >= 0 && row < m_selected.first()->badge().layers.size();
    const QSignalBlocker blocker(m_sliderLayerOpacity);
    m_sliderLayerOpacity->setEnabled(valid);
    if (!valid) {
        m_sliderLayerOpacity->setValue(100);
        updateLayerPreviewUi();
        updateLayerFillUi();
        return;
    }
    const auto& layer = m_selected.first()->badge().layers[row];
    m_sliderLayerOpacity->setValue(int(std::round(std::clamp(layer.opacity, 0.0, 1.0) * 100.0)));
    updateLayerPreviewUi();
    updateLayerFillUi();
}

void MainWindow::updateLayerPreviewUi() {
    if (!m_layerPreviewLabel) {
        return;
    }
    const bool hasSelection = !m_selected.isEmpty();
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool valid = hasSelection && row >= 0 && row < m_selected.first()->badge().layers.size();
    if (!valid) {
        m_layerPreviewLabel->setPixmap({});
        m_layerPreviewLabel->setText(QStringLiteral("Composite Preview"));
        return;
    }

    const auto& badge = m_selected.first()->badge();
    const QPixmap preview = renderLayerPreviewPixmap(badge, row, m_layerPreviewLabel->palette(), 128);
    m_layerPreviewLabel->setText(QString());
    m_layerPreviewLabel->setPixmap(preview);
}

// --- Badge ---
void MainWindow::onAddBadge() {
    const auto before = currentDesignerBadges();
    BadgeItem b;
    b.clipToCircle = true;
    QPointF center = m_designer->mapToScene(m_designer->viewport()->rect().center());
    const double mmToPx = Constants::kMmToPx;
    b.xMm = center.x() / mmToPx - b.widthMm / 2;
    b.yMm = center.y() / mmToPx - b.heightMm / 2;
    auto after = before;
    after.append(b);
    pushBadgeChange("追加", before, QList<int>{}, after, QList<int>{static_cast<int>(after.size() - 1)});
    appendLog("バッジを追加しました");
}

void MainWindow::onBatchAdd() {
    BatchLayoutDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QList<BadgeItem> batchBadges;
        batchBadges.reserve(dlg.rows() * dlg.cols());
        for (int r = 0; r < dlg.rows(); ++r)
            for (int c = 0; c < dlg.cols(); ++c) {
                BadgeItem b; b.widthMm = dlg.badgeWidth(); b.heightMm = dlg.badgeHeight();
                b.xMm = 10 + c * (b.widthMm + 1);
                b.yMm = 10 + r * (b.heightMm + 1);
                b.clipToCircle = dlg.clipCircle();
                batchBadges.append(b);
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
}

void MainWindow::onMixedLayout() {
    QList<BadgeItem> sourceBadges;
    const auto current = currentDesignerBadges();
    if (!m_selected.isEmpty()) {
        const auto selected = selectedBadgeIndices();
        sourceBadges.reserve(selected.size());
        for (int index : selected) {
            if (index >= 0 && index < current.size()) {
                sourceBadges.append(current[index]);
            }
        }
    } else {
        sourceBadges = current;
    }

    if (sourceBadges.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("混在面付け"), QStringLiteral("面付けするテンプレートがありません。"));
        return;
    }

    MixedLayoutDialog dlg(sourceBadges, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QList<BadgeItem> mixed = dlg.expandedTemplates();
    if (mixed.isEmpty()) {
        return;
    }

    if (dlg.sortBySize()) {
        std::stable_sort(mixed.begin(), mixed.end(), [](const BadgeItem& a, const BadgeItem& b) {
            return badgeFootprintScore(a) > badgeFootprintScore(b);
        });
    }

    if (dlg.sortBySize()) {
        const badge::DocumentData paperDocument = projectsync::currentDocument(mixed, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
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
    appendLog(QStringLiteral("混在面付けを準備しました: %1 種類 / %2 枚")
                  .arg(sourceBadges.size())
                  .arg(mixed.size()));
    if (m_layoutPageCount > 1) {
        appendLog(QStringLiteral("レイアウトを %1 ページに分割しました").arg(QString::number(m_layoutPageCount)));
    }
}

void MainWindow::onAutoLayout() {
    onSendToLayout();
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
    pushBadgeChange("画像ドロップ", before, QList<int>{}, after, QList<int>{static_cast<int>(after.size() - 1)});
    appendLog(QStringLiteral("画像を追加しました: %1").arg(filePath));
}

void MainWindow::syncLayoutWorkspace(bool refreshDiagnostics) {
    refreshDocumentFromDesigner();
    const badge::DocumentData document = projectsync::currentDocument(m_layoutBadges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    badge::DocumentData layoutDocument = document;
    const QList<BadgeItem> sourceBadges = badge::qt::fromCoreBadges(layoutDocument.badges);
    const int sourceCount = sourceBadges.size();
    int placedCount = sourceCount;
    QString overflowSummary;
    int pageCount = 1;
    const bool cutFriendly = m_chkCutFriendlyLayout && m_chkCutFriendlyLayout->isChecked();
    if (m_layoutPreviewMode == LayoutPreviewMode::FillPageFromSelection) {
        BadgeItem templateBadge;
        bool hasTemplate = false;
        if (!m_layoutBadges.isEmpty()) {
            templateBadge = badgeForLayoutPreview(m_layoutBadges.first());
            hasTemplate = true;
        } else if (!m_selected.isEmpty()) {
            templateBadge = badgeForLayoutPreview(m_selected.first()->badge());
            hasTemplate = true;
        } else if (!m_badges.isEmpty()) {
            templateBadge = badgeForLayoutPreview(m_badges.first());
            hasTemplate = true;
        }
        if (hasTemplate) {
            const QList<BadgeItem> laidOut = cutFriendly
                ? LayoutEngine::fillPageGrid(templateBadge, layoutDocument.paper)
                : LayoutEngine::fillPage(templateBadge, layoutDocument.paper);
            layoutDocument.badges = badge::qt::toCoreBadges(laidOut);
            placedCount = laidOut.size();
            overflowSummary = layoutOverflowSummary(QList<BadgeItem>{templateBadge},
                                                    placedCount,
                                                    layoutDocument.paper,
                                                    false);
        }
    } else if (m_layoutPreviewMode == LayoutPreviewMode::AutoLayoutAll) {
        const QList<BadgeItem> laidOut = cutFriendly
            ? LayoutEngine::autoLayoutGrid(sourceBadges, layoutDocument.paper)
            : LayoutEngine::autoLayout(sourceBadges, layoutDocument.paper);
        layoutDocument.badges = badge::qt::toCoreBadges(laidOut);
        placedCount = laidOut.size();
        overflowSummary = layoutOverflowSummary(sourceBadges,
                                                placedCount,
                                                layoutDocument.paper,
                                                true);
    } else if (m_layoutPreviewMode == LayoutPreviewMode::PackedMixedPages) {
        if (!m_layoutPages.isEmpty()) {
            m_layoutPageIndex = std::clamp(m_layoutPageIndex, 0, static_cast<int>(m_layoutPages.size()) - 1);
            const QList<BadgeItem> currentPage = m_layoutPages[m_layoutPageIndex];
            layoutDocument.badges = badge::qt::toCoreBadges(currentPage);
            placedCount = 0;
            for (const auto& page : m_layoutPages) {
                placedCount += page.size();
            }
            pageCount = m_layoutPages.size();
            overflowSummary = layoutOverflowSummary(sourceBadges,
                                                    placedCount,
                                                    layoutDocument.paper,
                                                    true);
            if (pageCount > 1 && placedCount == sourceCount) {
                overflowSummary = QStringLiteral("複数ページに分割しました");
            }
        }
    }
    m_layoutSourceCount = sourceCount;
    m_layoutPlacedCount = placedCount;
    m_layoutOverflowCount = std::max(0, sourceCount - placedCount);
    m_layoutPageCount = std::max(1, pageCount);
    m_layoutOverflowSummary = overflowSummary;
    m_paperWidthMm = document.paper.widthMm;
    m_paperHeightMm = document.paper.heightMm;
    m_paperMarginMm = document.paper.marginMm;
    m_paperSpacingMm = document.paper.spacingMm;
    m_layoutWorkspace->setDocument(layoutDocument);
    updateLayoutPageUi();
    updateSafetyGuideHud();
    if (refreshDiagnostics) {
        requestDiagnosticsRefresh("layout synced");
    }
}

void MainWindow::refreshDocumentFromDesigner() {
    if (m_designer) {
        m_badges = m_designer->badgeItems();
    } else {
        m_badges.clear();
    }
}

void MainWindow::updateInspectorMode() {
    const bool designer = m_isDesigner;
    if (m_propGroup) m_propGroup->setVisible(designer);
    if (m_colorGroup) m_colorGroup->setVisible(designer);
    if (m_layerGroup) m_layerGroup->setVisible(designer);
    if (m_guideGroup) m_guideGroup->setVisible(designer);
    if (m_effectGroup) m_effectGroup->setVisible(designer);
    if (m_layoutGroup) m_layoutGroup->setVisible(!designer);
    if (!designer && m_designer) {
        m_designer->setEyedropperActive(false);
    }
}

void MainWindow::updateLayerFillUi() {
    QColor color;
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool hasTarget = !m_selected.isEmpty() && row >= 0;
    if (hasTarget) {
        const auto& layers = m_selected.first()->badge().layers;
        if (row >= 0 && row < layers.size()) {
            color = layers[row].fillColor;
        }
    }

    if (m_propPickedColor) {
        if (color.isValid()) {
            const QString hex = color.alpha() == 255
                ? color.name().toUpper()
                : color.name(QColor::HexArgb).toUpper();
            m_propPickedColor->setText(hex);
        } else {
            m_propPickedColor->clear();
            m_propPickedColor->setPlaceholderText(QStringLiteral("未設定"));
        }
    }
    if (m_pickedColorSwatch) {
        QPalette pal = m_pickedColorSwatch->palette();
        pal.setColor(QPalette::Window, color.isValid() ? color : QColor(96, 96, 96));
        pal.setColor(QPalette::WindowText, Qt::black);
        m_pickedColorSwatch->setPalette(pal);
    }
    if (m_btnEyedropper) {
        m_btnEyedropper->setEnabled(hasTarget);
        if (!hasTarget && m_btnEyedropper->isChecked()) {
            m_btnEyedropper->setChecked(false);
        }
    }
}

void MainWindow::updateToolbarsForMode() {
    const bool designer = m_isDesigner;
    if (m_designerToolbar) {
        m_designerToolbar->setVisible(designer);
    }
    if (m_layoutToolbar) {
        m_layoutToolbar->setVisible(!designer);
    }
    if (m_actAddBadge) {
        m_actAddBadge->setEnabled(designer);
    }
    if (m_actBatchAdd) {
        m_actBatchAdd->setEnabled(designer);
    }
    if (m_actMixedLayout) {
        m_actMixedLayout->setEnabled(designer);
    }
    if (m_actSendToLayout) {
        m_actSendToLayout->setEnabled(designer);
    }
    if (m_actOpenDesignerPerspective) {
        m_actOpenDesignerPerspective->setVisible(designer);
    }
    if (m_actOpenLayoutPerspective) {
        m_actOpenLayoutPerspective->setVisible(!designer);
    }
    if (m_actClearLayout) {
        m_actClearLayout->setVisible(!designer);
    }
    if (m_actLayoutPagePrev) {
        m_actLayoutPagePrev->setVisible(!designer);
    }
    if (m_actLayoutPageNext) {
        m_actLayoutPageNext->setVisible(!designer);
    }
    if (m_layoutPageLabel) {
        m_layoutPageLabel->setVisible(!designer);
    }
    if (m_layoutPageCombo) {
        m_layoutPageCombo->setVisible(!designer);
    }
    if (m_layoutPageThumbList) {
        m_layoutPageThumbList->setVisible(!designer);
    }
    updateLayoutPageUi();
}

void MainWindow::onSendToLayout() {
    refreshDocumentFromDesigner();
    m_lastTransferDesignerImage = QImage();
    m_lastTransferLayoutImage = QImage();
    m_lastTransferDebugTitle.clear();
    m_lastTransferDebugDetail.clear();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    const auto document = projectsync::currentDocument(m_badges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    const double guideSizeMm = !m_selected.isEmpty()
        ? badgeGuideSizeMm(m_selected.first()->badge())
        : (!m_badges.isEmpty() ? badgeGuideSizeMm(m_badges.first()) : 32.0);
    const QPointF guideCenterScene = m_designer && m_designer->scene()
        ? m_designer->scene()->sceneRect().center()
        : QPointF(0.0, 0.0);
    QList<BadgeItem> guideBadges;
    const auto graphicItems = m_designer ? m_designer->graphicItems() : QList<BadgeGraphicItem*>{};
    for (auto* graphicItem : graphicItems) {
        if (!graphicItem) {
            continue;
        }
        const QImage crop = renderTransferCropImage(*m_designer, *graphicItem, guideCenterScene, guideSizeMm);
        if (crop.isNull()) {
            continue;
        }
        BadgeItem transferBadge = makeLayoutTransferBadge(graphicItem->badge(), crop, guideSizeMm);
        if (m_lastTransferDesignerImage.isNull()) {
            m_lastTransferDesignerImage = crop;
            m_lastTransferLayoutImage = renderLayoutDebugImage(transferBadge, crop.width());
            m_lastTransferDebugTitle = graphicItem->badge().label.isEmpty()
                ? QStringLiteral("最初の転送バッジ")
                : QStringLiteral("最初の転送バッジ: %1").arg(graphicItem->badge().label);
            m_lastTransferDebugDetail = QStringLiteral("Designer crop: %1x%2 / Layout input: %3x%4 / guide size: %5mm")
                .arg(m_lastTransferDesignerImage.width())
                .arg(m_lastTransferDesignerImage.height())
                .arg(m_lastTransferLayoutImage.width())
                .arg(m_lastTransferLayoutImage.height())
                .arg(guideSizeMm, 0, 'f', 2);
            appendLog(QStringLiteral("転送デバッグ画像を更新: designer=%1x%2 layout=%3x%4")
                          .arg(m_lastTransferDesignerImage.width())
                          .arg(m_lastTransferDesignerImage.height())
                          .arg(m_lastTransferLayoutImage.width())
                          .arg(m_lastTransferLayoutImage.height()));
        }
        guideBadges.append(transferBadge);
    }
    if (guideBadges.isEmpty()) {
        m_layoutSourceCount = 0;
        m_layoutPlacedCount = 0;
        m_layoutOverflowCount = 0;
        m_layoutOverflowSummary.clear();
        requestDiagnosticsRefresh("transfer target empty");
        appendLog(QStringLiteral("中央ガイドの安全域内に送れるバッジがありません"));
        return;
    }
    if (guideBadges.size() <= 1) {
        m_layoutPreviewMode = LayoutPreviewMode::FillPageFromSelection;
        m_layoutBadges = QList<BadgeItem>{guideBadges.first()};
        m_layoutPageIndex = 0;
    } else {
        m_layoutPreviewMode = LayoutPreviewMode::AutoLayoutAll;
        m_layoutBadges = LayoutEngine::autoLayout(guideBadges, document.paper);
        m_layoutPageIndex = 0;
    }
    requestLayoutRefresh("send to layout");
    flushInternalEvents();
    m_skipNextLayoutSync = true;
    openLayoutPerspective();
}

void MainWindow::onShowTransferDebug() {
    appendLog(QStringLiteral("転送デバッグを開こうとしました"));
    if (!m_transferDebugDialog) {
        m_transferDebugDialog = new TransferDebugDialog(this);
        appendLog(QStringLiteral("転送デバッグダイアログを新規作成しました"));
    }
    appendLog(QStringLiteral("転送デバッグ内容: designer=%1x%2 layout=%3x%4")
                  .arg(m_lastTransferDesignerImage.width())
                  .arg(m_lastTransferDesignerImage.height())
                  .arg(m_lastTransferLayoutImage.width())
                  .arg(m_lastTransferLayoutImage.height()));
    m_transferDebugDialog->setImages(
        m_lastTransferDesignerImage,
        m_lastTransferLayoutImage,
        m_lastTransferDebugTitle.isEmpty() ? QStringLiteral("転送デバッグ") : m_lastTransferDebugTitle,
        m_lastTransferDebugDetail.isEmpty() ? QStringLiteral("まだ転送していません") : m_lastTransferDebugDetail);
    m_transferDebugDialog->show();
    m_transferDebugDialog->raise();
    m_transferDebugDialog->activateWindow();
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

void MainWindow::applyTheme(bool dark) {
    m_isDark = dark;
    QPalette pal = dark ? makeCinema4DDarkPalette() : QApplication::style()->standardPalette();
    QApplication::setPalette(pal);
    if (m_dockStyleManager) {
        m_dockStyleManager->applyTheme(QApplication::palette());
    }
    if (m_designer) {
        m_designer->applyThemePalette(pal);
    }
    if (m_layoutWorkspace) {
        m_layoutWorkspace->applyThemePalette(pal);
    }
    if (m_actTheme) {
        setMaterialIcon(m_actTheme, dark ? QStringLiteral("dark_mode") : QStringLiteral("light_mode"));
    }
    m_actTheme->setText(dark ? "☀ テーマ切替" : "☾ テーマ切替");
    applyWindowsBackdrop();
}

void MainWindow::applyWindowsBackdrop() {
#ifdef Q_OS_WIN
    if (auto* window = windowHandle()) {
        applyWinBackdrop(reinterpret_cast<HWND>(window->winId()), m_isDark);
    } else if (winId()) {
        applyWinBackdrop(reinterpret_cast<HWND>(winId()), m_isDark);
    }
#else
    Q_UNUSED(m_isDark);
#endif
}

void MainWindow::applyAppSettings(const AppSettings& settings) {
    m_appSettings = settings;

    applyTheme(settings.darkTheme);
    const bool experimentalGpuViewport = viewportbackend::resolvedGpuViewportEnabled(settings.experimentalGpuViewport);
    if (m_designer) {
        m_designer->setExperimentalGpuViewport(experimentalGpuViewport);
    }
    if (m_layoutWorkspace) {
        m_layoutWorkspace->setExperimentalGpuViewport(experimentalGpuViewport);
    }

    if (m_actGridVisible) {
        const QSignalBlocker blocker(m_actGridVisible);
        m_actGridVisible->setChecked(settings.gridVisible);
    }
    if (m_actSnapToGrid) {
        const QSignalBlocker blocker(m_actSnapToGrid);
        m_actSnapToGrid->setChecked(settings.snapToGrid);
    }
    if (m_designer) {
        m_designer->setGridVisible(settings.gridVisible);
        m_designer->setSnapToGrid(settings.snapToGrid);
        m_designer->setGridSpacingMm(settings.gridSpacingMm);
        m_designer->setLightingEnabled(settings.lightingEnabled);
        m_designer->setLightAngle(settings.lightAngle);
        m_designer->setLightIntensity(settings.lightIntensity / 100.0);
        m_designer->setGlitterEnabled(settings.glitterEnabled);
        m_designer->setGlitterPattern(settings.glitterPattern);
    }
    if (m_chkLighting) {
        const QSignalBlocker blocker(m_chkLighting);
        m_chkLighting->setChecked(settings.lightingEnabled);
    }
    if (m_sliderLightAngle) {
        const QSignalBlocker blocker(m_sliderLightAngle);
        m_sliderLightAngle->setValue(settings.lightAngle);
    }
    if (m_sliderLightIntensity) {
        const QSignalBlocker blocker(m_sliderLightIntensity);
        m_sliderLightIntensity->setValue(settings.lightIntensity);
    }
    if (m_chkGlitter) {
        const QSignalBlocker blocker(m_chkGlitter);
        m_chkGlitter->setChecked(settings.glitterEnabled);
    }
    if (m_comboGlitter) {
        const QSignalBlocker blocker(m_comboGlitter);
        m_comboGlitter->setCurrentIndex(settings.glitterPattern);
        m_comboGlitter->setEnabled(settings.glitterEnabled);
    }
}

void MainWindow::refreshBadges() {
    if (m_designer) {
        m_designer->refreshAll();
    }
    updateSafetyGuideHud();
    requestDiagnosticsRefresh("badges refreshed");
}

void MainWindow::updateTitle() {
    QString name = m_currentFile.isEmpty() ? "新規プロジェクト" : QFileInfo(m_currentFile).fileName();
    setWindowTitle(name + " - Badge Editor Pro");
}

void MainWindow::updateSafetyGuideHud() {
    if (!m_designer) {
        return;
    }

    QList<DesignerWidget::SafetyGuideEntry> entries;
    const int designCount = m_designer->graphicItems().size();
    entries.append({
        QStringLiteral("① デザイン"),
        designCount > 0 ? QStringLiteral("%1件 / 作業中").arg(designCount) : QStringLiteral("未作成"),
        statusColor(designCount > 0 ? StatusLevel::Good : StatusLevel::Neutral),
    });

    const bool layoutReady = m_layoutPreviewMode != LayoutPreviewMode::CurrentDesign && !m_layoutBadges.isEmpty();
    const bool packedLayout = m_layoutPreviewMode == LayoutPreviewMode::PackedMixedPages;
    const QString layoutDetail = m_layoutOverflowSummary.isEmpty()
        ? (layoutReady
               ? QStringLiteral("%1 %2/%3 / %4ページ")
                     .arg(packedLayout ? QStringLiteral("詰め込み済み") : QStringLiteral("確認済み"),
                          QString::number(m_layoutPlacedCount),
                          QString::number(std::max(0, m_layoutSourceCount)),
                          QString::number(std::max(1, m_layoutPageCount)))
               : QStringLiteral("未確認"))
        : QStringLiteral("%1 (%2/%3 / %4ページ)")
              .arg(m_layoutOverflowSummary,
                   QString::number(m_layoutPlacedCount),
                   QString::number(std::max(0, m_layoutSourceCount)),
                   QString::number(std::max(1, m_layoutPageCount)));
    entries.append({
        QStringLiteral("② レイアウト"),
        layoutDetail,
        statusColor(!m_layoutOverflowSummary.isEmpty() ? StatusLevel::Warning
                                                       : (layoutReady ? StatusLevel::Good : StatusLevel::Warning)),
    });

    bool hasImages = false;
    bool hasMissingLinks = false;
    bool hasLoadedColorSpace = false;
    for (auto* graphicItem : m_designer->graphicItems()) {
        if (!graphicItem) {
            continue;
        }
        const BadgeItem& badge = graphicItem->badge();
        const auto checkPath = [&](const QString& path) {
            if (path.isEmpty()) {
                return;
            }
            hasImages = true;
            if (!QFileInfo::exists(path)) {
                hasMissingLinks = true;
            }
        };
        checkPath(badge.imagePath);
        for (const auto& layer : badge.layers) {
            checkPath(layer.imagePath);
        }
        const QString colorSpace = graphicItem->colorSpaceLabel();
        if (!colorSpace.isEmpty() && colorSpace != QStringLiteral("読み込みなし")) {
            hasLoadedColorSpace = true;
        }
    }
    const StatusLevel colorLevel = hasImages
        ? (hasMissingLinks ? StatusLevel::Warning : (hasLoadedColorSpace ? StatusLevel::Good : StatusLevel::Warning))
        : StatusLevel::Neutral;
    const QString colorDetail = !hasImages
        ? QStringLiteral("画像なし")
        : (hasMissingLinks ? QStringLiteral("リンク不良") : (hasLoadedColorSpace ? QStringLiteral("読み込み済み") : QStringLiteral("色空間未確認")));
    entries.append({
        QStringLiteral("③ カラースペース"),
        colorDetail,
        statusColor(colorLevel),
    });

    auto setLabel = [](QLabel* label, const QString& title, const QString& detail, const QColor& color) {
        if (!label) {
            return;
        }
        label->setText(QStringLiteral("<b>%1</b><br>%2").arg(title, detail));
        QPalette pal = label->palette();
        pal.setColor(QPalette::Window, QColor(color.red(), color.green(), color.blue(), 36));
        pal.setColor(QPalette::WindowText, color);
        pal.setColor(QPalette::Text, color);
        label->setPalette(pal);
    };

    if (m_checklistDesign) {
        setLabel(m_checklistDesign, entries[0].title, entries[0].detail, entries[0].color);
    }
    if (m_checklistLayout) {
        setLabel(m_checklistLayout, entries[1].title, entries[1].detail, entries[1].color);
    }
    if (m_checklistColor) {
        setLabel(m_checklistColor, entries[2].title, entries[2].detail, entries[2].color);
    }

    if (m_safetyGuideDesign) {
        setLabel(m_safetyGuideDesign, entries[0].title, entries[0].detail, entries[0].color);
    }
    if (m_safetyGuideLayout) {
        setLabel(m_safetyGuideLayout, entries[1].title, entries[1].detail, entries[1].color);
    }
    if (m_safetyGuideColor) {
        setLabel(m_safetyGuideColor, entries[2].title, entries[2].detail, entries[2].color);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const auto& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
            onImageDropped(path);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveAppSettings();
    saveDockState();
    QMainWindow::closeEvent(event);
}

void MainWindow::loadDockState() {
    QSettings settings;
    m_dockManager->loadPerspectives(settings);
    const QByteArray state = settings.value("dock/state").toByteArray();
    if (!state.isEmpty()) {
        m_dockManager->restoreState(state, 1);
    }
    const QString activePerspective = settings.value("dock/activePerspective", "designer").toString();
    if (activePerspective == "layout") {
        openLayoutPerspective();
    } else {
        openDesignerPerspective();
    }
}

void MainWindow::saveDockState() {
    QSettings settings;
    settings.setValue("dock/state", m_dockManager->saveState(1));
    m_dockManager->savePerspectives(settings);
    settings.setValue("dock/activePerspective", m_isDesigner ? "designer" : "layout");
    refreshPerspectiveMenu();
}

void MainWindow::loadAppSettings() {
    QSettings settings;
    AppSettings loaded;
    loaded.darkTheme = settings.value("app/darkTheme", loaded.darkTheme).toBool();
    loaded.gridVisible = settings.value("app/gridVisible", loaded.gridVisible).toBool();
    loaded.snapToGrid = settings.value("app/snapToGrid", loaded.snapToGrid).toBool();
    loaded.gridSpacingMm = settings.value("app/gridSpacingMm", loaded.gridSpacingMm).toDouble();
    loaded.lightingEnabled = settings.value("app/lightingEnabled", loaded.lightingEnabled).toBool();
    loaded.lightAngle = settings.value("app/lightAngle", loaded.lightAngle).toInt();
    loaded.lightIntensity = settings.value("app/lightIntensity", loaded.lightIntensity).toInt();
    loaded.glitterEnabled = settings.value("app/glitterEnabled", loaded.glitterEnabled).toBool();
    loaded.glitterPattern = settings.value("app/glitterPattern", loaded.glitterPattern).toInt();
    loaded.printResolution = settings.value("app/printResolution", loaded.printResolution).toInt();
    loaded.experimentalGpuViewport = settings.value("app/experimentalGpuViewport", loaded.experimentalGpuViewport).toBool();
    applyAppSettings(loaded);
}

void MainWindow::saveAppSettings() {
    QSettings settings;
    settings.setValue("app/darkTheme", m_appSettings.darkTheme);
    settings.setValue("app/gridVisible", m_appSettings.gridVisible);
    settings.setValue("app/snapToGrid", m_appSettings.snapToGrid);
    settings.setValue("app/gridSpacingMm", m_appSettings.gridSpacingMm);
    settings.setValue("app/lightingEnabled", m_appSettings.lightingEnabled);
    settings.setValue("app/lightAngle", m_appSettings.lightAngle);
    settings.setValue("app/lightIntensity", m_appSettings.lightIntensity);
    settings.setValue("app/glitterEnabled", m_appSettings.glitterEnabled);
    settings.setValue("app/glitterPattern", m_appSettings.glitterPattern);
    settings.setValue("app/printResolution", std::max(72, m_appSettings.printResolution));
    settings.setValue("app/experimentalGpuViewport", m_appSettings.experimentalGpuViewport);
}

void MainWindow::resetDockState() {
    if (m_defaultDockState.isEmpty()) {
        return;
    }
    m_dockManager->restoreState(m_defaultDockState, 1);
    m_designerDock->setAsCurrentTab();
    m_isDesigner = true;
    m_actDesigner->setChecked(true);
    m_actLayout->setChecked(false);
    updateInspectorMode();
    requestLayoutRefresh("dock state reset");
    flushInternalEvents();
}

void MainWindow::openDesignerPerspective() {
    if (!m_dockManager || !m_designerDock) {
        return;
    }
    refreshDocumentFromDesigner();
    m_designerDock->setAsCurrentTab();
    m_dockManager->openPerspective("designer");
    syncPerspectiveUiDeferred("designer");
}

void MainWindow::openLayoutPerspective() {
    if (!m_dockManager || !m_layoutDock) {
        return;
    }
    m_layoutDock->setAsCurrentTab();
    m_dockManager->openPerspective("layout");
    syncPerspectiveUiDeferred("layout");
}

void MainWindow::syncPerspectiveUiDeferred(const QString& name) {
    QTimer::singleShot(0, this, [this, name]() {
        syncPerspectiveUi(name);
    });
}

void MainWindow::saveDesignerPerspective() {
    if (!m_dockManager) {
        return;
    }
    m_dockManager->addPerspective("designer");
    saveDockState();
}

void MainWindow::saveLayoutPerspective() {
    if (!m_dockManager) {
        return;
    }
    m_dockManager->addPerspective("layout");
    saveDockState();
}

void MainWindow::savePerspectiveAs() {
    if (!m_dockManager) {
        return;
    }
    const QString name = QInputDialog::getText(this, "Perspective を保存", "名前:");
    if (name.trimmed().isEmpty()) {
        return;
    }
    m_dockManager->addPerspective(name.trimmed());
    saveDockState();
    m_dockManager->openPerspective(name.trimmed());
    syncPerspectiveUi(name.trimmed());
}

void MainWindow::deleteSavedPerspective() {
    if (!m_dockManager) {
        return;
    }

    const QStringList names = m_dockManager->perspectiveNames();
    QStringList deletable;
    for (const auto& name : names) {
        if (name != "designer" && name != "layout") {
            deletable.append(name);
        }
    }

    if (deletable.isEmpty()) {
        QMessageBox::information(this, "Perspective", "削除できる保存済み perspective がありません");
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Perspective を削除", "削除する名前:", deletable, 0, false, &ok);
    if (!ok || name.isEmpty()) {
        return;
    }

    m_dockManager->removePerspective(name);
    saveDockState();

    if (m_dockManager->perspectiveNames().contains("designer")) {
        m_dockManager->openPerspective("designer");
        syncPerspectiveUi("designer");
    } else {
        syncPerspectiveUi(QString());
    }
}

void MainWindow::openSavedPerspective() {
    auto* act = qobject_cast<QAction*>(sender());
    if (!act || !m_dockManager) {
        return;
    }
    const QString name = act->data().toString();
    if (name.isEmpty()) {
        return;
    }
    m_dockManager->openPerspective(name);
    syncPerspectiveUiDeferred(name);
}

void MainWindow::syncPerspectiveUi(const QString& name) {
    const bool knownPerspective = (name == "designer" || name == "layout");
    const bool designer = knownPerspective ? (name != "layout")
                                           : (!m_dockArea || !m_layoutDock ||
                                              m_dockArea->currentDockWidget() != m_layoutDock);
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    if (designer) {
        m_designer->setGuidesVisible(true);
        m_skipNextLayoutSync = false;
    }
    updateInspectorMode();
    updateToolbarsForMode();
    if (!designer) {
        if (m_skipNextLayoutSync) {
            m_skipNextLayoutSync = false;
        } else {
            requestLayoutRefresh("layout perspective shown");
            flushInternalEvents();
        }
    }
}

void MainWindow::refreshPerspectiveMenu() {
    if (!m_perspectiveMenu || !m_dockManager) {
        return;
    }

    m_savedPerspectiveMenu = nullptr;
    m_perspectiveMenu->clear();
    m_perspectiveMenu->addAction("現在を編集として保存", this, &MainWindow::saveDesignerPerspective);
    m_perspectiveMenu->addAction("現在を配置確認として保存", this, &MainWindow::saveLayoutPerspective);
    m_perspectiveMenu->addAction("名前を付けて保存...", this, &MainWindow::savePerspectiveAs);
    m_perspectiveMenu->addSeparator();
    m_perspectiveMenu->addAction("編集を開く", this, &MainWindow::openDesignerPerspective);
    m_perspectiveMenu->addAction("配置確認を開く", this, &MainWindow::openLayoutPerspective);

    const QStringList names = m_dockManager->perspectiveNames();
    if (!names.isEmpty()) {
        m_savedPerspectiveMenu = m_perspectiveMenu->addMenu("保存済み");
        m_savedPerspectiveMenu->addAction("削除...", this, &MainWindow::deleteSavedPerspective);
        m_savedPerspectiveMenu->addSeparator();
        for (const auto& name : names) {
            auto* action = m_savedPerspectiveMenu->addAction(name, this, &MainWindow::openSavedPerspective);
            action->setData(name);
        }
    }
}
