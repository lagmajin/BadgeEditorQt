#include "badgegraphicitem.h"
#include "imageprocessor.h"
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QFileInfo>
#include <QStyleOptionGraphicsItem>
#include <QCursor>
#include <wobjectimpl.h>

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
    QPointF m_startPos;
    
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override { m_dragging = true; m_startPos = e->scenePos(); e->accept(); }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (!m_dragging) return;
        QPointF deltaLocal = m_badge->mapFromScene(e->scenePos()) - m_badge->mapFromScene(m_startPos);
        m_startPos = e->scenePos();
        const double mmToPx = 96.0 / 25.4;
        BadgeItem& b = m_badge->badge();
        if (m_corner == TL || m_corner == BL) { b.widthMm -= deltaLocal.x() / mmToPx; b.xMm += deltaLocal.x() / mmToPx; }
        else { b.widthMm += deltaLocal.x() / mmToPx; }
        if (m_corner == TL || m_corner == TR) { b.heightMm -= deltaLocal.y() / mmToPx; b.yMm += deltaLocal.y() / mmToPx; }
        else { b.heightMm += deltaLocal.y() / mmToPx; }
        if (b.widthMm < 5) b.widthMm = 5;
        if (b.heightMm < 5) b.heightMm = 5;
        m_badge->updateHandles();
        m_badge->update();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        m_dragging = false;
        m_badge->syncFromBadge();
    }
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
    updateHandles();
}

W_OBJECT_IMPL(BadgeGraphicItem)

void BadgeGraphicItem::updateHandles() {
    if (!m_badge.isSelected) {
        for (auto* h : m_handles) { if (h->scene()) h->scene()->removeItem(h); delete h; }
        m_handles.clear();
        return;
    }
    const double mmToPx = 96.0 / 25.4;
    double pw = m_badge.widthMm * mmToPx;
    double ph = m_badge.heightMm * mmToPx;
    double margin = 3.0;
    QPointF corners[4] = {QPointF(-margin, -margin), QPointF(pw+margin, -margin), QPointF(-margin, ph+margin), QPointF(pw+margin, ph+margin)};
    int types[4] = {0,1,2,3};
    for (int i = 0; i < 4; ++i) {
        if (i < m_handles.size()) {
            m_handles[i]->setPos(corners[i]);
        } else {
            auto* h = new ResizeHandle(static_cast<ResizeHandle::Corner>(types[i]), this);
            h->setPos(corners[i]);
            m_handles.append(h);
        }
    }
    while (m_handles.size() > 4) {
        auto* h = m_handles.takeLast();
        if (h->scene()) h->scene()->removeItem(h);
        delete h;
    }
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
    const QPixmap& baseImage = m_processed.isNull() ? m_thumbnail : m_processed;
    if (!baseImage.isNull()) {
        painter->setOpacity(1.0);
        painter->drawPixmap(r.toRect(), baseImage);
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
    m_loadedImagePath = m_badge.imagePath;
    m_thumbnail = QPixmap();
    m_processed = QPixmap();
    m_colorSpaceLabel = QStringLiteral("読み込みなし");

    if (m_badge.imagePath.isEmpty() || !QFileInfo::exists(m_badge.imagePath)) {
        update();
        return;
    }

    QString label;
    m_thumbnail = QPixmap::fromImage(ImageProcessor::loadImage(m_badge.imagePath, &label));
    m_colorSpaceLabel = label;
    applyColorCorrection();
}

void BadgeGraphicItem::applyColorCorrection() {
    if (m_badge.brightness == 0 && m_badge.contrast == 0 && m_badge.saturation == 0) {
        m_processed = QPixmap();
        update();
        return;
    }
    if (!m_thumbnail.isNull())
        m_processed = ImageProcessor::applyCorrection(m_thumbnail, m_badge.brightness, m_badge.contrast, m_badge.saturation);
    update();
}

void BadgeGraphicItem::syncFromBadge() {
    prepareGeometryChange();
    setTransformOriginPoint(boundingRect().center());
    if (m_badge.imagePath != m_loadedImagePath) {
        loadImage();
    }
    bool changed = false;
    double px = m_badge.xMm * 96.0 / 25.4;
    double py = m_badge.yMm * 96.0 / 25.4;
    if (qAbs(pos().x() - px) > 0.1 || qAbs(pos().y() - py) > 0.1) {
        setPos(px, py);
        changed = true;
    }
    if (qAbs(rotation() - m_badge.rotation) > 0.1) {
        setRotation(m_badge.rotation);
        changed = true;
    }
    updateHandles();
    update();
    if (changed)
        emit badgeMoved(this);
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
        emit badgeMoved(this);
    }
    if (change == ItemSelectedHasChanged) {
        m_badge.isSelected = value.toBool();
        updateHandles();
        prepareGeometryChange();
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
