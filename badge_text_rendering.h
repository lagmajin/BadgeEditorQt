#ifndef BADGE_TEXT_RENDERING_H
#define BADGE_TEXT_RENDERING_H

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <algorithm>

namespace badge::rendering {

inline void drawBadgeDisplayText(QPainter* painter, const QRectF& badgeRect, const QString& text) {
    if (!painter || text.isEmpty() || badgeRect.isEmpty()) {
        return;
    }

    const QRectF textRect = badgeRect.adjusted(badgeRect.width() * 0.1,
                                               badgeRect.height() * 0.1,
                                               -badgeRect.width() * 0.1,
                                               -badgeRect.height() * 0.1);
    QFont font(QStringLiteral("Arial"));
    font.setBold(true);
    font.setPixelSize(std::clamp(int(std::min(textRect.width(), textRect.height()) * 0.16), 8, 40));
    constexpr int textFlags = Qt::AlignCenter | Qt::TextWordWrap;
    QFontMetricsF metrics(font);
    while (font.pixelSize() > 8) {
        const QRectF bounds = metrics.boundingRect(textRect, textFlags, text);
        if (bounds.width() <= textRect.width() && bounds.height() <= textRect.height()) {
            break;
        }
        font.setPixelSize(font.pixelSize() - 1);
        metrics = QFontMetricsF(font);
    }

    painter->save();
    painter->setOpacity(1.0);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(textRect.translated(-1.0, -1.0), textFlags, text);
    painter->setPen(Qt::black);
    painter->drawText(textRect, textFlags, text);
    painter->restore();
}

} // namespace badge::rendering

#endif // BADGE_TEXT_RENDERING_H
