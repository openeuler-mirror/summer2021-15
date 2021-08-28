#-------------------------------------------------
#
# Project created by QtCreator
#
#-------------------------------------------------

QT       += core gui

QTPLUGIN += qsvgicon

# Exclude unused plugins to avoid bloating of statically-linked build
QT_PLUGINS -= qdds qicns qjp2 qmng qtga qtiff qwbmp qwebp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ImageWriter
TEMPLATE = app


SOURCES += mainwindow.cpp \
    imagewriter.cpp \
    common.cpp \
    usbdevice.cpp

HEADERS  += mainwindow.h \
    imagewriter.h \
    common.h \
    usbdevice.h

FORMS    += main.ui

RESOURCES += \
    ImageWriter.qrc

# The following variables can be used for automatic VERSIONINFO generating,
# but unfortunately it is impossible to use them together with RC_FILE or RES_FILE
# which is needed for specifying the executable file icon in Windows.
VERSION = 1.0.0
#QMAKE_TARGET_PRODUCT = "Image Writer"
#QMAKE_TARGET_DESCRIPTION = "Creating bootable installation USB disk"
#QMAKE_TARGET_COPYRIGHT = "Copyright (c) 2021 Hollow Man"

LIBS += -ldl
QMAKE_LFLAGS_RELEASE -= -Wl,-z,now       # Make sure weak symbols are not resolved on link-time
QMAKE_LFLAGS_DEBUG -= -Wl,-z,now
QMAKE_LFLAGS -= -Wl,-z,now
GCCSTRVER = $$system(g++ -dumpversion)
GCCVERSION = $$split(GCCSTRVER, .)
GCCV_MJ = $$member(GCCVERSION, 0)
GCCV_MN = $$member(GCCVERSION, 1)
greaterThan(GCCV_MJ, 3) {
    lessThan(GCCV_MN, 7) {
        QMAKE_CXXFLAGS += -std=gnu++0x
    }
    greaterThan(GCCV_MN, 6) {
        QMAKE_CXXFLAGS += -std=gnu++11
    }
}
contains(QT_CONFIG, static) {
    # Static build is meant for releasing, clean up the binary
    QMAKE_LFLAGS += -s
}

TRANSLATIONS = lang/imagewriter-zh_CN.ts \

# install
isEmpty(PREFIX) {
  PREFIX = /usr
}
 
isEmpty(BINDIR) {
  BINDIR = $$PREFIX/bin
}
 
isEmpty(DATADIR) {
  DATADIR = $$PREFIX/share
}

target.path = $$BINDIR
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro
sources.path = .

desktop.path = $$DATADIR/applications
desktop.files += imagewriter.desktop

i18n.path = $$DATADIR/qt5/translations
i18n.files += lang/*.qm

i18ncnscript.path = $$DATADIR/locale/zh_CN/LC_MESSAGES
i18ncnscript.files += po/*.mo

icon.path = $$DATADIR/icons/hicolor/scalable/apps
icon.files += res/icon-iw.svg

script.path = $$PREFIX/bin
script.files += verifyimagewriter

INSTALLS += target desktop i18n i18ncnscript icon script
