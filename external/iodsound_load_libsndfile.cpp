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

#include "../src/qthelper.h"

extern "C" {
#include    <stdio.h>
#include    <sndfile.h>
}

#include <QFileInfo>

QString FTSound::getAudioFileReadingDescription(){

    QString txt("<a href='http://www.mega-nerd.com/libsndfile'>libsndfile</a>");

    // Add version information
    char buffer[128];
    sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
    txt += " version " + QString(buffer);

    return txt;
}
QStringList FTSound::getAudioFileReadingSupportedFormats() {
    QStringList list;

    char buffer[128];
    sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer)); // Seems to be necessary for the SFC_GET_FORMAT_SUBTYPE to work (!?)

    SF_FORMAT_INFO	info;
    SF_INFO 		sfinfo;
    int format, major_count, subtype_count, m, s;

    sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(int));
    sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof(int));

    sfinfo.channels = 1;
    for (m = 0 ; m < major_count ; m++) {
        info.format = m;
        sf_command(NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof(info));
        QString txt = QString(info.name)+"(."+info.extension+"): ";

        list.append(txt);

        format = info.format;

        for (s = 0; s<subtype_count; s++) {
            info.format = s;
            sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof(info));

            format = (format & SF_FORMAT_TYPEMASK) | info.format;

            sfinfo.format = format;
            if (sf_format_check (&sfinfo))
                list.append(QString("\t")+info.name);
        }
    }

    return list;
}
int FTSound::getNumberOfChannels(const QString& filePath){

    QFileInfo fileInfo(filePath); // To be compatible with Qt5.0.2

    if(!fileInfo.exists())
        throw QString("The file: ")+filePath+" doesn't seem to exist.";

    SNDFILE* in;
    SF_INFO sfinfo;
    in = sf_open(filePath.toLocal8Bit().constData(), SFM_READ, &sfinfo);

    if(in==NULL)
        return 0;

    sf_close(in);

    return sfinfo.channels;
}


/* This will be the length of the buffer used to hold samples while
** we process them.
*/
#define BUFFER_LEN      1024

///* libsndfile can handle more than 6 channels but we'll restrict it to 6. */
//#define    MAX_CHANNELS    6

void FTSound::load(int channelid){

    m_fileaudioformat = QAudioFormat(); // Clear the format
    m_channelid = channelid;
    bool sumchannels = m_channelid==-2;

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
        throw QString("libsndfile: Cannot open input file");
    }

    if(!sumchannels && m_channelid>int(sfinfo.channels))
        throw QString("libsndfile: The requested channel ID is higher than the number of channels in the file.");

    m_fileaudioformat.setChannelCount(sfinfo.channels);
    m_fileaudioformat.setSampleRate(sfinfo.samplerate);
    setSamplingRate(sfinfo.samplerate);

    // TODO Fill the codec name based on:
    //      http://www.mega-nerd.com/libsndfile/api.html

//    std::cout << sfinfo.format << endl;

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
    double sum = 0.0;
    channelid--; // Move indices [1,N] to [0,N-1] to avoid computing -1 to often
    int nbchan = sfinfo.channels;
    int curchannelid = 0;
    while((readcount = sf_read_double (infile, data, BUFFER_LEN))) {
        for(int n=0; n<readcount; n++){

            if(sumchannels){
                sum += data[n];
                if(curchannelid==nbchan-1){
                    wav.push_back(sum/nbchan);
                    sum = 0.0;
                }
            }
            else if(curchannelid==channelid)
                wav.push_back(data[n]);

            curchannelid = (curchannelid+1)%nbchan;
        }
    };

    /* Close input and output files. */
    sf_close(infile);
}
