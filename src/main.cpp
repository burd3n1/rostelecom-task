#include "database.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <exception>

int main(int argc, char *argv[]) {
    QApplication app(argc,argv);
    app.setApplicationName("QtShapeEditor");
    app.setOrganizationName("QtShapeEditor");
    app.setApplicationVersion("1.0");
    app.setStyle("Fusion");
    QCommandLineParser parser;
    parser.setApplicationDescription("Редактор фигур на C++ / Qt / SQLite");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"db","Путь к базе SQLite", "path"});
    parser.addOption({"demo","Создать демонстрационные данные, если база пуста"});
    parser.addOption({"screenshot","Сохранить снимок окна и завершить работу", "path"});
    parser.process(app);
    const QString path=parser.isSet("db") ? parser.value("db") :
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/shapes.sqlite";
    try {
        Database database(path);
        if (parser.isSet("demo") && database.shapes().isEmpty()) database.addDemo();
        MainWindow window(database); window.show();
        if (parser.isSet("screenshot")) QTimer::singleShot(800,&window,[&]{
            const QString imagePath=parser.value("screenshot");
            QDir().mkpath(QFileInfo(imagePath).absolutePath());
            app.exit(window.grab().save(imagePath) ? 0 : 2);
        });
        return app.exec();
    } catch (const std::exception &error) {
        std::fprintf(stderr,"%s\n",error.what());
        if (!parser.isSet("screenshot")) QMessageBox::critical(nullptr,"Ошибка запуска",QString::fromUtf8(error.what()));
        return 1;
    }
}
