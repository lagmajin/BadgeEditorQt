#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("BadgeEditorQt");
    app.setApplicationVersion("1.0.0");
    MainWindow w;
    w.show();
    return app.exec();
}
