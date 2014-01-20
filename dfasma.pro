# Project created by QtCreator 2013-11-07T11:24:29
#
# Copyright (C) 2014 Gilles Degottex <gilles.degottex@gmail.com>
# 
# This file is part of DFasma.
# 
# DFasma is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# DFasma is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# A copy of the GNU General Public License is available in the LICENSE.txt
# file provided in the source code of DFasma. Another copy can be found at
# <http://www.gnu.org/licenses/>.

#-------------------------------------------------------------------------------
# Compilation options

#CONFIG += audiofilereading_qt
CONFIG += audiofilereading_libsndfile
#CONFIG += audiofilereading_libav

# Audio file reading libraries -------------------------------------------------

CONFIG(audiofilereading_qt) {
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_QT
    HEADERS  += src/iodsound_load_qt.h
    SOURCES  += src/iodsound_load_qt.cpp
}
CONFIG(audiofilereading_libsndfile) {
    message(Building with libsndfile support.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBSNDFILE
    SOURCES  += src/iodsound_load_libsndfile.cpp
    LIBS += -lsndfile
}
CONFIG(audiofilereading_libav) {
    message(Building with libav support.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBAV
    SOURCES += src/iodsound_load_libav.cpp
    LIBS += -lavformat -lavcodec -lavutil
}

# DFT computation libraries ----------------------------------------------------

LIBS += -lfftw3


# Common configurations --------------------------------------------------------

QT += core gui multimedia opengl

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = dfasma
TEMPLATE = app

SOURCES   += src/main.cpp\
             src/wmainwindow.cpp \
             src/iodsound.cpp \
             src/gvwaveform.cpp \
             src/gvspectrum.cpp \
             src/wdialogsettings.cpp \
             external/audioengine/audioengine.cpp \
             external/CFFTW3.cpp

HEADERS   += src/wmainwindow.h \
             src/iodsound.h \
             src/gvwaveform.h \
             src/gvspectrum.h \
             src/wdialogsettings.h \
             external/audioengine/audioengine.h \
             external/CFFTW3.h

FORMS     += src/wmainwindow.ui \
             src/wdialogsettings.ui

RESOURCES += ressources.qrc
