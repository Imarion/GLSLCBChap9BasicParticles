QT += gui core

CONFIG += c++11

TARGET = BasicParticles
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += $$PWD/../glm/glm

SOURCES += main.cpp \
    BasicParticles.cpp \
    vboplane.cpp

HEADERS += \
    BasicParticles.h \
    vboplane.h

OTHER_FILES += \
    fshader.txt \
    vshader.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt
