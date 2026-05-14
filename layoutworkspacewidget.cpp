#include "layoutworkspacewidget.h"
#include "badgeitem.h"
#include "badge.model.h"
#include "imageprocessor.h"
#include "constants.h"
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
#include <QPalette>
#include <QColorSpace>
#include <QFileInfo>
#include <QString>
#include <QFont>
#include <QHash>
#include <QGraphicsSimpleTextItem>
#include <QUrl>
#include <QDir>
#include <algorithm>
import viewportbackend;

import badge.imageio;
import badge.document;
import badge.qtbridge;

struct LayoutWorkspaceWidget::Impl {
    badge::DocumentData document;
    QString lastError;
};

namespace {
constexpr int kItemRole = 0;
const char* kSafeGuideTag = "safe-guide";
const char* kEmptyHintTag = "empty-hint";
const char* kBleedGuideTag = "bleed-guide";

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

QRectF paperRectPx(const badge::DocumentData& document, double dpi) {
    const double scale = dpi / Constants::kMmPerInch;
    return QRectF(0.0,
                  0.0,
                  document.paper.widthMm * scale,
                  document.paper.heightMm * scale);
}

void renderDocumentScene(QGraphicsScene* scene, const badge::DocumentData& document, QPainter* painter, double sourceDpi, const QRectF& target) {
    const QRectF source = paperRectPx(document, sourceDpi);
    scene->render(painter, target, source, Qt::IgnoreAspectRatio);
}

void drawPageNumberLabel(QPainter* painter, const QRectF& pageRect, int pageIndex, int pageCount) {
    if (!painter || pageCount <= 1 || pageIndex < 0 || pageIndex >= pageCount) {
        return;
    }

    const QString label = QStringLiteral("%1 / %2")
                              .arg(QString::number(pageIndex + 1),
                                   QString::number(pageCount));
    painter->save();
    QFont font = painter->font();
    font.setPointSizeF(std::max(6.5, font.pointSizeF() > 0 ? font.pointSizeF() - 2.0 : 7.0));
    font.setBold(false);
    painter->setFont(font);

    const QFontMetricsF fm(font);
    const QRectF textRect = fm.boundingRect(label).adjusted(-4.0, -2.0, 4.0, 2.0);
    const qreal marginX = std::max(8.0, pageRect.width() * 0.035);
    const qreal marginY = std::max(8.0, pageRect.height() * 0.035);
    QRectF box(QPointF(pageRect.right() - marginX - textRect.width(),
                       pageRect.bottom() - marginY - textRect.height()),
               textRect.size());
    if (box.left() < pageRect.left() + 8.0) {
        box.moveLeft(pageRect.left() + 8.0);
    }
    if (box.top() < pageRect.top() + 8.0) {
        box.moveTop(pageRect.top() + 8.0);
    }

    painter->setPen(QPen(QColor(90, 90, 90, 120), 0.8));
    painter->setBrush(QColor(255, 255, 255, 150));
    painter->drawRoundedRect(box, 3.5, 3.5);
    painter->setPen(QColor(90, 90, 90, 180));
    painter->drawText(box, Qt::AlignCenter, label);
    painter->restore();
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

QPainterPath shapePathForBadge(const QRectF& rect, badge::GuideShape shape, qreal cornerRadiusPx) {
    QPainterPath path;
    switch (shape) {
    case badge::GuideShape::Rectangle:
        path.addRect(rect);
        break;
    case badge::GuideShape::RoundedRectangle:
        path.addRoundedRect(rect, cornerRadiusPx, cornerRadiusPx);
        break;
    case badge::GuideShape::Oval:
    case badge::GuideShape::Circle:
    default:
        path.addEllipse(rect);
        break;
    }
    return path;
}

class LayoutBleedGuideItem final : public QGraphicsItem {
public:
    LayoutBleedGuideItem(double x,
                         double y,
                         double w,
                         double h,
                         const badge::GuideData& guide,
                         bool clipToCircle)
        : m_shape(guide.shape),
          m_bleedPx(std::max(0.0, guide.bleedMm) * Constants::kMmToPx),
          m_safeInsetPx(std::max(0.0, guide.safeInsetMm) * Constants::kMmToPx),
          m_cornerRadiusPx(std::max(0.0, guide.cornerRadiusMm) * Constants::kMmToPx) {
        const double size = clipToCircle ? std::max(w, h) : w;
        const double height = clipToCircle ? std::max(w, h) : h;
        m_baseRect = QRectF(0.0, 0.0, size, height);
        m_outerRect = m_baseRect.adjusted(-m_bleedPx, -m_bleedPx, m_bleedPx, m_bleedPx);
        setPos(x, y);
        setZValue(2.0);
        setAcceptedMouseButtons(Qt::NoButton);
        setFlag(ItemIsSelectable, false);
        setFlag(ItemIsFocusable, false);
        setFlag(ItemStacksBehindParent, false);
    }

    QRectF boundingRect() const override {
        return m_outerRect.adjusted(-6.0, -6.0, 6.0, 6.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->setBrush(Qt::NoBrush);

        const QColor bleedColor(255, 153, 51, 220);
        const QColor safeColor(70, 178, 255, 220);

        painter->setPen(QPen(bleedColor, 1.3, Qt::DashLine));
        painter->drawPath(shapePathForBadge(m_outerRect, m_shape, m_cornerRadiusPx));

        if (m_safeInsetPx > 0.0) {
            const QRectF innerRect = m_baseRect.adjusted(m_safeInsetPx,
                                                        m_safeInsetPx,
                                                        -m_safeInsetPx,
                                                        -m_safeInsetPx);
            if (innerRect.isValid() && innerRect.width() > 0.0 && innerRect.height() > 0.0) {
                painter->setPen(QPen(safeColor, 1.1, Qt::DashLine));
                painter->drawPath(shapePathForBadge(innerRect, m_shape, std::max(0.0, m_cornerRadiusPx - m_safeInsetPx)));
            }
        }

        painter->restore();
    }

private:
    QRectF m_baseRect;
    QRectF m_outerRect;
    badge::GuideShape m_shape = badge::GuideShape::Circle;
    qreal m_bleedPx = 0.0;
    qreal m_safeInsetPx = 0.0;
    qreal m_cornerRadiusPx = 0.0;
};

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
        const double safeInsetPx = kSafeInsetMm * Constants::kMmToPx;
        const QRectF safeRect = rect.adjusted(safeInsetPx, safeInsetPx, -safeInsetPx, -safeInsetPx);
        QPainterPath clip;
        clip.addEllipse(safeRect.isValid() ? safeRect : rect);
        painter.setClipPath(clip);
    }

    QString colorSpaceLabel;
    const QString primaryPath = !qtBadge.layers.isEmpty() ? qtBadge.layers.first().imagePath : qtBadge.imagePath;
    const QImage baseLoaded = ImageProcessor::loadImage(primaryPath, &colorSpaceLabel);
    if (!baseLoaded.isNull()) {
        const LayerItem* primaryLayer = qtBadge.layers.isEmpty() ? nullptr : &qtBadge.layers.first();
        QPixmap basePixmap = applyLayerFillColor(QPixmap::fromImage(baseLoaded), primaryLayer ? primaryLayer->fillColor : QColor());
    if (!primaryLayer || primaryLayer->visible) {
            painter.save();
            painter.setOpacity(primaryLayer ? primaryLayer->opacity : 1.0);
            painter.setCompositionMode(primaryLayer ? compositionModeForLayer(primaryLayer->blendMode) : QPainter::CompositionMode_SourceOver);
            QRectF imageRect = qtBadge.flattenedForLayoutTransfer
                ? contentRect
                : QRectF(contentRect.center().x() - (contentRect.width() * std::max(0.1, qtBadge.imageScale)) * 0.5,
                         contentRect.center().y() - (contentRect.height() * std::max(0.1, qtBadge.imageScale)) * 0.5,
                         contentRect.width() * std::max(0.1, qtBadge.imageScale),
                         contentRect.height() * std::max(0.1, qtBadge.imageScale));
            if (primaryLayer && !qtBadge.flattenedForLayoutTransfer) {
                imageRect.translate(primaryLayer->offsetX * Constants::kMmToPx,
                                    primaryLayer->offsetY * Constants::kMmToPx);
            }
            painter.drawPixmap(imageRect, basePixmap, QRectF(basePixmap.rect()));
            painter.restore();
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
        layerPixmap = applyLayerFillColor(layerPixmap, layer.fillColor);
        painter.save();
        painter.setOpacity(layer.opacity);
        painter.setCompositionMode(compositionModeForLayer(layer.blendMode));
        const QRectF layerRect = contentRect.translated(layer.offsetX * Constants::kMmToPx,
                                                        layer.offsetY * Constants::kMmToPx);
        painter.drawPixmap(layerRect, layerPixmap, QRectF(layerPixmap.rect()));
        painter.restore();
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
    key += QStringLiteral("%1x%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16|%17|%18|%19|%20|%21")
               .arg(targetSize.width())
               .arg(targetSize.height())
               .arg(static_cast<int>(badgeData.productMode))
               .arg(static_cast<int>(badgeData.guide.shape))
               .arg(badgeData.guide.bleedMm, 0, 'f', 3)
               .arg(badgeData.guide.safeInsetMm, 0, 'f', 3)
               .arg(badgeData.guide.cornerRadiusMm, 0, 'f', 3)
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
        key += QStringLiteral("|L:%1:%2:%3:%4:%5:%6:%7:%8")
                   .arg(QString::fromStdString(layer.imagePath))
                   .arg(QString::fromStdString(layer.name))
                   .arg(layer.opacity, 0, 'f', 3)
                   .arg(layer.visible ? 1 : 0)
                   .arg(layer.offsetX, 0, 'f', 3)
                   .arg(layer.offsetY, 0, 'f', 3)
                   .arg(layer.blendMode)
                   .arg(QString::fromStdString(layer.fillColor));
    }
    return key;
}

QString pageThumbnailCacheKey(const badge::DocumentData& document,
                              const QList<BadgeItem>& page,
                              int sizePx,
                              bool includeGuides) {
    QString key;
    key.reserve(512);
    key += QStringLiteral("%1x%2|%3|%4|%5|%6")
               .arg(sizePx)
               .arg(includeGuides ? 1 : 0)
               .arg(document.paper.widthMm, 0, 'f', 2)
               .arg(document.paper.heightMm, 0, 'f', 2)
               .arg(document.paper.marginMm, 0, 'f', 2)
               .arg(document.paper.spacingMm, 0, 'f', 2);
    key += QStringLiteral("|N:%1").arg(page.size());
    for (const auto& item : page) {
        const badge::BadgeData data = badge::qt::toCoreBadge(item);
        const double renderWidthMm = item.clipToCircle ? std::max(item.widthMm, item.heightMm) + Constants::kCircleBleedMm
                                                       : item.widthMm;
        const double renderHeightMm = item.clipToCircle ? std::max(item.widthMm, item.heightMm) + Constants::kCircleBleedMm
                                                        : item.heightMm;
        const QSize renderSize(std::max(1, int(std::round(renderWidthMm * Constants::kMmToPx))),
                               std::max(1, int(std::round(renderHeightMm * Constants::kMmToPx))));
        key += QStringLiteral("|B:");
        key += badgeRenderCacheKey(data, renderSize);
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

void setSceneItemsVisible(const QList<QGraphicsItem*>& items, bool visible) {
    for (auto* item : items) {
        if (item) {
            item->setVisible(visible);
        }
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
        painter->setPen(Qt::NoPen);
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

void populateSceneForDocument(QGraphicsScene* scene,
                              const badge::DocumentData& document,
                              QHash<QString, QPixmap>& renderCache) {
    const double mmToPx = Constants::kMmToPx;
    if (!scene) {
        return;
    }

    scene->clear();
    scene->addRect(0, 0,
                   document.paper.widthMm * mmToPx,
                   document.paper.heightMm * mmToPx,
                   QPen(Qt::black), QBrush(Qt::white));

    QPen safePen(Qt::red, 1, Qt::DashLine);
    safePen.setDashPattern({5, 5});
    const double marginPx = document.paper.marginMm * mmToPx;
    const double safeWidthPx = std::max(0.0, document.paper.widthMm - document.paper.marginMm * 2.0) * mmToPx;
    const double safeHeightPx = std::max(0.0, document.paper.heightMm - document.paper.marginMm * 2.0) * mmToPx;
    auto* safeGuide = scene->addRect(marginPx, marginPx,
                                      safeWidthPx,
                                      safeHeightPx,
                                      safePen, Qt::NoBrush);
    safeGuide->setData(kItemRole, QString::fromLatin1(kSafeGuideTag));

    if (document.badges.empty()) {
        addEmptyHint(scene, QRectF(0.0,
                                   0.0,
                                   document.paper.widthMm * mmToPx,
                                   document.paper.heightMm * mmToPx));
    }

    for (const auto& b : document.badges) {
        const double x = b.xMm * mmToPx;
        const double y = b.yMm * mmToPx;
        const double w = (b.clipToCircle ? std::max(b.widthMm, b.heightMm) + Constants::kCircleBleedMm : b.widthMm) * mmToPx;
        const double h = (b.clipToCircle ? std::max(b.widthMm, b.heightMm) + Constants::kCircleBleedMm : b.heightMm) * mmToPx;
        const QSize renderSize(std::max(1, int(std::round(w))), std::max(1, int(std::round(h))));
        const QString cacheKey = badgeRenderCacheKey(b, renderSize);
        const auto it = renderCache.constFind(cacheKey);
        const QPixmap pixmap = (it != renderCache.cend()) ? *it : renderBadgePixmap(b, renderSize);
        if (it == renderCache.cend()) {
            renderCache.insert(cacheKey, pixmap);
        }

        scene->addItem(new LayoutBadgeItem(x, y, w, h, b.clipToCircle, pixmap));
        auto* bleedGuide = new LayoutBleedGuideItem(x, y, w, h, b.guide, b.clipToCircle);
        bleedGuide->setData(kItemRole, QString::fromLatin1(kBleedGuideTag));
        scene->addItem(bleedGuide);
    }
}

void renderDocumentPage(const badge::DocumentData& document,
                        QPainter* painter,
                        const QRectF& target,
                        bool includeGuides,
                        QHash<QString, QPixmap>* sharedCache = nullptr) {
    if (!painter) {
        return;
    }
    QGraphicsScene scene;
    QHash<QString, QPixmap> localCache;
    auto& cache = sharedCache ? *sharedCache : localCache;
    populateSceneForDocument(&scene, document, cache);
    if (!includeGuides) {
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kSafeGuideTag)), false);
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kEmptyHintTag)), false);
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kBleedGuideTag)), false);
    }
    renderDocumentScene(&scene, document, painter, Constants::kDisplayDpi, target);
}

} // namespace

LayoutWorkspaceWidget::LayoutWorkspaceWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing);
    m_view->setBackgroundBrush(palette().brush(QPalette::Window));
    viewportbackend::applySceneViewportProfile(m_view, viewportbackend::experimentalGpuViewportEnabled());
    layout->addWidget(m_view);

    m_impl = new Impl;
    updateSceneRect();
}

LayoutWorkspaceWidget::~LayoutWorkspaceWidget() {
    delete m_impl;
}

void LayoutWorkspaceWidget::applyThemePalette(const QPalette& palette) {
    setPalette(palette);
    setAutoFillBackground(true);
    if (m_view) {
        m_view->setPalette(palette);
        m_view->setBackgroundBrush(palette.brush(QPalette::Window));
        if (auto* viewport = m_view->viewport()) {
            viewport->setPalette(palette);
            viewport->setAutoFillBackground(true);
        }
    }
    if (m_scene) {
        m_scene->setBackgroundBrush(palette.brush(QPalette::Window));
    }
}

void LayoutWorkspaceWidget::setDocument(const badge::DocumentData& document) {
    m_impl->document = document;
    m_impl->lastError.clear();
    m_thumbnailCache.clear();
    rebuildScene();
}

void LayoutWorkspaceWidget::refresh() {
    m_thumbnailCache.clear();
    rebuildScene();
}

void LayoutWorkspaceWidget::setExperimentalGpuViewport(bool on) {
    viewportbackend::applySceneViewportProfile(m_view, on);
}

QString LayoutWorkspaceWidget::lastError() const {
    return m_impl ? m_impl->lastError : QString();
}

QPixmap LayoutWorkspaceWidget::renderPageThumbnail(const QList<BadgeItem>& page, int sizePx, bool includeGuides) const {
    if (sizePx <= 0 || !m_impl) {
        return {};
    }

    badge::DocumentData doc = m_impl->document;
    doc.badges = badge::qt::toCoreBadges(page);

    const QString cacheKey = pageThumbnailCacheKey(doc, page, sizePx, includeGuides);
    if (const auto it = m_thumbnailCache.constFind(cacheKey); it != m_thumbnailCache.cend()) {
        return *it;
    }

    QGraphicsScene scene;
    QHash<QString, QPixmap> renderCache;
    populateSceneForDocument(&scene, doc, renderCache);
    if (!includeGuides) {
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kSafeGuideTag)), false);
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kEmptyHintTag)), false);
        setSceneItemsVisible(sceneItemsByTag(&scene, QString::fromLatin1(kBleedGuideTag)), false);
    }

    QImage image(QSize(sizePx, sizePx), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

    const QRectF source = paperRectPx(doc, Constants::kDisplayDpi);
    const QSizeF sourceSize = source.size();
    const qreal scale = std::min(qreal(sizePx) / std::max(1.0, sourceSize.width()),
                                 qreal(sizePx) / std::max(1.0, sourceSize.height()));
    const QSizeF targetSize = sourceSize * scale;
    const QRectF target((qreal(sizePx) - targetSize.width()) * 0.5,
                        (qreal(sizePx) - targetSize.height()) * 0.5,
                        targetSize.width(),
                        targetSize.height());
    scene.render(&painter, target, source, Qt::KeepAspectRatio);
    painter.end();
    const QPixmap thumb = QPixmap::fromImage(image);
    m_thumbnailCache.insert(cacheKey, thumb);
    return thumb;
}

bool LayoutWorkspaceWidget::exportPng(const QString& filePath, int dpi, bool whiteBackground) const {
    if (!m_scene || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        m_impl->lastError = QStringLiteral("PNG 出力に必要な用紙サイズが未設定です");
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    const auto bleedGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kBleedGuideTag));
    setSceneItemsVisible(safeGuides, false);
    setSceneItemsVisible(emptyHints, false);
    setSceneItemsVisible(bleedGuides, false);

    const QRectF rect = paperRectPx(m_impl->document, dpi);
    QImage image(rect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        m_impl->lastError = QStringLiteral("PNG 出力用の画像バッファを作成できませんでした");
        setSceneItemsVisible(safeGuides, true);
        setSceneItemsVisible(emptyHints, true);
        setSceneItemsVisible(bleedGuides, true);
        return false;
    }
    image.fill(whiteBackground ? Qt::white : Qt::transparent);
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    renderDocumentScene(m_scene, m_impl->document, &painter, Constants::kDisplayDpi, QRectF(QPointF(0, 0), rect.size()));
    painter.end();
    setSceneItemsVisible(safeGuides, true);
    setSceneItemsVisible(emptyHints, true);
    setSceneItemsVisible(bleedGuides, true);
    const bool ok = image.save(filePath);
    if (ok) {
        m_impl->lastError.clear();
    } else {
        m_impl->lastError = QStringLiteral("PNG の保存に失敗しました: %1").arg(QDir::toNativeSeparators(filePath));
    }
    return ok;
}

bool LayoutWorkspaceWidget::exportPdf(const QString& filePath, int dpi, QPdfWriter::ColorModel colorModel) const {
    if (!m_scene || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        m_impl->lastError = QStringLiteral("PDF 出力に必要な用紙サイズが未設定です");
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    const auto bleedGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kBleedGuideTag));
    setSceneItemsVisible(safeGuides, false);
    setSceneItemsVisible(emptyHints, false);
    setSceneItemsVisible(bleedGuides, false);

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
    renderDocumentScene(m_scene, m_impl->document, &painter, Constants::kDisplayDpi, target);
    painter.end();
    for (auto* item : safeGuides) {
        item->setVisible(true);
    }
    for (auto* item : emptyHints) {
        item->setVisible(true);
    }
    for (auto* item : bleedGuides) {
        item->setVisible(true);
    }
    if (QFileInfo::exists(filePath)) {
        m_impl->lastError.clear();
        return true;
    }
    m_impl->lastError = QStringLiteral("PDF を書き出せませんでした: %1").arg(QDir::toNativeSeparators(filePath));
    return false;
}

bool LayoutWorkspaceWidget::exportPdf(const QList<QList<BadgeItem>>& pages,
                                      const QString& filePath,
                                      int dpi,
                                      QPdfWriter::ColorModel colorModel) const {
    if (pages.isEmpty()) {
        m_impl->lastError = QStringLiteral("PDF 出力するページがありません");
        return false;
    }

    QPdfWriter writer(filePath);
    writer.setResolution(dpi);
    writer.setColorModel(colorModel);
    if (colorModel == QPdfWriter::ColorModel::RGB) {
        writer.setOutputIntent(srgbOutputIntent());
    }
    const auto& firstPaper = m_impl->document.paper;
    writer.setPageSize(QPageSize(QSizeF(firstPaper.widthMm, firstPaper.heightMm), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter(&writer);
    if (!painter.isActive()) {
        m_impl->lastError = QStringLiteral("PDF 出力の描画を開始できませんでした");
        return false;
    }
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

    QHash<QString, QPixmap> renderCache;
    for (int i = 0; i < pages.size(); ++i) {
        badge::DocumentData doc = m_impl->document;
        doc.badges = badge::qt::toCoreBadges(pages[i]);
        const QRectF target(QPointF(0, 0), QSizeF(writer.width(), writer.height()));
        QGraphicsScene scene;
        populateSceneForDocument(&scene, doc, renderCache);
        const auto safeGuides = sceneItemsByTag(&scene, QString::fromLatin1(kSafeGuideTag));
        const auto emptyHints = sceneItemsByTag(&scene, QString::fromLatin1(kEmptyHintTag));
        const auto bleedGuides = sceneItemsByTag(&scene, QString::fromLatin1(kBleedGuideTag));
        setSceneItemsVisible(safeGuides, false);
        setSceneItemsVisible(emptyHints, false);
        setSceneItemsVisible(bleedGuides, false);
        renderDocumentScene(&scene, doc, &painter, Constants::kDisplayDpi, target);
        drawPageNumberLabel(&painter, target, i, pages.size());
        if (i + 1 < pages.size()) {
            writer.newPage();
        }
    }
    painter.end();

    if (QFileInfo::exists(filePath)) {
        m_impl->lastError.clear();
        return true;
    }
    m_impl->lastError = QStringLiteral("PDF を書き出せませんでした: %1").arg(QDir::toNativeSeparators(filePath));
    return false;
}

bool LayoutWorkspaceWidget::print(QPrinter* printer, bool includeGuides) const {
    if (!m_scene || !printer || m_impl->document.paper.widthMm <= 0.0 || m_impl->document.paper.heightMm <= 0.0) {
        m_impl->lastError = QStringLiteral("印刷に必要な用紙サイズが未設定です");
        return false;
    }

    const auto safeGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kSafeGuideTag));
    const auto emptyHints = sceneItemsByTag(m_scene, QString::fromLatin1(kEmptyHintTag));
    const auto bleedGuides = sceneItemsByTag(m_scene, QString::fromLatin1(kBleedGuideTag));
    if (!includeGuides) {
        for (auto* item : safeGuides) {
            item->setVisible(false);
        }
        for (auto* item : emptyHints) {
            item->setVisible(false);
        }
        for (auto* item : bleedGuides) {
            item->setVisible(false);
        }
    }

    QPainter painter(printer);
    if (!painter.isActive()) {
        m_impl->lastError = QStringLiteral("プリンタへの描画を開始できませんでした");
        if (!includeGuides) {
            setSceneItemsVisible(safeGuides, true);
            setSceneItemsVisible(emptyHints, true);
            setSceneItemsVisible(bleedGuides, true);
        }
        return false;
    }
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRect fullRect = printer->pageLayout().fullRectPixels(printer->resolution());
    painter.fillRect(fullRect, Qt::white);
    const QRectF target(QPointF(0, 0), QSizeF(fullRect.size()));
    renderDocumentScene(m_scene, m_impl->document, &painter, Constants::kDisplayDpi, target);
    painter.end();

    if (!includeGuides) {
        setSceneItemsVisible(safeGuides, true);
        setSceneItemsVisible(emptyHints, true);
        setSceneItemsVisible(bleedGuides, true);
    }
    m_impl->lastError.clear();
    return true;
}

bool LayoutWorkspaceWidget::print(QPrinter* printer,
                                 const QList<QList<BadgeItem>>& pages,
                                 bool includeGuides) const {
    if (!printer || pages.isEmpty()) {
        m_impl->lastError = QStringLiteral("印刷するページがありません");
        return false;
    }

    QPainter painter(printer);
    if (!painter.isActive()) {
        m_impl->lastError = QStringLiteral("プリンタへの描画を開始できませんでした");
        return false;
    }
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRect fullRect = printer->pageLayout().fullRectPixels(printer->resolution());
    QHash<QString, QPixmap> renderCache;

    for (int i = 0; i < pages.size(); ++i) {
        badge::DocumentData doc = m_impl->document;
        doc.badges = badge::qt::toCoreBadges(pages[i]);
        painter.fillRect(fullRect, Qt::white);
        QGraphicsScene scene;
        populateSceneForDocument(&scene, doc, renderCache);
        const auto safeGuides = sceneItemsByTag(&scene, QString::fromLatin1(kSafeGuideTag));
        const auto emptyHints = sceneItemsByTag(&scene, QString::fromLatin1(kEmptyHintTag));
        const auto bleedGuides = sceneItemsByTag(&scene, QString::fromLatin1(kBleedGuideTag));
        if (!includeGuides) {
            setSceneItemsVisible(safeGuides, false);
            setSceneItemsVisible(emptyHints, false);
            setSceneItemsVisible(bleedGuides, false);
        }
        const QRectF target(QPointF(0, 0), QSizeF(fullRect.size()));
        renderDocumentScene(&scene, doc, &painter, Constants::kDisplayDpi, target);
        drawPageNumberLabel(&painter, target, i, pages.size());
        if (i + 1 < pages.size()) {
            printer->newPage();
        }
    }
    painter.end();
    m_impl->lastError.clear();
    return true;
}

void LayoutWorkspaceWidget::updateSceneRect() {
    const double mmToPx = Constants::kMmToPx;
    m_scene->setSceneRect(-50, -50,
                          m_impl->document.paper.widthMm * mmToPx + 100,
                          m_impl->document.paper.heightMm * mmToPx + 100);
}

void LayoutWorkspaceWidget::rebuildScene() {
    QHash<QString, QPixmap> renderCache;
    updateSceneRect();
    populateSceneForDocument(m_scene, m_impl->document, renderCache);
}
