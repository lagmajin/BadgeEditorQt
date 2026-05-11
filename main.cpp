#include <QApplication>
#include <QCoreApplication>
#include <QFont>
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
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("BadgeEditorQt");
    app.setApplicationName("BadgeEditorQt");
    app.setApplicationVersion("1.0.0");

#ifdef Q_OS_WIN
    QApplication::setDesktopFileName(QStringLiteral("BadgeEditorQt"));
#endif

    QFont font = app.font();
    font.setPointSize(font.pointSize() + 1);
    app.setFont(font);
    MainWindow w;
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
