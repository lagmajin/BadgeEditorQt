#include <QApplication>
#include <QFont>
#include "mainwindow.h"

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
