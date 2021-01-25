QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += src

SOURCES += \
    src/backup/backupmanager.cpp \
    src/server/bedrockserver.cpp \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/backup/backupmanager.h \
    src/server/bedrockserver.h \
    src/mainwindow.h

FORMS += \
    src/mainwindow.ui

TRANSLATIONS += \
    lang/mcbc_en.ts \
    lang/mcbc_en_GB.ts

win32:CONFIG(release, debug|release) {
    QMAKE_POST_LINK = mkdir dist & copy release\*.exe dist\ && %QTDIR%\bin\windeployqt dist
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

DISTFILES += \
    LICENCE \
    README.md
