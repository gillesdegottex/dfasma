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

# ------------------------------------------------------------------------------
# Compilation options
# (except for fft_fftreal and audiofilereading_builtin, all the other
#  options request linking with external libraries)

# For the Discrete Fast Fourier Transform
# Chose among: fft_fftw3, fft_fftreal
CONFIG += fft_fftw3
# For FFTW3: Allow to limit the time spent in the resize of the FFT
#(available only from FFTW3's version 3.1)
DEFINES += FFTW3RESIZINGMAXTIMESPENT

# For the audio file support
# Chose among: audiofilereading_libsndfile, audiofilereading_libsox,
#              audiofilereading_libav,
#              audiofilereading_qt, audiofilereading_builtin
CONFIG += audiofilereading_builtin

# Additional file format support
# SDIF (can be disabled) (sources at: http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/)
# CONFIG += sdifreading

# The most cross-platform/portable/compatible compilation settings
# (comment all the above and uncomment below)
#CONFIG += fft_fftreal
#CONFIG += audiofilereading_builtin


# OS specific options
QMAKE_MAC_SDK = macosx10.6

# (modify the following at your own risks !) -----------------------------------

# SDIF file library ------------------------------------------------------------

CONFIG(sdifreading) {
    message(Building with SDIF file reader.)
    DEFINES += SUPPORT_SDIF
    #LIBS += /usr/local/lib/libEasdif_static.a
    LIBS += -lEasdif
    #QMAKE_CXXFLAGS  += -I/u/formes/share/include
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
CONFIG(audiofilereading_libsox) {
    message(Building with libsox support for audio file reading.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBSOX
    SOURCES  += external/iodsound_load_libsox.cpp
    LIBS += -lsox
}
CONFIG(audiofilereading_libav) {
    message(Building with libav support for audio file reading.)
    QMAKE_CXXFLAGS += -DAUDIOFILEREADING_LIBAV
    SOURCES += external/iodsound_load_libav.cpp
    LIBS += -lavformat -lavcodec -lavutil
}

# DFT computation libraries ----------------------------------------------------

CONFIG(fft_fftw3){
    QMAKE_CXXFLAGS += -DFFT_FFTW3
    win32:INCLUDEPATH += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll32/"
    unix:LIBS += -lfftw3
    win32:LIBS += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll32/libfftw3-3.dll"
}
CONFIG(fft_fftreal){
    QMAKE_CXXFLAGS += -DFFT_FFTREAL
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
             external/mkfilter/mkfilter.cpp \
             src/ftfzero.cpp \
             src/ftlabels.cpp \
             src/gvwaveform.cpp \
             src/gvamplitudespectrum.cpp \
             src/gvamplitudespectrumwdialogsettings.cpp \
             src/gvphasespectrum.cpp \
             src/wdialogsettings.cpp \
             external/audioengine/audioengine.cpp \
             external/FFTwrapper.cpp

HEADERS   += src/wmainwindow.h \
             src/sigproc.h \
             src/filetype.h \
             src/ftsound.h \
             src/ftfzero.h \
             src/ftlabels.h \
             src/gvwaveform.h \
             src/gvamplitudespectrum.h \
             src/gvamplitudespectrumwdialogsettings.h \
             src/gvphasespectrum.h \
             src/wdialogsettings.h \
             external/audioengine/audioengine.h \
             external/FFTwrapper.h

FORMS     += src/wmainwindow.ui \
             src/wdialogsettings.ui \
             src/gvamplitudespectrumwdialogsettings.ui

RESOURCES += ressources.qrc
