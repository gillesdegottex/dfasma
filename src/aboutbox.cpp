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

#include "aboutbox.h"
#include "ui_aboutbox.h"

#include <QFile>
#include <QTextStream>

#include "wmainwindow.h"
#include "qaehelpers.h"
#include "qaesigproc.h"
#include "ftsound.h"

#ifdef SUPPORT_SDIF
    #include <easdif/easdif.h>
#endif

extern QString DFasmaVersion();

AboutBox::AboutBox(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutBox)
{
    ui->setupUi(this);

    QString fullversion = "Version "+DFasmaVersion();
    fullversion += "\nCompiled by "+getCompilerVersion()+" for ";
    #ifdef Q_PROCESSOR_X86_32
      fullversion += "32bits";
    #endif
    #ifdef Q_PROCESSOR_X86_64
      fullversion += "64bits";
    #endif
    ui->lblVersion->setText(fullversion);

    QString txt = "";

    txt += "<h4>Purpose</h4>";
    txt += "<i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains. "; //  Even though there are a few scaling functionalities, DFasma is basically not an audio editor
    txt += "Its design is inspired by the <i>Xspect</i> software which was developed at <a href='http://www.ircam.fr'>Ircam</a>.</p>";
    // <a href='http://recherche.ircam.fr/equipes/analyse-synthese/DOCUMENTATIONS/xspect/xsintro1.2.html'>Xspect software</a>

    txt += "<p>To suggest a new functionality or report a bug, do not hesitate to <a href='https://github.com/gillesdegottex/dfasma/issues'>raise an issue on GitHub.</a></p>";

    txt += "<h4>Legal</h4>\
            The core of this software is under the <a href=\"http://www.gnu.org/licenses/gpl.html\">GPL (v3) License</a>.\
            All original source files of any kind (code source and any ressources) are under the same license, except\
            the content of the \"external\" directory of the source code which contains imported source code under various licenses.\
            <br/>\
            The icons are under the <a href=\"http://creativecommons.org/licenses/by/3.0/us/\">Creative Commons License</a>.\
            <br/><br/>\
            Copyright &copy; 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a><br/>\
            <i>DFasma</i> is coded in C++/<a href='http://qt-project.org'>Qt</a>.\
            The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.";

    txt += "<h4>Disclaimer</h4>\
            ALL THE FUNCTIONALITIES OF <I>DFASMA</I> AND ITS CODE ARE PROVIDED WITHOUT ANY WARRANTY \
            (E.G. THERE IS NO WARRANTY OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE). \
            ALSO, THE COPYRIGHT HOLDERS AND CONTRIBUTORS DO NOT TAKE ANY LEGAL RESPONSIBILITY \
            REGARDING THE IMPLEMENTATIONS OF THE PROCESSING TECHNIQUES OR ALGORITHMS \
            (E.G. CONSEQUENCES OF BUGS OR ERRONEOUS IMPLEMENTATIONS).<br/> \
            Please see the README.txt file for additional information.";

    ui->txtAbout->setText(txt);


    // Libraries

    // FFT
    QString fftinfostr = "";
    fftinfostr += "<i>Fast Fourier Transform (FFT):</i> "+qae::FFTwrapper::getLibraryInfo();
    fftinfostr += " ("+QString::number(sizeof(FFTTYPE)*8)+"bits(";
    if(sizeof(FFTTYPE)==4)  fftinfostr += "single";
    if(sizeof(FFTTYPE)==8)  fftinfostr += "double";
    if(sizeof(FFTTYPE)==16)  fftinfostr += "quadruple";
    fftinfostr += "); smallest: "+QString::number(20*log10(std::numeric_limits<FFTTYPE>::min()))+"dB)";
    ui->vlLibraries->addWidget(new QLabel(fftinfostr, this));

    // SDIF
    QString sdifinfostr = "";
    #ifdef SUPPORT_SDIF
        sdifinfostr = "<i>For SDIF file format:</i> <a href=\"http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/\">Easdif</a> version "+QString(EASDIF_VERSION_STRING);
        #ifdef SUPPORT_SDIF_STATIC
            sdifinfostr += " [static link]";
        #else
            sdifinfostr += " [dynamic link]";
        #endif
    #else
        sdifinfostr = "<i>No support for SDIF file format</i>";
    #endif
    ui->vlLibraries->addWidget(new QLabel(sdifinfostr, this));

    QString fileaudiotxt = "<i>For reading Audio files:</i> "+FTSound::getAudioFileReadingDescription();
    #ifdef file_audio_static
        fileaudiotxt += " [static link]";
    #else
        fileaudiotxt += " [dynamic link]";
    #endif
    ui->vlLibraries->addWidget(new QLabel(fileaudiotxt, this));
    QStringList list = FTSound::getAudioFileReadingSupportedFormats();
    for(QStringList::Iterator it=list.begin(); it!=list.end(); ++it)
        ui->listSupportedFormats->addItem(*it);

    //    QMessageBox::aboutQt(this, "About this software");
}

AboutBox::~AboutBox()
{
    delete ui;
}
