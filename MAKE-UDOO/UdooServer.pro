QT += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialport

TARGET = UdooServer
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++1y

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -Ofast -march=native -flto -fopenmp
QMAKE_LFLAGS_RELEASE -= -Wl,-O1
QMAKE_LFLAGS_RELEASE += -Wl,-O3 -Wl,-flto

QMAKE_CXXFLAGS_DEBUG -= -O1
QMAKE_CXXFLAGS_DEBUG -= -O2
QMAKE_CXXFLAGS_DEBUG += -O0 -Wno-unknown-pragmas

SOURCES += main.cpp\
	Server.cpp \
        Track.cpp \
	PlayThread.cpp \
	SaveManager.cpp \
        SerialManager.cpp

HEADERS += Server.h \
	Track.h \
	PlayThread.h \
        SongData.h \
	SaveManager.h \
        SerialManager.h

FORMS   +=

INCLUDEPATH += $$PWD/../../watermarking/src/libwatermark
DEPENDPATH += $$PWD/../../watermarking/src/libwatermark

LIBS+= -lgomp -lsndfile -lrtaudio # -lavcodec -lavformat -lavutil
#LIBS+= -lgcov


INCLUDEPATH += /usr/local/include/KF5/KArchive
LIBS += -lKF5Archive

RESOURCES += 

OTHER_FILES += 


#### Libraries ####
  ##  Oscpack  ##
unix:!macx: LIBS += -L$$PWD/../../../git/oscpack/build/ -loscpack

INCLUDEPATH += $$PWD/../../../git/oscpack/src
DEPENDPATH += $$PWD/../../../git/oscpack/src

unix:!macx: PRE_TARGETDEPS += $$PWD/../../../git/oscpack/build/liboscpack.a
