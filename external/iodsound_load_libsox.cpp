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

#include <QFileInfo>

#define BUFFER_LEN      1024

int FTSound::getNumberOfChannels(const QString& filePath){

    if(!QFileInfo::exists(filePath))
        throw QString("The file: ")+filePath+" doesn't seem to exist.";

    sox_format_t* in; // input and output files
    size_t readcount;

    // Open the input file (with default parameters)
    in = sox_open_read(filePath.toLocal8Bit().constData(), NULL, NULL, NULL);

    if(in==NULL)
        throw QString("libsox: Cannot open input file");

    int nbchannels = in->signal.channels;

    sox_close(in);

    return nbchannels;
}


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


void FTSound::load(int channelid){

    m_fileaudioformat = QAudioFormat(); // Clear the format
    m_channelid = channelid;
    bool sumchannels = m_channelid==-2;

    sox_format_t* in; // input and output files
    sox_sample_t* buf;
    size_t readcount;

    // Open the input file (with default parameters)
    in = sox_open_read(fileFullPath.toLocal8Bit().constData(), NULL, NULL, NULL);

    if(in==NULL)
        throw QString("libsox: Cannot open input file");

    if(!sumchannels && m_channelid>in->signal.channels)
        throw QString("libsox: The requested channel ID is higher than the number of channels in the file.");

    m_fileaudioformat.setChannelCount(in->signal.channels);

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
    double sum = 0.0;
    channelid--; // Move indices [1,N] to [0,N-1] to avoid computing -1 to often
    int nbchan = in->signal.channels;
    int curchannelid = 0;
    while((readcount=sox_read(in, buf, BUFFER_LEN))) {

        for(size_t i = 0; i < readcount; ++i) {
            SOX_SAMPLE_LOCALS;
            // convert the sample from SoX's internal format to a `double' for
            // processing in this application:
            sample = SOX_SAMPLE_TO_FLOAT_64BIT(buf[i],);

            if(sumchannels){
                sum += sample;
                if(curchannelid==nbchan-1){
                    wav.push_back(sum/nbchan);
                    sum = 0.0;
                }
            }
            else if(curchannelid==channelid)
                wav.push_back(sample);

            curchannelid = (curchannelid+1)%nbchan;
        }
    }

    // All done; tidy up:
    free(buf);
    sox_close(in);
}
