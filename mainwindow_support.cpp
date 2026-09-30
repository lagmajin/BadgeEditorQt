#include "mainwindow_support.h"

#include <QDir>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QStringList>
#include <algorithm>

QColor blend(const QColor& a, const QColor& b, qreal ratio) {
    const qreal clamped = qBound<qreal>(0.0, ratio, 1.0);
    return QColor::fromRgbF(
        a.redF() * (1.0 - clamped) + b.redF() * clamped,
        a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
        a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
        a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

void showOperationWarning(QWidget* parent,
                          const QString& title,
                          const QString& action,
                          const QString& path,
                          const QString& detail) {
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
