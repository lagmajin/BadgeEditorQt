#ifndef MAINWINDOW_SUPPORT_H
#define MAINWINDOW_SUPPORT_H

#include <QPrinter>
#include <QString>
#include <QColor>
#include <QtGlobal>
#include "badgeitem.h"

class QWidget;
namespace badge { struct DocumentData; }

void showOperationWarning(QWidget* parent,
                          const QString& title,
                          const QString& action,
                          const QString& path = QString(),
                          const QString& detail = QString());

void configurePrinterForDocument(QPrinter& printer,
                                 const badge::DocumentData& document,
                                 int resolution);

QColor blend(const QColor& a, const QColor& b, qreal ratio);
QString badgeSizeText(const BadgeItem& badge);

#endif
