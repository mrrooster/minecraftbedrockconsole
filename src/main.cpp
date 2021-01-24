/***
 *
 * This file is part of Minecraft Bedrock Server Console software.
 *
 * It is licenced under the GNU GPL Version 3.
 *
 * A copy of this can be found in the LICENCE file
 *
 * (c) Ian Clark
 *
 **
*/
#include "mainwindow.h"

#include <QApplication>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Rooster Productions");
    QCoreApplication::setOrganizationDomain("ohmyno.co.uk");
    QCoreApplication::setApplicationName("MCBedrockCons");
    QCoreApplication::setApplicationVersion("202101242304");
    QApplication a(argc, argv);
    QTranslator translator;

    if (translator.load(QLocale(),QLatin1String("mcbc"), QLatin1String("_"), QLatin1String(":/lang"))) {
        qDebug() << "Locale languages: "<<QLocale().uiLanguages();
        qDebug() << "Loaded language: "<<translator.language()<<translator.filePath();
        QCoreApplication::installTranslator(&translator);
    }

    //a.setApplicationDisplayName("Minecraft server console");

    MainWindow w;
    w.show();
    return a.exec();
}
