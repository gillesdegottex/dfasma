# Project created by QtCreator
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
#  options need to link with external libraries)

# For the Discrete Fast Fourier Transform
# Chose among: fft_fftw3, fft_builtin_fftreal
CONFIG += fft_fftw3

# For the audio file support
# Chose among: file_audio_libsndfile, file_audio_libsox, file_audio_builtin
CONFIG += file_audio_libsndfile

# Additional file format support
# SDIF (sources at: http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/)
CONFIG += file_sdif
#CONFIG += file_sdif_static

# Activate this line for logging some information into a txt file
#CONFIG += debug_logfile

# ------------------------------------------------------------------------------
# (modify the following at your own risks !) -----------------------------------

message(CONFIG=$$CONFIG)

# Generate the version number from git
# (if fail, fall back on the version present in the README.txt file)
DFASMAVERSIONGITPRO = $$system(git describe --tags --always)
DFASMABRANCHGITPRO = $$system(git rev-parse --abbrev-ref HEAD)
message(Git: DFasma version: $$DFASMAVERSIONGITPRO Branch: $$DFASMABRANCHGITPRO)
DEFINES += DFASMAVERSIONGIT=$$system(git describe --tags --always)
DEFINES += DFASMABRANCHGIT=$$system(git rev-parse --abbrev-ref HEAD)

# To place the application's files in the proper folder
# To place the shortcut in the proper folder
isEmpty(PREFIXSHORTCUT){
    isEmpty(PREFIX){
            PREFIXSHORTCUT = /usr/local
    }
    else {
        PREFIXSHORTCUT = $$PREFIX
    }
}
isEmpty(PREFIX){
    PREFIX = /usr/local
}
unix:DEFINES += PREFIX=$$PREFIX
message(PREFIX=$$PREFIX)
message(PREFIXSHORTCUT=$$PREFIXSHORTCUT)

# Manage Architecture
win32:message(OS: Windows)
unix:message(OS: UNIX)
COMPILERMSG=""
msvc:COMPILERMSG+="MSVC"
gcc:COMPILERMSG+="GCC"
contains(QT_ARCH, i386):COMPILERMSG+=" for 32bits"
contains(QT_ARCH, x86_64):COMPILERMSG+=" for 64bits"
message(Compiler: $$COMPILERMSG)
message(Qt: Version: $$QT_VERSION)


CONFIG(debug_logfile) {
    message(Information will be dropped in a log file)
    DEFINES += DEBUG_LOGFILE
} else {
    release: DEFINES += QT_NO_WARNING_OUTPUT QT_NO_DEBUG_OUTPUT
}

# Audio file reading libraries -------------------------------------------------

CONFIG(file_audio_builtin, file_audio_libsndfile|file_audio_libsox|file_audio_builtin|file_audio_qt|file_audio_libav) {
    message(Audio file reader: standalone minimal built-in)
    QMAKE_CXXFLAGS += -Dfile_audio_BUILTIN
    HEADERS  += external/wavfile/wavfile.h
    SOURCES  += external/wavfile/wavfile.cpp external/iodsound_load_builtin.cpp
}
CONFIG(file_audio_qt, file_audio_libsndfile|file_audio_libsox|file_audio_builtin|file_audio_qt|file_audio_libav) {
    message(Audio file reader: standalone built-in Qt)
    QMAKE_CXXFLAGS += -Dfile_audio_QT
    HEADERS  += external/iodsound_load_qt.h
    SOURCES  += external/iodsound_load_qt.cpp
}
CONFIG(file_audio_libsndfile, file_audio_libsndfile|file_audio_libsox|file_audio_builtin|file_audio_qt|file_audio_libav) {
    message(Audio file reader: libsndfile)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBSNDFILE
    SOURCES += external/iodsound_load_libsndfile.cpp
    win32 {
        isEmpty(FILE_AUDIO_LIBDIR) {
            contains(QMAKE_TARGET.arch, x86_64) {
                FILE_AUDIO_LIBDIR = "$$_PRO_FILE_PWD_/../lib/libsndfile-1.0.25-w64"
            } else {
                FILE_AUDIO_LIBDIR = "$$_PRO_FILE_PWD_/../lib/libsndfile-1.0.25-w32"
            }
        }
        msvc: LIBS += "$$FILE_AUDIO_LIBDIR/lib/libsndfile-1.lib"
        gcc: LIBS += -L$$FILE_AUDIO_LIBDIR/lib -L$$FILE_AUDIO_LIBDIR/bin -lsndfile-1
    }
    unix:LIBS += -lsndfile
    !isEmpty(FILE_AUDIO_LIBDIR){
        message(FILE_AUDIO_LIBDIR=$$FILE_AUDIO_LIBDIR)
        INCLUDEPATH += $$FILE_AUDIO_LIBDIR/include
        LIBS += -L$$FILE_AUDIO_LIBDIR/lib
    }
}
CONFIG(file_audio_libsox, file_audio_libsndfile|file_audio_libsox|file_audio_builtin|file_audio_qt|file_audio_libav) {
    message(Audio file reader: libsox)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBSOX
    SOURCES  += external/iodsound_load_libsox.cpp
    win32:INCLUDEPATH += "$$_PRO_FILE_PWD_/../libsox-14.4.0-32b/include/"
    unix:LIBS += -lsox
    win32:LIBS += "$$_PRO_FILE_PWD_/../libsox-14.4.0-32b/lib/libsox.dll.a"
}
CONFIG(file_audio_libav, file_audio_libsndfile|file_audio_libsox|file_audio_builtin|file_audio_qt|file_audio_libav) {
    message(Audio file reader: libav)
    QMAKE_CXXFLAGS += -Dfile_audio_LIBAV
    SOURCES += external/iodsound_load_libav.cpp
    LIBS += -lavformat -lavcodec -lavutil
}

# FFT Implementation libraries ----------------------------------------------------
# For FFTW3: Allow to limit the time spent in the resize of the FFT
# (available only from FFTW3's version 3.1)
# This can be useful when using the flag FFTW_MEASURE for plan construction
# because the resize can take quite a lot of time in this case.
# However, this flag is likely to generate non-deterministic runs.
# Thus, I prefer to use the flag FFTW_ESTIMATE.
# Plan construction is actually very fast with FFTW_ESTIMATE so that it
# is not really necessary to limit the plan construction time.
#DEFINES += FFTW3RESIZINGMAXTIMESPENT

CONFIG(fft_fftw3, fft_fftw3|fft_builtin_fftreal){
    message(FFT Implementation: FFTW3)
    QMAKE_CXXFLAGS += -DFFT_FFTW3
    win32 {
        isEmpty(FFT_LIBDIR) {
            contains(QMAKE_TARGET.arch, x86_64) {
                FFT_LIBDIR = "$$_PRO_FILE_PWD_/../lib/fftw-3.3.4-dll64"
            } else {
                FFT_LIBDIR = "$$_PRO_FILE_PWD_/../lib/fftw-3.3.4-dll32"
            }
        }
        !isEmpty(FFT_LIBDIR){
            message(FFT_LIBDIR=$$FFT_LIBDIR)
            INCLUDEPATH += $$FFT_LIBDIR
            LIBS += -L$$FFT_LIBDIR
        }
        msvc: LIBS += $$FFT_LIBDIR/libfftw3-3.lib
        gcc: LIBS += -lfftw3-3
        # msvc: LIBS += $$FFT_LIBDIR/libfftw3f-3.lib
        # gcc: LIBS += -lfftw3f-3
    }
    unix {
        !isEmpty(FFT_LIBDIR){
            message(FFT_LIBDIR=$$FFT_LIBDIR)
            INCLUDEPATH += $$FFT_LIBDIR/include
            LIBS += -L$$FFT_LIBDIR/lib
        }
        LIBS += -lfftw3
        # LIBS += -lfftw3f
    }
}
CONFIG(fft_builtin_fftreal, fft_fftw3|fft_builtin_fftreal){
    message(FFT Implementation: standalone built-in FFTReal)
    QMAKE_CXXFLAGS += -DFFT_FFTREAL
    HEADERS +=  external/liqaudioextra/external/FFTReal/FFTReal.h \
                external/liqaudioextra/external/FFTReal/FFTReal.hpp \
                external/liqaudioextra/external/FFTReal/def.h \
                external/liqaudioextra/external/FFTReal/DynArray.h \
                external/liqaudioextra/external/FFTReal/DynArray.hpp \
                external/liqaudioextra/external/FFTReal/OscSinCos.h \
                external/liqaudioextra/external/FFTReal/OscSinCos.hpp
}

# SDIF file library ------------------------------------------------------------

CONFIG(file_sdif) {
    message(Files: SDIF support: YES)
    DEFINES += SUPPORT_SDIF

    isEmpty(FILE_SDIF_LIBDIR) {
        FILE_SDIF_LIBDIR = "$$_PRO_FILE_PWD_/external/sdif/easdif"
    }

    CONFIG(file_sdif_static) {
        message("    "SDIF linked statically)
        FILE_SDIF_LINKTYPE = "_static"
        DEFINES += SUPPORT_SDIF_STATIC
    }
    else{
        message("    "SDIF linked dynamically)
    }

    LIBS += -lEasdif$$FILE_SDIF_LINKTYPE
    !isEmpty(FILE_SDIF_LIBDIR){
        message("    "FILE_SDIF_LIBDIR=$$FILE_SDIF_LIBDIR)
        INCLUDEPATH += $$FILE_SDIF_LIBDIR/include
        LIBS += -L$$FILE_SDIF_LIBDIR/lib
    }
}
else {
    message(Files: SDIF support: NO)
}

# Common configurations --------------------------------------------------------

QT += core gui multimedia opengl svg
QT -= network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

TARGET = dfasma
TEMPLATE = app
RC_ICONS = icons/dfasma.ico
RESOURCES += ressources.qrc

win32: msvc: CONFIG += embed_manifest_exe

FORMS     += src/wmainwindow.ui \
             src/wdialogselectchannel.ui \
             src/wdialogfilecreate.ui \
             src/wdialogsettings.ui \
             src/gvspectrumamplitudewdialogsettings.ui \
             src/gvspectrogramwdialogsettings.ui \
             src/aboutbox.ui \
             src/wgenerictimevalue.ui \
             src/wdialogfiletypechoosertxt.ui

INCLUDEPATH += external/REAPER
INCLUDEPATH += external/libqaudioextra/include

SOURCES   += src/main.cpp\
             src/wmainwindow.cpp \
             src/wdialogsettings.cpp \
             src/wfileslist.cpp \
             src/aboutbox.cpp \
             src/wdialogfilecreate.cpp \
             src/filetype.cpp \
             src/ftsound.cpp \
             src/wdialogselectchannel.cpp \
             src/ftfzero.cpp \
             src/ftlabels.cpp \
             src/gvwaveform.cpp \
             src/gvspectrumamplitude.cpp \
             src/fftresizethread.cpp \
             src/gvspectrumamplitudewdialogsettings.cpp \
             src/gvspectrumphase.cpp \
             src/gvspectrumgroupdelay.cpp \
             src/gvspectrogram.cpp \
             src/stftcomputethread.cpp \
             src/gvspectrogramwdialogsettings.cpp \
             src/ftgenerictimevalue.cpp \
             src/gvgenerictimevalue.cpp \
             src/wgenerictimevalue.cpp \
             src/wdialogfiletypechoosertxt.cpp \
             external/libqxt/qxtspanslider.cpp \
             external/audioengine/audioengine.cpp \
             external/libqaudioextra/src/qaemath.cpp \
             external/libqaudioextra/src/qaesigproc.cpp \
             external/libqaudioextra/src/qaecolormap.cpp \
             external/libqaudioextra/src/qaesettingsauto.cpp \
             external/libqaudioextra/src/qaegigrid.cpp \
             external/libqaudioextra/src/qaegiuniformlysampledsignal.cpp \
             external/libqaudioextra/src/qaegisampledsignal.cpp \
             external/libqaudioextra/external/mkfilter/mkfilter.cpp \
             external/REAPER/epoch_tracker/epoch_tracker.cc \
             external/REAPER/epoch_tracker/fft.cc \
             external/REAPER/epoch_tracker/fd_filter.cc \
             external/REAPER/epoch_tracker/lpc_analyzer.cc

HEADERS   += src/wmainwindow.h \
             src/wdialogsettings.h \
             src/wfileslist.h \
             src/aboutbox.h \
             src/wdialogfilecreate.h \
             src/filetype.h \
             src/ftsound.h \
             src/wdialogselectchannel.h \
             src/ftfzero.h \
             src/ftlabels.h \
             src/gvwaveform.h \
             src/gvspectrumamplitude.h \
             src/fftresizethread.h \
             src/gvspectrumamplitudewdialogsettings.h \
             src/gvspectrumphase.h \
             src/gvspectrumgroupdelay.h \
             src/gvspectrogram.h \
             src/stftcomputethread.h \
             src/gvspectrogramwdialogsettings.h \
             src/ftgenerictimevalue.h \
             src/gvgenerictimevalue.h \
             src/wgenerictimevalue.h \
             src/wdialogfiletypechoosertxt.h \
             external/libqxt/qxtglobal.h \
             external/libqxt/qxtnamespace.h \
             external/libqxt/qxtspanslider.h \
             external/libqxt/qxtspanslider_p.h \
             external/audioengine/audioengine.h \
             external/libqaudioextra/include/qaemath.h \
             external/libqaudioextra/include/qaesigproc.h \
             external/libqaudioextra/include/qaecolormap.h \
             external/libqaudioextra/include/qaesettingsauto.h \
             external/libqaudioextra/include/qaegigrid.h \
             external/libqaudioextra/include/qaegiuniformlysampledsignal.h \
             external/libqaudioextra/include/qaegisampledsignal.h \
             external/libqaudioextra/external/mkfilter/mkfilter.h \
             external/REAPER/epoch_tracker/epoch_tracker.h \
             external/REAPER/epoch_tracker/fft.h \
             external/REAPER/epoch_tracker/fd_filter.h \
             external/REAPER/epoch_tracker/lpc_analyzer.h


# Installation configurations --------------------------------------------------
target.path = $$PREFIX/bin
shortcut.path = $$PREFIXSHORTCUT/share/applications
shortcut.files = distrib/dfasma.desktop
iconsvg.path = $$PREFIX/share/icons/hicolor/scalable/apps
iconsvg.files = icons/dfasma.svg
iconpng.path = $$PREFIX/share/icons/hicolor/128x128/apps
iconpng.files = icons/dfasma.png
appdata.path = $$PREFIX/share/appdata
appdata.files = distrib/dfasma.appdata.xml
#translations.path = $$PREFIX/share/fmit/tr
#translations.files = tr/*
INSTALLS += target shortcut iconsvg iconpng appdata
