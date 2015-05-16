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

# Compilation options ----------------------------------------------------------
# (except for fft_fftreal and file_audio_builtin, all the other
#  options request linking with external libraries)

# For the Discrete Fast Fourier Transform
# Chose among: fft_fftw3, fft_builtin_fftreal
CONFIG += fft_fftw3
# For FFTW3: Allow to limit the time spent in the resize of the FFT
#(available only from FFTW3's version 3.1)
DEFINES += FFTW3RESIZINGMAXTIMESPENT

# For the audio file support
# Chose among: file_audio_libsndfile, file_audio_libsox,
#              file_audio_libav,
#              file_audio_qt, file_audio_builtin
CONFIG += file_audio_libsndfile

# Additional file format support
# SDIF (can be disabled) (sources at: http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/)
CONFIG += file_sdif

## OS specific options
#QMAKE_MAC_SDK = macosx10.6

#CONFIG += precision_float
CONFIG += precision_double

# ------------------------------------------------------------------------------
# (modify the following at your own risks !) -----------------------------------

# Generate the version number from git
# (if fail, fall back on the version present in the README.txt file)
DEFINES += DFASMAVERSIONGIT=$$system(git describe --tags --always)

# Manage Architecture
win32:message(Build for Windows)
unix:message(Build for Linux)
contains(QMAKE_TARGET.arch, x86):message(Build for 32bits)
contains(QMAKE_TARGET.arch, x86_64):message(Build for 64bits)

# Manage Precision
CONFIG(precision_float) {
    DEFINES += SIGPROC_FLOAT
    message(Compile with single precision)
} else {
    message(Compile with double precision)
}

# SDIF file library ------------------------------------------------------------

CONFIG(file_sdif) {
    message(Building with SDIF file reader.)
    DEFINES += SUPPORT_SDIF
    QMAKE_CXXFLAGS  += -I/u/anasynth/degottex/.local/include/easdif/
    #QMAKE_CXXFLAGS  += -I/u/formes/share/include
    #LIBS += /usr/local/lib/libEasdif_static.a
    LIBS += -L/u/anasynth/degottex/.local/lib/x86_64-Linux-rh65/
    LIBS += -lEasdif
}

# Audio file reading libraries -------------------------------------------------

CONFIG(file_audio_builtin) {
    message(Building with minimal built-in audio file reader.)
    QMAKE_CXXFLAGS += -Dfile_audio_BUILTIN
    HEADERS  += external/wavfile/wavfile.h
    SOURCES  += external/wavfile/wavfile.cpp external/iodsound_load_builtin.cpp
}
CONFIG(file_audio_qt) {
    message(Building with Qt support for audio file reading.)
    QMAKE_CXXFLAGS += -Dfile_audio_QT
    HEADERS  += external/iodsound_load_qt.h
    SOURCES  += external/iodsound_load_qt.cpp
}
CONFIG(file_audio_libsndfile) {
    message(Building with libsndfile support for audio file reading.)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBSNDFILE
    SOURCES  += external/iodsound_load_libsndfile.cpp
    win32 {
        contains(QMAKE_TARGET.arch, x86_64) {
            INCLUDEPATH += "$$_PRO_FILE_PWD_/../libsndfile-1.0.25-w64/include/"
            LIBS += "$$_PRO_FILE_PWD_/../libsndfile-1.0.25-w64/lib/libsndfile-1.lib"
        } else {
            INCLUDEPATH += "$$_PRO_FILE_PWD_/../libsndfile-1.0.25-w32/include/"
            LIBS += "$$_PRO_FILE_PWD_/../libsndfile-1.0.25-w32/lib/libsndfile-1.lib"
        }
    }
    unix:LIBS += -lsndfile
}
CONFIG(file_audio_libsox) {
    message(Building with libsox support for audio file reading.)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBSOX
    SOURCES  += external/iodsound_load_libsox.cpp
    win32:INCLUDEPATH += "$$_PRO_FILE_PWD_/../libsox-14.4.0-32b/include/"
    unix:LIBS += -lsox
    win32:LIBS += "$$_PRO_FILE_PWD_/../libsox-14.4.0-32b/lib/libsox.dll.a"
}
CONFIG(file_audio_libav) {
    message(Building with libav support for audio file reading.)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBAV
    SOURCES += external/iodsound_load_libav.cpp
    LIBS += -lavformat -lavcodec -lavutil
}

# DFT computation libraries ----------------------------------------------------

CONFIG(fft_fftw3){
    message(Building with FFTW3.)
    QMAKE_CXXFLAGS += -DFFT_FFTW3
    win32 {
        contains(QMAKE_TARGET.arch, x86_64) {
            INCLUDEPATH += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll64/"
            LIBS += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll64/libfftw3-3.lib"
        } else {
            INCLUDEPATH += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll32/"
            LIBS += "$$_PRO_FILE_PWD_/../fftw-3.3.4-dll32/libfftw3-3.lib"
        }
    }
    CONFIG(precision_double) {
        unix:LIBS += -lfftw3
    }
    CONFIG(precision_float) {
        unix:LIBS += -lfftw3f
    }
}
CONFIG(fft_builtin_fftreal){
    message(Building with built-in FFTReal.)
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
QT -= network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

TARGET = dfasma
TEMPLATE = app

RC_ICONS = icons/dfasma.ico

SOURCES   += src/main.cpp\
             external/libqxt/qxtspanslider.cpp \
             src/wmainwindow.cpp \
             src/wdialogselectchannel.cpp \
             src/filetype.cpp \
             src/ftsound.cpp \
             src/ftfzero.cpp \
             src/ftlabels.cpp \
             src/gvwaveform.cpp \
             src/gvamplitudespectrum.cpp \
             src/gvamplitudespectrumwdialogsettings.cpp \
             src/fftresizethread.cpp \
             src/gvphasespectrum.cpp \
             src/wdialogsettings.cpp \
             src/gvspectrogram.cpp \
             src/stftcomputethread.cpp \
             src/gvspectrogramwdialogsettings.cpp \
             src/sigproc.cpp \
             external/mkfilter/mkfilter.cpp \
             external/audioengine/audioengine.cpp \
             src/colormap.cpp \
             src/QSettingsAuto.cpp \
             src/aboutbox.cpp

HEADERS   += src/wmainwindow.h \
             external/libqxt/qxtglobal.h \
             external/libqxt/qxtnamespace.h \
             external/libqxt/qxtspanslider.h \
             external/libqxt/qxtspanslider_p.h \
             src/wdialogselectchannel.h \
             src/sigproc.h \
             external/mkfilter/mkfilter.h \
             src/filetype.h \
             src/ftsound.h \
             src/ftfzero.h \
             src/ftlabels.h \
             src/gvwaveform.h \
             src/gvamplitudespectrum.h \
             src/gvamplitudespectrumwdialogsettings.h \
             src/gvphasespectrum.h \
             src/wdialogsettings.h \
             src/gvspectrogram.h \
             src/stftcomputethread.h \
             src/gvspectrogramwdialogsettings.h \
             src/fftresizethread.h \
             external/audioengine/audioengine.h \
             src/colormap.h \
             src/QSettingsAuto.h \
             src/aboutbox.h

FORMS     += src/wmainwindow.ui \
             src/wdialogselectchannel.ui \
             src/wdialogsettings.ui \
             src/gvamplitudespectrumwdialogsettings.ui \
             src/gvspectrogramwdialogsettings.ui \
             src/aboutbox.ui

RESOURCES += ressources.qrc
