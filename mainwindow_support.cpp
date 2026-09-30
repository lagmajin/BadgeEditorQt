#include "mainwindow_support.h"

import badge.document;

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

QString badgeSizeText(const BadgeItem& badge) {
    return QStringLiteral("%1 × %2 mm")
        .arg(QString::number(std::max(0.0, badge.widthMm), 'f', 1),
             QString::number(std::max(0.0, badge.heightMm), 'f', 1));
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

void configurePrinterForDocument(QPrinter& printer,
                                 const badge::DocumentData& document,
                                 int resolution) {
    printer.setResolution(std::max(72, resolution));
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QSizeF(document.paper.widthMm, document.paper.heightMm),
                                  QPageSize::Millimeter));
    printer.setPageOrientation(document.paper.widthMm >= document.paper.heightMm
                                    ? QPageLayout::Landscape
                                    : QPageLayout::Portrait);
}
