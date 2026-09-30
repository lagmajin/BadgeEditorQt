#ifndef MAINWINDOW_SUPPORT_H
#define MAINWINDOW_SUPPORT_H

#include <QPrinter>
#include <QString>
#include <QColor>
#include <QtGlobal>

class QWidget;
struct BadgeItem;
namespace badge { struct DocumentData; }

void showOperationWarning(QWidget* parent,
                          const QString& title,
                          const QString& action,
                          const QString& path = QString(),
                          const QString& detail = QString());

QColor blend(const QColor& a, const QColor& b, qreal ratio);

#endif
