#ifndef LIGHTINGEFFECT_H
#define LIGHTINGEFFECT_H

#include <QGraphicsEffect>

class LightingEffect : public QGraphicsEffect {
public:
    explicit LightingEffect(QObject* parent = nullptr);
    void draw(QPainter* painter) override;
    QRectF boundingRectFor(const QRectF& sourceRect) const override;
    
    void setLightAngle(double degrees);
    void setIntensity(double val);
    double lightAngle() const { return m_angle; }
    double intensity() const { return m_intensity; }

private:
    double m_angle = 315.0;
    double m_intensity = 0.35;
};

#endif
