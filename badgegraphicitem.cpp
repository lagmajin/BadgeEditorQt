#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QFileInfo>

BadgeGraphicItem::BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_badge(badge) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setTransformOriginPoint(boundingRect().center());
    loadImage();
}

QRectF BadgeGraphicItem::boundingRect() const {
    const double mmToPx = 96.0 / 25.4;
    double pw = m_badge.widthMm * mmToPx;
    double ph = m_badge.heightMm * mmToPx;
    double margin = m_badge.isSelected ? 3.0 : 2.0;
    return QRectF(-margin, -margin, pw + margin * 2, ph + margin * 2);
}

void BadgeGraphicItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    double mmToPx = 96.0 / 25.4;
    double pw = m_badge.widthMm * mmToPx;
    double ph = m_badge.heightMm * mmToPx;
    double margin = m_badge.isSelected ? 3.0 : 2.0;
    QRectF r(margin, margin, pw, ph);
    renderCore(painter, r);
    
    // Border
    painter->setPen(QPen(m_badge.isSelected ? Qt::red : Qt::black, m_badge.isSelected ? 2 : 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r);
}

void BadgeGraphicItem::renderCore(QPainter* painter, const QRectF& r) {
    painter->save();
    if (m_badge.clipToCircle) {
        QPainterPath path;
        path.addEllipse(r);
        painter->setClipPath(path);
    }

    // Render layers
    for (const auto& layer : m_badge.layers) {
        if (!layer.visible) continue;
        QPixmap img;
        if (!layer.imagePath.isEmpty() && QFileInfo::exists(layer.imagePath))
            img.load(layer.imagePath);
        if (!img.isNull()) {
            painter->setOpacity(layer.opacity);
            QRectF lr = r.adjusted(layer.offsetX * 96.0 / 25.4, layer.offsetY * 96.0 / 25.4, 0, 0);
            painter->drawPixmap(lr.toRect(), img);
        }
    }

    // Fallback to main image if no layers
    if (m_badge.layers.isEmpty()) {
        if (!m_processed.isNull()) {
            painter->drawPixmap(r.toRect(), m_processed);
        } else if (!m_thumbnail.isNull()) {
            painter->drawPixmap(r.toRect(), m_thumbnail);
        } else if (m_badge.imagePath.isEmpty() || !QFileInfo::exists(m_badge.imagePath)) {
            painter->fillRect(r, QColor(240, 240, 240));
            QPen dashPen(Qt::lightGray);
            dashPen.setDashPattern({2, 2});
            painter->setPen(dashPen);
            painter->drawEllipse(r);
            painter->setPen(Qt::gray);
            painter->setFont(QFont("Arial", 7));
            painter->drawText(r, Qt::AlignCenter, "Image\nDrop/DoubleClick");
        } else {
            painter->drawPixmap(r.toRect(), m_thumbnail);
        }
    } else {
        // Has layers but no main image = empty background
    }

    if (!m_badge.displayText.isEmpty()) {
        painter->setOpacity(1.0);
        QFont font("Arial", 10);
        painter->setFont(font);
        painter->setPen(Qt::black);
        painter->drawText(r, Qt::AlignCenter, m_badge.displayText);
    }
    painter->restore();
}

void BadgeGraphicItem::loadImage() {
    if (m_badge.imagePath.isEmpty() || !QFileInfo::exists(m_badge.imagePath)) return;
    m_thumbnail.load(m_badge.imagePath);
    applyColorCorrection();
}

void BadgeGraphicItem::applyColorCorrection() {
    if (m_badge.brightness == 0 && m_badge.contrast == 0 && m_badge.saturation == 0) {
        m_processed = QPixmap();
        return;
    }
    if (!m_thumbnail.isNull())
        m_processed = ImageProcessor::applyCorrection(m_thumbnail, m_badge.brightness, m_badge.contrast, m_badge.saturation);
}

void BadgeGraphicItem::syncFromBadge() {
    prepareGeometryChange();
    setTransformOriginPoint(boundingRect().center());
    setPos(m_badge.xMm * 96.0 / 25.4, m_badge.yMm * 96.0 / 25.4);
    setRotation(m_badge.rotation);
    update();
}

void BadgeGraphicItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        emit badgeClicked(this);
}

void BadgeGraphicItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    Q_UNUSED(event);
    emit badgeDoubleClicked(this);
}

QVariant BadgeGraphicItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        const double mmToPx = 96.0 / 25.4;
        m_badge.xMm = pos().x() / mmToPx;
        m_badge.yMm = pos().y() / mmToPx;
    }
    return QGraphicsObject::itemChange(change, value);
}
