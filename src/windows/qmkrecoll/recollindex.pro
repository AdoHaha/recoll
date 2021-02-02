
QT       -= core gui

TARGET = recollindex
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += BUILDING_RECOLL

SOURCES += \
../../index/recollindex.cpp \
../../index/checkretryfailed.cpp \
../../index/rclmonprc.cpp \
../../index/rclmonrcv.cpp 

INCLUDEPATH += ../../common ../../index ../../internfile ../../query \
            ../../unac ../../utils ../../aspell ../../rcldb ../../qtgui \
            ../../xaposix ../../confgui ../../bincimapmime 

windows {
  DEFINES += UNICODE
  DEFINES += PSAPI_VERSION=1
  DEFINES += RCL_MONITOR
  DEFINES += __WIN32__
  contains(QMAKE_CC, gcc){
    # MingW
    QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
    LIBS += \
       ../build-librecoll-Desktop_Qt_5_8_0_MinGW_32bit-Release/release/librecoll.dll \
       -lshlwapi -lpsapi -lkernel32
    }

    contains(QMAKE_CC, cl){
      # MSVC
      RECOLLDEPS = ../../../../recolldeps/msvc
      DEFINES += USING_STATIC_LIBICONV
      LIBS += \
        -L../build-librecoll-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release \
          -llibrecoll \
        $$RECOLLDEPS/libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc/libxml2.lib \
        $$RECOLLDEPS/libxslt/libxslt-1.1.29/win32/bin.msvc/libxslt.lib \
        -L../build-libxapian-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release \
          -llibxapian \
        $$RECOLLDEPS/zlib-1.2.11/zdll.lib \
 -L$$RECOLLDEPS/build-libiconv-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release \
        -llibiconv -lShell32  \
        -lrpcrt4 -lws2_32 -luser32 \
        -lshlwapi -lpsapi -lkernel32
    }

  INCLUDEPATH += ../../windows
}

mac {
  QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
  SOURCES += \
    ../../utils/closefrom.cpp \
    ../../utils/execmd.cpp \
    ../../utils/netcon.cpp \
    ../../utils/rclionice.cpp

  LIBS += \
     ../build-librecoll-Desktop_Qt_5_14_2_clang_64bit-Release/liblibrecoll.a \
     ../../../../xapian-core-1.4.18/.libs/libxapian.a \
     -lxslt -lxml2 -liconv -lz
}
