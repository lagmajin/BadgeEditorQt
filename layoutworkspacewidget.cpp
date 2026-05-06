#include "layoutworkspacewidget.h"
#include "badgeitem.h"
#include "imageprocessor.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsItem>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPdfOutputIntent>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QPrinter>
#include <QImage>
#include <QColorSpace>
#include <QFileInfo>
#include <QString>
#include <QFont>
#include <QHash>
#include <QGraphicsSimpleTextItem>
#include <QUrl>
#include <algorithm>

import badge.imageio;
import badge.document;
import badge.qtbridge;

struct LayoutWorkspaceWidget::Impl {
    badge::DocumentData document;
};

namespace {
constexpr double kMmToInch = 1.0 / 25.4;
constexpr double kSceneDpi = 96.0;
constexpr double kCircleBleedMm = 3.0;
constexpr int kItemRole = 0;
const char* kSafeGuideTag = "safe-guide";
const char* kEmptyHintTag = "empty-hint";

QRectF paperRectPx(const badge::DocumentData& document, double dpi) {
    const double scale = dpi * kMmToInch;
    return QRectF(0.0,
                  0.0,
                  document.paper.widthMm * scale,
                  document.paper.heightMm * scale);
}

void renderDocumentScene(QGraphicsScene* scene, const badge::DocumentData& document, QPainter* painter, double sourceDpi, const QRectF& target) {
    const QRectF source = paperRectPx(document, sourceDpi);
    scene->render(painter, target, source, Qt::IgnoreAspectRatio);
}

QPdfOutputIntent srgbOutputIntent() {
    QPdfOutputIntent intent;
    intent.setOutputConditionIdentifier(QStringLiteral("sRGB IEC61966-2.1"));
    intent.setOutputCondition(QStringLiteral("Standard RGB output"));
    intent.setRegistryName(QUrl(QStringLiteral("http://www.color.org")));
    intent.setOutputProfile(QColorSpace(QColorSpace::SRgb));
    return intent;
}

void addEmptyHint(QGraphicsScene* scene, const QRectF& paperRect) {
    if (!scene || paperRect.isEmpty()) {
        return;
    }

    auto* item = scene->addSimpleText(QStringLiteral("レイアウト対象がありません\nDesigner から送信してください"));
    item->setData(kItemRole, QString::fromLatin1(kEmptyHintTag));
    QFont font = item->font();
    font.setPointSizeF(std::max(12.0, font.pointSizeF() + 2.0));
    font.setBold(true);
    item->setFont(font);
    item->setBrush(QColor(70, 70, 70, 220));
    const QRectF br = item->boundingRect();
    item->setPos(paperRect.center().x() - br.width() * 0.5,
                 paperRect.center().y() - br.height() * 0.5);
}

QPixmap renderBadgePixmap(const badge::BadgeData& badgeData, const QSize& targetSize) {
    if (!targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        return {};
    }

    const BadgeItem qtBadge = badge::qt::fromCoreBadge(badgeData);
    QImage canvas(targetSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(qtBadge.flattenedForLayoutTransfer ? Qt::transparent : Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF rect(QPointF(0.0, 0.0), QSizeF(targetSize));
    const QRectF contentRect = rect;
    if (qtBadge.clipToCircle && !qtBadge.flattenedForLayoutTransfer) {
        constexpr double kSafeInsetMm = 3.5;
        const double safeInsetPx = kSafeInsetMm * kSceneDpi * kMmToInch;
        const QRectF safeRect = rect.adjusted(safeInsetPx, safeInsetPx, -safeInsetPx, -safeInsetPx);
        QPainterPath clip;
        clip.addEllipse(safeRect.isValid() ? safeRect : rect);
        painter.setClipPath(clip);
    }

    QString colorSpaceLabel;
    const QString primaryPath = !qtBadge.layers.isEmpty() ? qtBadge.layers.first().imagePath : qtBadge.imagePath;
    const QImage baseLoaded = ImageProcessor::loadImage(primaryPath, &colorSpaceLabel);
    if (!baseLoaded.isNull()) {
        QPixmap basePixmap = QPixmap::fromImage(baseLoaded);
        const LayerItem* primaryLayer = qtBadge.layers.isEmpty() ? nullptr : &qtBadge.layers.first();
        if (!primaryLayer || primaryLayer->visible) {
            painter.setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
            const QRectF imageRect = qtBadge.flattenedForLayoutTransfer
                ? contentRect
                : QRectF(contentRect.center().x() - (contentRect.width() * std::max(0.1, qtBadge.imageScale)) * 0.5,
                         contentRect.center().y() - (contentRect.height() * std::max(0.1, qtBadge.imageScale)) * 0.5,
                         contentRect.width() * std::max(0.1, qtBadge.imageScale),
                         contentRect.height() * std::max(0.1, qtBadge.imageScale));
            painter.drawPixmap(imageRect, basePixmap, QRectF(basePixmap.rect()));
        }
    }

    const int startLayer = qtBadge.layers.isEmpty() ? 0 : 1;
    if (qtBadge.flattenedForLayoutTransfer) {
        painter.end();
        return QPixmap::fromImage(canvas);
    }

    for (int i = startLayer; i < qtBadge.layers.size(); ++i) {
        const auto& layer = qtBadge.layers[i];
        if (!layer.visible || layer.imagePath.isEmpty() || !QFileInfo::exists(layer.imagePath)) {
            continue;
        }
        QPixmap layerPixmap;
        layerPixmap.load(layer.imagePath);
        if (layerPixmap.isNull()) {
            continue;
        }
        painter.setOpacity(layer.opacity);
        const QRectF layerRect = contentRect.translated(layer.offsetX * kSceneDpi * kMmToInch,
                                                        layer.offsetY * kSceneDpi * kMmToInch);
        painter.drawPixmap(layerRect, layerPixmap, QRectF(layerPixmap.rect()));
    }

    painter.setOpacity(1.0);
    if (!qtBadge.displayText.isEmpty()) {
        QFont font("Arial", 10);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(contentRect.adjusted(1, 1, 1, 1), qtBadge.displayText, QTextOption(Qt::AlignCenter));
        painter.setPen(Qt::black);
        painter.drawText(contentRect, qtBadge.displayText, QTextOption(Qt::AlignCenter));
    }

    painter.end();
    return QPixmap::fromImage(canvas);
}

QString badgeRenderCacheKey(const badge::BadgeData& badgeData, const QSize& targetSize) {
    QString key;
    key.reserve(256);
    key += QStringLiteral("%1x%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13")
               .arg(targetSize.width())
               .arg(targetSize.height())
               .arg(badgeData.widthMm, 0, 'f', 3)
               .arg(badgeData.heightMm, 0, 'f', 3)
               .arg(badgeData.imageScale, 0, 'f', 3)
               .arg(badgeData.materialPreset)
               .arg(badgeData.specularStrength, 0, 'f', 3)
               .arg(badgeData.envReflectionStrength, 0, 'f', 3)
               .arg(badgeData.glitterStrength, 0, 'f', 3)
               .arg(badgeData.clipToCircle ? 1 : 0)
               .arg(badgeData.flattenedForLayoutTransfer ? 1 : 0)
               .arg(QString::fromStdString(badgeData.imagePath))
               .arg(QString::fromStdString(badgeData.displayText))
               .arg(badgeData.brightness, 0, 'f', 3)
               .arg(badgeData.contrast, 0, 'f', 3)
               .arg(badgeData.saturation, 0, 'f', 3);

    for (const auto& layer : badgeData.layers) {
        key += QStringLiteral("|L:%1:%2:%3:%4:%5:%6")
                   .arg(QString::fromStdString(layer.imagePath))
                   .arg(QString::fromStdString(layer.name))
                   .arg(layer.opacity, 0, 'f', 3)
                   .arg(layer.visible ? 1 : 0)
                   .arg(layer.offsetX, 0, 'f', 3)
                   .arg(layer.offsetY, 0, 'f', 3);
    }
    return key;
}

QList<QGraphicsItem*> sceneItemsByTag(QGraphicsScene* scene, const QString& tag) {
    QList<QGraphicsItem*> matches;
    if (!scene) {
        return matches;
    }
    for (auto* item : scene->items()) {
        if (item && item->data(kItemRole).toString() == tag) {
            matches.append(item);
        }
    }
    return matches;
}
}

class LayoutBadgeItem final : public QGraphicsItem {
public:
    LayoutBadgeItem(double x, double y, double w, double h, bool clipToCircle, const QPixmap& pixmap)
        : m_rect(0.0, 0.0, clipToCircle ? std::max(w, h) : w, clipToCircle ? std::max(w, h) : h),
          m_clipToCircle(clipToCircle),
          m_pixmap(pixmap) {
        setPos(x, y);
    }

    QRectF boundingRect() const override {
        return m_rect.adjusted(-8.0, -8.0, 8.0, 8.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->save();
        painter->setPen(QPen(Qt::black));
        painter->setBrush(m_clipToCircle ? Qt::NoBrush : QBrush(Qt::white));
        if (m_clipToCircle) {
            painter->drawEllipse(m_rect);
        } else {
            painter->drawRect(m_rect);
        }

        if (!m_pixmap.isNull()) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            if (m_clipToCircle) {
                QPainterPath clip;
                clip.addEllipse(m_rect);
                painter->setClipPath(clip);
            }
            painter->drawPixmap(m_rect, m_pixmap, QRectF(m_pixmap.rect()));
        }

        // Manual cut line around each badge.
        QPen cutPen(QColor(210, 0, 0), 1.4);
        painter->setPen(cutPen);
        painter->setBrush(Qt::NoBrush);
        if (m_clipToCircle) {
            painter->drawEllipse(m_rect.adjusted(0.5, 0.5, -0.5, -0.5));
        } else {
            painter->drawRect(m_rect.adjusted(0.5, 0.5, -0.5, -0.5));
        }

        // Crop marks are useful for rectangular badges; for round badges they just look noisy.
        if (!m_clipToCircle) {
            const qreal mark = 6.0;
            painter->drawLine(QPointF(m_rect.left(), m_rect.top() + mark), QPointF(m_rect.left(), m_rect.top()));
            painter->drawLine(QPointF(m_rect.left(), m_rect.top()), QPointF(m_rect.left() + mark, m_rect.top()));
            painter->drawLine(QPointF(m_rect.right(), m_rect.top() + mark), QPointF(m_rect.right(), m_rect.top()));
            painter->drawLine(QPointF(m_rect.right() - mark, m_rect.top()), QPointF(m_rect.right(), m_rect.top()));
            painter->drawLine(QPointF(m_rect.left(), m_rect.bottom() - mark), QPointF(m_rect.left(), m_rect.bottom()));
            painter->drawLine(QPointF(m_rect.left(), m_rect.bottom()), QPointF(m_rect.left() + mark, m_rect.bottom()));
            painter->drawLine(QPointF(m_rect.right(), m_rect.bottom() - mark), QPointF(m_rect.right(), m_rect.bottom()));
            painter->drawLine(QPointF(m_rect.right() - mark, m_rect.bottom()), QPointF(m_rect.right(), m_rect.bottom()));
        }

        painter->restore();
    }

private:
    QRectF m_rect;
    bool m_clipToCircle = false;
    QPixmap m_pixmap;
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

bool LayoutWorkspaceWidget::exportPng(const QString& filePath, int dpi, bool whiteBackground) const {
    if (!m_scene || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    for (auto* item : safeGuides) {
        item->setVisible(false);
    }
    for (auto* item : emptyHints) {
        item->setVisible(false);
    }

    const QRectF rect = paperRectPx(m_impl->document, dpi);
    QImage image(rect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        for (auto* item : safeGuides) {
            item->setVisible(true);
        }
        for (auto* item : emptyHints) {
            item->setVisible(true);
        }
        return false;
    }
    image.fill(whiteBackground ? Qt::white : Qt::transparent);
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    renderDocumentScene(m_scene, m_impl->document, &painter, kSceneDpi, QRectF(QPointF(0, 0), rect.size()));
    painter.end();
    for (auto* item : safeGuides) {
        item->setVisible(true);
    }
    for (auto* item : emptyHints) {
        item->setVisible(true);
    }

    return image.save(filePath);
}

bool LayoutWorkspaceWidget::exportPdf(const QString& filePath, int dpi, QPdfWriter::ColorModel colorModel) const {
    if (!m_scene || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    for (auto* item : safeGuides) {
        item->setVisible(false);
    }
    for (auto* item : emptyHints) {
        item->setVisible(false);
    }

    QPdfWriter writer(filePath);
    writer.setResolution(dpi);
    writer.setColorModel(colorModel);
    if (colorModel == QPdfWriter::ColorModel::RGB) {
        writer.setOutputIntent(srgbOutputIntent());
    }
    writer.setPageSize(QPageSize(QSizeF(m_impl->document.paper.widthMm, m_impl->document.paper.heightMm), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter(&writer);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRectF target(QPointF(0, 0), QSizeF(writer.width(), writer.height()));
    renderDocumentScene(m_scene, m_impl->document, &painter, kSceneDpi, target);
    painter.end();
    for (auto* item : safeGuides) {
        item->setVisible(true);
    }
    for (auto* item : emptyHints) {
        item->setVisible(true);
    }
    return true;
}

bool LayoutWorkspaceWidget::print(QPrinter* printer) const {
    if (!m_scene || !printer || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    for (auto* item : safeGuides) {
        item->setVisible(false);
    }
    for (auto* item : emptyHints) {
        item->setVisible(false);
    }

    printer->setColorMode(QPrinter::Color);
    QPainter painter(printer);
    if (!painter.isActive()) {
        for (auto* item : safeGuides) {
            item->setVisible(true);
        }
        for (auto* item : emptyHints) {
            item->setVisible(true);
        }
        return false;
    }
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRect fullRect = printer->pageLayout().fullRectPixels(printer->resolution());
    painter.fillRect(fullRect, Qt::white);
    const QRectF target(QPointF(0, 0), QSizeF(fullRect.size()));
    renderDocumentScene(m_scene, m_impl->document, &painter, kSceneDpi, target);
    painter.end();

    for (auto* item : safeGuides) {
        item->setVisible(true);
    }
    for (auto* item : emptyHints) {
        item->setVisible(true);
    }
    return true;
}

void LayoutWorkspaceWidget::updateSceneRect() {
    const double mmToPx = 96.0 / 25.4;
    m_scene->setSceneRect(-50, -50,
                          m_impl->document.paper.widthMm * mmToPx + 100,
                          m_impl->document.paper.heightMm * mmToPx + 100);
}

void LayoutWorkspaceWidget::rebuildScene() {
    const double mmToPx = 96.0 / 25.4;
    QHash<QString, QPixmap> renderCache;
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
    auto* safeGuide = m_scene->addRect(marginPx, marginPx,
                                       safeWidthPx,
                                       safeHeightPx,
                                       safePen, Qt::NoBrush);
    safeGuide->setData(kItemRole, QString::fromLatin1(kSafeGuideTag));

    if (m_impl->document.badges.empty()) {
        addEmptyHint(m_scene, QRectF(0.0,
                                     0.0,
                                     m_impl->document.paper.widthMm * mmToPx,
                                     m_impl->document.paper.heightMm * mmToPx));
    }

    for (const auto& b : m_impl->document.badges) {
        const double x = b.xMm * mmToPx;
        const double y = b.yMm * mmToPx;
        const double w = (b.clipToCircle ? std::max(b.widthMm, b.heightMm) + kCircleBleedMm : b.widthMm) * mmToPx;
        const double h = (b.clipToCircle ? std::max(b.widthMm, b.heightMm) + kCircleBleedMm : b.heightMm) * mmToPx;
        const QSize renderSize(std::max(1, int(std::round(w))), std::max(1, int(std::round(h))));
        const QString cacheKey = badgeRenderCacheKey(b, renderSize);
        const auto it = renderCache.constFind(cacheKey);
        const QPixmap pixmap = (it != renderCache.cend()) ? *it : renderBadgePixmap(b, renderSize);
        if (it == renderCache.cend()) {
            renderCache.insert(cacheKey, pixmap);
        }

        m_scene->addItem(new LayoutBadgeItem(x, y, w, h, b.clipToCircle, pixmap));
    }
}
