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

FORMS    += maindialog.ui

RESOURCES += \
    ImageWriter.qrc
