#include "lightingeffect.h"
#include <QPainter>
#include <QRadialGradient>
#include <cmath>

LightingEffect::LightingEffect(QObject* parent) : QGraphicsEffect(parent) {}

QRectF LightingEffect::boundingRectFor(const QRectF& sourceRect) const {
    return sourceRect.adjusted(-2, -2, 2, 2);
}

void LightingEffect::draw(QPainter* painter) {
    QPoint offset;
    const QPixmap pix = sourcePixmap(Qt::LogicalCoordinates, &offset);
    QRectF r = pix.rect();

    painter->drawPixmap(offset, pix);
    painter->save();
    
    // Sphere shading
    QRadialGradient shade(r.center(), r.width() / 2);
    double rad = m_angle * M_PI / 180.0;
    QPointF lightCenter = r.center() + QPointF(cos(rad) * r.width() * 0.3, -sin(rad) * r.height() * 0.3);
    shade.setCenter(lightCenter);
    shade.setFocalPoint(lightCenter);
    shade.setColorAt(0.0, QColor(255, 255, 255, 0));
    shade.setColorAt(0.35, QColor(0, 0, 0, int(10 * m_intensity)));
    shade.setColorAt(0.7, QColor(0, 0, 0, int(50 * m_intensity)));
    shade.setColorAt(1.0, QColor(0, 0, 0, int(120 * m_intensity)));

    painter->setBrush(shade);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(r);

    // Specular
    QRadialGradient spec(QPointF(r.left() + r.width() * 0.22, r.top() + r.height() * 0.14), r.width() * 0.15);
    spec.setColorAt(0.0, QColor(255, 255, 255, int(200 * m_intensity)));
    spec.setColorAt(0.5, QColor(255, 255, 255, int(80 * m_intensity)));
    spec.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->setBrush(spec);
    double specR = r.width() * 0.12;
    painter->drawEllipse(QPointF(r.left() + r.width() * 0.22, r.top() + r.height() * 0.14), specR, specR * 0.7);

    painter->restore();
}

void LightingEffect::setLightAngle(double degrees) { m_angle = degrees; update(); }
void LightingEffect::setIntensity(double val) { m_intensity = val; update(); }
