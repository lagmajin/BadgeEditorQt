#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QFileInfo>
#include <QStyleOptionGraphicsItem>
#include <QCursor>

// --- ResizeHandle: small corner square for resizing ---
class ResizeHandle : public QGraphicsRectItem {
public:
    enum Corner { TL, TR, BL, BR };
    ResizeHandle(Corner c, BadgeGraphicItem* parent) : QGraphicsRectItem(parent), m_corner(c), m_badge(parent) {
        setRect(-6, -6, 12, 12);
        setBrush(Qt::white);
        setPen(QPen(Qt::blue, 1.5));
        setFlag(ItemIgnoresTransformations, true);
        setFlag(ItemIsMovable, false);
        setFlag(ItemIsSelectable, false);
        setAcceptHoverEvents(true);
        setZValue(1000);
        switch (c) {
            case TL: case BR: setCursor(Qt::SizeFDiagCursor); break;
            case TR: case BL: setCursor(Qt::SizeBDiagCursor); break;
        }
    }
    Corner corner() const { return m_corner; }
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override { m_dragging = true; e->accept(); }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (!m_dragging) return;
        QPointF delta = e->scenePos() - e->lastScenePos();
        const double mmToPx = 96.0 / 25.4;
        BadgeItem& b = m_badge->badge();
        if (m_corner == TL || m_corner == BL) { b.widthMm -= delta.x() / mmToPx; b.xMm += delta.x() / mmToPx; }
        else { b.widthMm += delta.x() / mmToPx; }
        if (m_corner == TL || m_corner == TR) { b.heightMm -= delta.y() / mmToPx; b.yMm += delta.y() / mmToPx; }
        else { b.heightMm += delta.y() / mmToPx; }
        if (b.widthMm < 5) b.widthMm = 5;
        if (b.heightMm < 5) b.heightMm = 5;
        m_badge->syncFromBadge();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override { m_dragging = false; }
private:
    Corner m_corner;
    BadgeGraphicItem* m_badge;
    bool m_dragging = false;
};

BadgeGraphicItem::BadgeGraphicItem(const BadgeItem& badge, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_badge(badge) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setTransformOriginPoint(boundingRect().center());
    loadImage();
    createHandles();
}

void BadgeGraphicItem::createHandles() {
    // Remove old handles
    for (auto* h : m_handles) { if (h->scene()) h->scene()->removeItem(h); delete h; }
    m_handles.clear();
    if (!m_badge.isSelected) return;
    const double mmToPx = 96.0 / 25.4;
    double pw = m_badge.widthMm * mmToPx;
    double ph = m_badge.heightMm * mmToPx;
    double margin = m_badge.isSelected ? 3.0 : 2.0;
    QPointF corners[4] = {QPointF(-margin, -margin), QPointF(pw+margin, -margin), QPointF(-margin, ph+margin), QPointF(pw+margin, ph+margin)};
    int types[4] = {0,1,2,3};
    for (int i = 0; i < 4; ++i) {
        auto* h = new ResizeHandle(static_cast<ResizeHandle::Corner>(types[i]), this);
        h->setPos(corners[i]);
        m_handles.append(h);
    }
}

void BadgeGraphicItem::updateHandles() { createHandles(); }

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
    
    // Border (handles are drawn separately as child items)
    if (m_badge.isSelected) {
        painter->setPen(QPen(Qt::red, 1));
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

    bool drewAnything = false;

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
            drewAnything = true;
        }
    }

    // Draw main image (always, behind or in front of layers)
    if (!m_thumbnail.isNull()) {
        painter->setOpacity(1.0);
        painter->drawPixmap(r.toRect(), m_thumbnail);
        drewAnything = true;
    }

    // Placeholder if nothing drawn
    if (!drewAnything) {
        painter->fillRect(r, QColor(240, 240, 240));
        QPen dashPen(Qt::lightGray);
        dashPen.setDashPattern({2, 2});
        painter->setPen(dashPen);
        painter->drawEllipse(r.adjusted(2,2,-2,-2));
        painter->setPen(Qt::gray);
        painter->setFont(QFont("Arial", 7));
        painter->drawText(r, Qt::AlignCenter, "Drop image\nor DoubleClick");
    }

    if (!m_badge.displayText.isEmpty()) {
        painter->setOpacity(1.0);
        QFont font("Arial", 10);
        painter->setFont(font);
        painter->setPen(Qt::white);
        painter->drawText(r.adjusted(1,1,1,1), Qt::AlignCenter, m_badge.displayText);
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
    updateHandles();
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
    if (change == ItemSelectedHasChanged) {
        m_badge.isSelected = value.toBool();
        updateHandles();
        prepareGeometryChange();
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
