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

/* Highly inspired by
 * http://www.music.columbia.edu/pipermail/music-dsp/2002-January/047060.html
 */

#include "../src/ftsound.h"

#include <iostream>
using namespace std;

extern "C" {
#include    <stdio.h>
#include    <sndfile.h>
}

QString FTSound::getAudioFileReadingDescription(){

    QString txt = QString("<p>Using <a href='http://www.mega-nerd.com/libsndfile'>libsndfile</a>");

    // Add version information
    char buffer[128];
    sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
    txt += " - version " + QString(buffer);
    txt += "</p>";

    txt += "<p><b>Supported formats</b><br/>";
    SF_FORMAT_INFO	info;
    SF_INFO 		sfinfo;
    int format, major_count, subtype_count, m, s;

    sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof (int));
    sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof (int));

    sfinfo.channels = 1;
    for (m = 0 ; m < major_count ; m++) {
        info.format = m;
        sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
        txt += QString(info.name)+"(."+info.extension+")<br/>";

        format = info.format;

        for (s = 0; s<subtype_count; s++) {
            info.format = s;
            sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info));

            format = (format & SF_FORMAT_TYPEMASK) | info.format;

            sfinfo.format = format;
            if (sf_format_check (&sfinfo))
                txt += QString("*")+info.name+"<br/>";
        }
    }

    txt += "</p>";

    return txt;
}

/* This will be the length of the buffer used to hold samples while
** we process them.
*/
#define BUFFER_LEN      1024

///* libsndfile can handle more than 6 channels but we'll restrict it to 6. */
//#define    MAX_CHANNELS    6

void FTSound::load(const QString& _fileName){

    // Load audio file
    fileFullPath = _fileName;

    m_fileaudioformat = QAudioFormat(); // Clear the format

    /* This is a buffer of double precision floating point values
    ** which will hold our data while we process it.
    */
    static double data [BUFFER_LEN] ;

    /* A SNDFILE is very much like a FILE in the Standard C library. The
    ** sf_open_read and sf_open_write functions return an SNDFILE* pointer
    ** when they sucessfully open the specified file.
    */
    SNDFILE      *infile;

    /* A pointer to an SF_INFO stutct is passed to sf_open_read and sf_open_write
    ** which fill this struct with information about the file.
    */
    SF_INFO      sfinfo ;
    int          readcount ;

    /* Here's where we open the input file. We pass sf_open_read the file name and
    ** a pointer to an SF_INFO struct.
    ** On successful open, sf_open_read returns a SNDFILE* pointer which is used
    ** for all subsequent operations on that file.
    ** If an error occurs during sf_open_read, the function returns a NULL pointer.
    */
    if( !(infile = sf_open(fileFullPath.toLocal8Bit().constData(), SFM_READ, &sfinfo)) ) {
        /* Open failed so print an error message. */
        throw QString("libsndfile: Not able to open input file");
    }

    if(sfinfo.channels>1){
        throw QString("libsndfile: This audio file has multiple audio channel, whereas DFasma is not designed for this. Please convert this file into a mono audio file before re-opening it with DFasma.");
    }

    m_fileaudioformat.setChannelCount(1);

    setSamplingRate(sfinfo.samplerate);

    m_fileaudioformat.setSampleRate(sfinfo.samplerate);

    // TODO Fill the codec name based on:
    //      http://www.mega-nerd.com/libsndfile/api.html

    if((sfinfo.format&0x00FF)==SF_FORMAT_PCM_S8) {
        m_fileaudioformat.setSampleType(QAudioFormat::SignedInt);
        m_fileaudioformat.setSampleSize(8);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_PCM_16) {
        m_fileaudioformat.setSampleType(QAudioFormat::SignedInt);
        m_fileaudioformat.setSampleSize(16);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_PCM_24) {
        m_fileaudioformat.setSampleType(QAudioFormat::SignedInt);
        m_fileaudioformat.setSampleSize(24);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_PCM_32) {
        m_fileaudioformat.setSampleType(QAudioFormat::SignedInt);
        m_fileaudioformat.setSampleSize(32);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_PCM_U8) {
        m_fileaudioformat.setSampleType(QAudioFormat::UnSignedInt);
        m_fileaudioformat.setSampleSize(8);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_FLOAT) {
        m_fileaudioformat.setSampleType(QAudioFormat::Float);
        m_fileaudioformat.setSampleSize(32);
    }
    else if((sfinfo.format&0x00FF)==SF_FORMAT_DOUBLE) {
        m_fileaudioformat.setSampleType(QAudioFormat::Float);
        m_fileaudioformat.setSampleSize(64);
    }

    if((sfinfo.format&0xF0000000)==SF_ENDIAN_LITTLE)
        m_fileaudioformat.setByteOrder(QAudioFormat::LittleEndian);
    else if((sfinfo.format&0xF0000000)==SF_ENDIAN_BIG)
        m_fileaudioformat.setByteOrder(QAudioFormat::BigEndian);

    /* While there are samples in the input file, read them, process
    ** them and write them to the output file.
    */
    while((readcount = sf_read_double (infile, data, BUFFER_LEN))) {
        for(int n=0; n<readcount; n++)
            wav.push_back(data[n]);
    };

    /* Close input and output files. */
    sf_close(infile);
}
