#include <QApplication>
#include <QFont>
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
    QFont font = app.font();
    font.setPointSize(font.pointSize() + 1);
    app.setFont(font);
    MainWindow w;
    w.show();
    return app.exec();
}
#endif
