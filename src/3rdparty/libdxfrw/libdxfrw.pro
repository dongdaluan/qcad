exists(../../../shared.pri) {
    include( ../../../shared.pri )
}

win32-msvc {
  DEFINES += _CRT_SECURE_NO_DEPRECATE
}

INCLUDEPATH += src
HEADERS = \
    drw_base.h \
    drw_classes.h \
    drw_entities.h \
    drw_header.h \
    drw_interface.h \
    drw_objects.h \
    libdwgr.h \
    libdxfrw.h

SOURCES = \
    drw_classes.cpp \
    drw_entities.cpp \
	drw_header.cpp \
	drw_objects.cpp \
	libdwgr.cpp \
	libdxfrw.cpp \
        intern/drw_dbg.cpp \
	intern/drw_textcodec.cpp \
	intern/dwgbuffer.cpp \
	intern/dwgreader.cpp \
	intern/dwgreader15.cpp \
	intern/dwgreader18.cpp \
	intern/dwgreader21.cpp \
	intern/dwgreader24.cpp \
	intern/dwgreader27.cpp \
	intern/dwgutil.cpp \
	intern/dxfreader.cpp \
	intern/dxfwriter.cpp \
	intern/rscodec.cpp
	
	
TARGET = dxfrw
TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt
DEFINES += DXFLIB_LIBRARY
RC_FILE = 

#dxflib.rc
