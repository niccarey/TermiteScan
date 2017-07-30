include(include.pri)

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

CONFIG(release, debug|release){
    TARGET = TermiteScan-run
    DESTDIR = $$PWD/../TermiteScan-rel_v2.1
}

QMAKE_CXXFLAGS += -std=c++11 -fpermissive -O3
QMAKE_CXXFLAGS += -Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-parameter
CONFIG += link_pkgconfig
PKGCONFIG += glfw3 gl glu libusb-1.0

INCLUDEPATH += /home/nic/Documents/librealsense-master/include/
INCLUDEPATH += /home/nic/Documents/librealsense-master/src/
INCLUDEPATH += /usr/include/

LIBS += -L$$DESTDIR/ -lrealsense

LIBS += -pthread

SOURCES += \
    TermiteScan.cpp \
    depthimageframe.cpp \
    irimageframe.cpp \
    colimageframe.cpp

LIBS += -L$$DESTDIR/ -lrealsense
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
