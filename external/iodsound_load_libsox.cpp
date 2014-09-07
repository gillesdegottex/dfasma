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

/*
 * Inpired from libsox: src/example2.c
*/

#include "../src/ftsound.h"

#include <iostream>
using namespace std;

extern "C" {
#include <stdio.h>
#include <assert.h>
#include <sox.h>
}

#define BUFFER_LEN      1024

QString FTSound::getAudioFileReadingDescription(){

    QString txt = QString("<p>Using <a href='http://sox.sourceforge.net'>libsox</a>");

    // Add version information
//    sox_version_info_t const * soxinfo = sox_version_info();
    txt += " - version " + QString(sox_version());
    txt += "</p>";

//    sox_encodings_info_t const * encinfo = sox_get_encodings_info();
//    txt += "<p><b>Supported formats</b><br/>";
//    txt += QString(encinfo->name)+"(."+encinfo->desc+")<br/>";

//    txt += "</p>";

    return txt;
}


void FTSound::load(const QString& _fileName){

    // Load audio file
    fileFullPath = _fileName;

    m_fileaudioformat = QAudioFormat(); // Clear the format

    sox_format_t* in; // input and output files
    sox_sample_t* buf;
    size_t readcount;

    // Open the input file (with default parameters)
    assert(in = sox_open_read(fileFullPath.toLocal8Bit().constData(), NULL, NULL, NULL));

    if(in->signal.channels>1)
        throw QString("libsox: This audio file has multiple audio channel, whereas DFasma is not designed for this. Please convert this file into a mono audio file before re-opening it with DFasma.");

    m_fileaudioformat.setChannelCount(1);

    setSamplingRate(in->signal.rate);

    m_fileaudioformat.setSampleRate(in->signal.rate);
    m_fileaudioformat.setSampleSize(in->encoding.bits_per_sample);
    // TODO Check with known examples
    if(in->encoding.encoding==SOX_ENCODING_SIGN2)
        m_fileaudioformat.setSampleType(QAudioFormat::SignedInt);
    else if(in->encoding.encoding==SOX_ENCODING_UNSIGNED)
        m_fileaudioformat.setSampleType(QAudioFormat::UnSignedInt);
    else if(in->encoding.encoding==SOX_ENCODING_FLOAT)
        m_fileaudioformat.setSampleType(QAudioFormat::Float);
    m_fileaudioformat.setByteOrder((in->encoding.opposite_endian)?QAudioFormat::LittleEndian:QAudioFormat::BigEndian);
    // TODO Check with known examples

    // Allocate a block of memory to store the block of audio samples:
    buf = (sox_sample_t*)(new char[sizeof(sox_sample_t) * BUFFER_LEN]);

    // Read and process blocks of audio until EOF:
    double sample;
    while((readcount=sox_read(in, buf, BUFFER_LEN))) {

        for(size_t i = 0; i < readcount; ++i) {
            SOX_SAMPLE_LOCALS;
            // convert the sample from SoX's internal format to a `double' for
            // processing in this application:
            sample = SOX_SAMPLE_TO_FLOAT_64BIT(buf[i],);

            wav.push_back(sample);
        }
    }

    // All done; tidy up:
    free(buf);
    sox_close(in);
}
