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

# SDIF support

DEFINES += FFT_FFTW3
#DEFINES += FFT_FFTREAL

CONFIG += sdifreading

# TODO move CONFIG to DEFINES
CONFIG += audiofilereading_libsndfile
#CONFIG += audiofilereading_qt
#CONFIG += audiofilereading_libav
#CONFIG += audiofilereading_builtin # This is a minimal audio file reader which
                                    # should be use only for portablity test purpose

CONFIG(sdifreading) {
    message(Building with SDIF file reader.)
    DEFINES += SUPPORT_SDIF
    #LIBS += /u/formes/share/lib/x86_64-Linux-rh50/libsdif.a
    LIBS += -L/u/formes/share/lib/x86_64-Linux-rh65 -lEasdif
    QMAKE_CXXFLAGS  += -I/u/formes/share/include
}

# Audio file reading libraries -------------------------------------------------

CONFIG(audiofilereading_builtin) {
    message(Building with minimal built-in audio file reader.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_BUILTIN
    HEADERS  += external/wavfile/wavfile.h
    SOURCES  += external/wavfile/wavfile.cpp external/iodsound_load_builtin.cpp
}
CONFIG(audiofilereading_qt) {
    message(Building with Qt support for audio file reading.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_QT
    HEADERS  += external/iodsound_load_qt.h
    SOURCES  += external/iodsound_load_qt.cpp
}
CONFIG(audiofilereading_libsndfile) {
    message(Building with libsndfile support for audio file reading.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBSNDFILE
    SOURCES  += external/iodsound_load_libsndfile.cpp
    LIBS += -lsndfile
}
CONFIG(audiofilereading_libav) {
    message(Building with libav support for audio file reading.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBAV
    SOURCES += external/iodsound_load_libav.cpp
    LIBS += -lavformat -lavcodec -lavutil
}

# DFT computation libraries ----------------------------------------------------

contains(DEFINES, FFT_FFTW3){
    LIBS += -lfftw3
}
contains(DEFINES, FFT_FFTREAL){
    SOURCES +=
    HEADERS +=  external/FFTReal/FFTReal.h \
                external/FFTReal/FFTReal.hpp \
                external/FFTReal/def.h \
                external/FFTReal/DynArray.h \
                external/FFTReal/DynArray.hpp \
                external/FFTReal/OscSinCos.h \
                external/FFTReal/OscSinCos.hpp
}


# Common configurations --------------------------------------------------------

QT += core gui multimedia opengl

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = dfasma
TEMPLATE = app

SOURCES   += src/main.cpp\
             src/wmainwindow.cpp \
             src/filetype.cpp \
             src/ftsound.cpp \
             src/ftfzero.cpp \
             src/ftlabels.cpp \
             src/gvwaveform.cpp \
             src/gvspectrum.cpp \
             src/wdialogsettings.cpp \
             external/audioengine/audioengine.cpp \
             external/FFTwrapper.cpp

HEADERS   += src/wmainwindow.h \
             src/filetype.h \
             src/ftsound.h \
             src/ftfzero.h \
             src/ftlabels.h \
             src/gvwaveform.h \
             src/gvspectrum.h \
             src/wdialogsettings.h \
             external/audioengine/audioengine.h \
             external/FFTwrapper.h

FORMS     += src/wmainwindow.ui \
             src/wdialogsettings.ui

RESOURCES += ressources.qrc
