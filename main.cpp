#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QPixmap>
#include <QTimer>
#include <QStringList>
#include <cstdio>
#include "mainwindow.h"

#ifndef Q_OS_WIN
int main(int, char**) {
    std::fputs("BadgeEditorQt is supported on Windows only.\n", stderr);
    return 1;
}
#else
namespace {

QIcon makeAppIcon() {
    QPixmap pixmap(256, 256);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(32.0, 24.0, 224.0, 232.0);
    bg.setColorAt(0.0, QColor(247, 169, 48));
    bg.setColorAt(1.0, QColor(214, 112, 35));
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(QRectF(22.0, 22.0, 212.0, 212.0), 48.0, 48.0);

    painter.setBrush(QColor(255, 255, 255, 230));
    painter.drawEllipse(QPointF(128.0, 112.0), 72.0, 72.0);

    painter.setBrush(QColor(34, 36, 42));
    painter.drawEllipse(QPointF(128.0, 112.0), 54.0, 54.0);

    QFont font("Segoe UI");
    font.setBold(true);
    font.setPixelSize(126);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(QRectF(64.0, 58.0, 128.0, 132.0), Qt::AlignCenter, QStringLiteral("B"));
    painter.end();

    return QIcon(pixmap);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("BadgeEditorQt");
    app.setApplicationName("BadgeEditorQt");
    app.setApplicationVersion("1.0.0");
    QIcon appIcon(QStringLiteral(":/app/appicon.png"));
    if (appIcon.isNull()) {
        appIcon = makeAppIcon();
    }
    app.setWindowIcon(appIcon);

#ifdef Q_OS_WIN
    QApplication::setDesktopFileName(QStringLiteral("BadgeEditorQt"));
#endif

    QFont font = app.font();
    font.setPointSize(font.pointSize() + 1);
    app.setFont(font);
    MainWindow w;
    w.setWindowIcon(appIcon);
    w.show();

    const QStringList args = QCoreApplication::arguments();
    QString startupPath;
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args[i];
        if (arg == QStringLiteral("--new")) {
            continue;
        }
        if (arg == QStringLiteral("--open") && i + 1 < args.size()) {
            startupPath = args[++i];
            continue;
        }
        if (!arg.startsWith(QLatin1Char('-')) && startupPath.isEmpty()) {
            startupPath = arg;
        }
    }

    if (!startupPath.isEmpty()) {
        QTimer::singleShot(0, &w, [&w, startupPath]() {
            w.openProjectPath(startupPath);
        });
    }
    return app.exec();
}
#endif
