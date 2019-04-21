include(include.pri)

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

CONFIG(release, debug|release){
    TARGET = TestStreams
    DESTDIR = $$PWD/../TestingStreams
}

QMAKE_CXXFLAGS += -std=c++11 -fpermissive -O3
QMAKE_CXXFLAGS += -Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-parameter
CONFIG += link_pkgconfig
PKGCONFIG += glfw3 gl glu libusb-1.0 opencv

INCLUDEPATH += /home/ssr/Documents/librealsense-2.20.0/include/
INCLUDEPATH += /home/ssr/Documents/librealsense-2.20.0/src/
INCLUDEPATH += /usr/include/

LIBS += -L$$DESTDIR/ -lrealsense2

LIBS += -pthread

SOURCES += \
    irFramesTest.cpp \
    depthimageframe.cpp \
    irimageframe.cpp \
    colimageframe.cpp

LIBS += -L/usr/lib/x86_64-linux-gnu -lboost_system
LIBS += -L/usr/lib/x86_64-linux-gnu -lboost_filesystem
LIBS += -L/usr/lib/x86_64-linux-gnu -lboost_date_time
LIBS += -L/usr/lib/x86_64-linux-gnu -lboost_thread
LIBS += -L/usr/lib/x86_64-linux-gnu -lboost_chrono
LIBS += -L/usr/lib/x86_64-linux-gnu -ljpeg
LIBS += -pthread


#PRE_TARGETDEPS += $$DESTDIR/librealsense.a

HEADERS += \
    depthimageframe.h \
    irimageframe.h \
    colimageframe.h
