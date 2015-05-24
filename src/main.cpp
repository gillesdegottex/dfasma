/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "wmainwindow.h"
#include <QApplication>
#include <QObject>

#ifdef AUDIOFILEREADING_LIBAV
extern "C" {
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#endif
#ifdef AUDIOFILEREADING_LIBSOX
extern "C" {
#include <sox.h>
#include <assert.h>
}
#endif

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
#endif

#include <iostream>

#include "qthelper.h"
#include "filetype.h"

int main(int argc, char *argv[])
{
    // Set all static variables
    // Map FileTypes with corresponding strings
    if(FileType::m_typestrings.empty()){
        FileType::m_typestrings.push_back("All files (*.*)");
        FileType::m_typestrings.push_back("Sound (*.wav *.aiff *.pcm *.snd *.flac *.ogg)");
        FileType::m_typestrings.push_back("F0 (*.bpf *.sdif)");
        FileType::m_typestrings.push_back("Label (*.bpf *.lab *.sdif)");
    }

    #ifdef AUDIOFILEREADING_LIBAV
        // This call is necessarily done once in your app to initialize
        // libavformat to register all the muxers, demuxers and protocols.
        av_register_all();
        // TODO av_unregister ?
    #endif
    #ifdef AUDIOFILEREADING_LIBSOX
        assert(sox_init() == SOX_SUCCESS);
        // TODO sox_quit();
    #endif

    #ifdef SUPPORT_SDIF
        Easdif::EasdifInit();
        // TODO EasdifEnd();
    #endif

    QApplication a(argc, argv);

    // The following is also necessary for QSettings
    QCoreApplication::setOrganizationName("DFasma");
    QCoreApplication::setOrganizationDomain("gillesdegottex.eu");
    QCoreApplication::setApplicationName("DFasma");

    a.setWindowIcon(QIcon(":/icons/dfasma.svg"));

    QStringList filestoload = QApplication::arguments();
    filestoload.removeAt(0);

//    WMainWindow w(filestoload);
    WMainWindow* w = new WMainWindow(filestoload);
    QObject::connect(&a, SIGNAL(focusWindowChanged(QWindow*)), w, SLOT(checkFileModifications()));
    w->show();

    int ret = a.exec();

    #ifdef SUPPORT_SDIF
//        std::cout << __LINE__ << std::endl;
        FileType::cleanupAndEEnd();
        Easdif::EasdifEnd();
    #endif

    delete w; // TODO BUG The crash at the end seems to be systematic when using this

    exit(ret); // WORKAROUND?: won't quit otherwise on some platform (e.g. bouzouki) TODO

    return ret;
}
