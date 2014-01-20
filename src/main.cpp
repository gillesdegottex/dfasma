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

#ifdef AUDIOFILEREADING_LIBAV
extern "C" {
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#endif

int main(int argc, char *argv[])
{
    #ifdef AUDIOFILEREADING_LIBAV
        // This call is necessarily done once in your app to initialize
        // libavformat to register all the muxers, demuxers and protocols.
        av_register_all();
    #endif

    QApplication a(argc, argv);

    QStringList sndfiles = QApplication::arguments();
    sndfiles.removeAt(0);

    WMainWindow w(sndfiles);
    w.show();

    return a.exec();
}
