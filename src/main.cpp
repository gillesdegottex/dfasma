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
#include <QIcon>
#include <QTextStream>
#include <QFileDialog>
#include <QString>
#include <QCommandLineParser>

#ifdef FILE_AUDIO_LIBAV
extern "C" {
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#endif
#ifdef FILE_AUDIO_LIBSOX
extern "C" {
#include <sox.h>
#include <assert.h>
}
#endif

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
#endif

#include <iostream>

#include "qaehelpers.h"

#ifdef DEBUG_LOGFILE
static QString g_debug_stream;
void DFasmaMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg){
    Q_UNUSED(type)
    Q_UNUSED(context)
    std::cout << "LOGGED: " << msg.toLatin1().constData() << std::endl;
    QTextStream(&g_debug_stream) << msg.toLatin1().constData() << endl;
}
#endif

QString g_version;
QString DFasmaVersion(){
    if(!g_version.isEmpty())
        return g_version;

    QString dfasmaversiongit(STR(DFASMAVERSIONGIT));
    QString dfasmabranchgit(STR(DFASMABRANCHGIT));

    QString	dfasmaversion;
    if(!dfasmaversiongit.isEmpty()) {
        dfasmaversion = dfasmaversiongit;
        if(dfasmabranchgit!="master")
            dfasmaversion += "-" + dfasmabranchgit;
    }
    else {
        QFile readmefile(":/README.txt");
        readmefile.open(QFile::ReadOnly | QFile::Text);
        QTextStream readmefilestream(&readmefile);
        readmefilestream.readLine();
        readmefilestream.readLine();
        dfasmaversion = readmefilestream.readLine().simplified();
        dfasmaversion = dfasmaversion.mid(8);
    }
    g_version = dfasmaversion;

    return g_version;
}

int main(int argc, char *argv[])
{
    #ifdef DEBUG_LOGFILE
    qInstallMessageHandler(DFasmaMessageHandler);
    #endif

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(true);

    // The following is also necessary for QSettings
    QCoreApplication::setOrganizationName("DFasma");
    QCoreApplication::setOrganizationDomain("gillesdegottex.eu");
    QCoreApplication::setApplicationName("DFasma");
    QCoreApplication::setApplicationVersion(DFasmaVersion());
    app.setWindowIcon(QIcon(":/icons/dfasma.svg"));

    QCommandLineParser parser;
    parser.setApplicationDescription("DFasma: A tool to analyse and compare audio files in time and frequency");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Files to load", "[files...]");
    QCommandLineOption gentimevalueOption(QStringList() << "g" << "gentimevalue", "Load the <file> as generic time/value", "file");
    parser.addOption(gentimevalueOption);

    parser.process(app); // Process the actual command line arguments
    QStringList filestoload = parser.positionalArguments();
    QStringList genfilestoload = parser.values("g");


    // Initialize some external libraries
    #ifdef FILE_AUDIO_LIBAV
        // This call is necessarily done once in your app to initialize
        // libavformat to register all the muxers, demuxers and protocols.
        av_register_all();
        // TODO av_unregister ?
    #endif
    #ifdef FILE_AUDIO_LIBSOX
        assert(sox_init() == SOX_SUCCESS);
    #endif

    #ifdef SUPPORT_SDIF
        Easdif::EasdifInit();
    #endif

    // Create the main window and run it
    WMainWindow* w = new WMainWindow(filestoload, genfilestoload);
    QObject::connect(&app, SIGNAL(focusWindowChanged(QWindow*)), w, SLOT(focusWindowChanged(QWindow*)));
    w->show();

    app.exec();
    DFLAG

    // The user requested to exit the application

    delete w;

    DFLAG

    // Unload some external libraries
    #ifdef SUPPORT_SDIF
        Easdif::EasdifEnd();
    #endif
    #ifdef FILE_AUDIO_LIBSOX
        sox_quit();
    #endif   

    DFLAG
    // If asked, drop some log information in a file
    #ifdef DEBUG_LOGFILE
        QString logfilename = QFileDialog::getSaveFileName(NULL, "Save log file as...");
        QFile logfile(logfilename);
        logfile.open(QIODevice::WriteOnly);
        QTextStream out(&logfile);
        std::cout << g_debug_stream.size() << std::endl;
        out << g_debug_stream;
        logfile.close();
    #endif

    DFLAG
    QCoreApplication::exit(0);
    DFLAG
    QCoreApplication::processEvents(); // Process all events before exit
//    DCOUT << "exit(" << ret << ")" << std::endl;
    DFLAG
    exit(0); // WORKAROUND?: need this to avoid remaining background process on some platform (e.g. bouzouki) TODO This is surely related to some seg fault on exit #179
    DFLAG

    return 0;
}
