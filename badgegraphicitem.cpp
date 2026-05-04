#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QFileInfo>

BadgeGraphicItem::BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_badge(badge) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    loadImage();
}

QRectF BadgeGraphicItem::boundingRect() const {
    const double mmToPx = 96.0 / 25.4;
    return QRectF(0, 0, m_badge.widthMm * mmToPx, m_badge.heightMm * mmToPx);
}

void BadgeGraphicItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    QRectF r = boundingRect();
    renderCore(painter, r);
    
    if (m_badge.isSelected) {
        painter->setPen(QPen(Qt::red, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(-1, -1, 1, 1));
    } else {
        painter->setPen(QPen(Qt::black, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r);
    }
}

void BadgeGraphicItem::renderCore(QPainter* painter, const QRectF& r) {
    painter->save();
    if (m_badge.clipToCircle) {
        QPainterPath path;
        path.addEllipse(r);
        painter->setClipPath(path);
    }

    if (!m_processed.isNull()) {
        painter->drawPixmap(r.toRect(), m_processed);
    } else if (!m_thumbnail.isNull()) {
        painter->drawPixmap(r.toRect(), m_thumbnail);
    } else {
        painter->fillRect(r, QColor(240, 240, 240));
        QPen dashPen(Qt::lightGray);
        dashPen.setDashPattern({2, 2});
        painter->setPen(dashPen);
        painter->drawEllipse(r);
        painter->setPen(Qt::gray);
        painter->setFont(QFont("Arial", 7));
        painter->drawText(r, Qt::AlignCenter, "Image\nDrop/DoubleClick");
    }

    if (!m_badge.displayText.isEmpty()) {
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
    setPos(m_badge.xMm * 96.0 / 25.4, m_badge.yMm * 96.0 / 25.4);
    setRotation(m_badge.rotation);
    prepareGeometryChange();
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
