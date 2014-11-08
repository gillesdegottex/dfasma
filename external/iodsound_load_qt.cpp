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


#include "../src/ftsound.h"

#include <iostream>
using namespace std;
#include <stdio.h>

#include "iodsound_load_qt.h"

QString FTSound::getAudioFileReadingDescription(){

    QString txt = QString("<p>Using builtin <a href='http://qt-project.org/doc/qt-5.0/qtmultimedia/audiooverview.html'>Qt audio decoder</a>");

//    // Add version information
//    char buffer[128];
//    sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
//    txt += " - version " + QString(buffer);
    txt += "</p>";

//    txt += "<p><b>Supported formats</b><br/>";
//    SF_FORMAT_INFO	info;
//    SF_INFO 		sfinfo;
//    int format, major_count, subtype_count, m, s;

//    sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof (int));
//    sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof (int));

//    sfinfo.channels = 1;
//    for (m = 0 ; m < major_count ; m++) {
//        info.format = m;
//        sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
//        txt += QString(info.name)+"(."+info.extension+")<br/>";

//        format = info.format;

//        for (s = 0; s<subtype_count; s++) {
//            info.format = s;
//            sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info));

//            format = (format & SF_FORMAT_TYPEMASK) | info.format;

//            sfinfo.format = format;
//            if (sf_format_check (&sfinfo))
//                txt += QString("*")+info.name+"<br/>";
//        }
//    }

//    txt += "</p>";

    return txt;
}

AudioDecoder::AudioDecoder(bool isDelete)
    : m_cout(stdout, QIODevice::WriteOnly)
{
    // Make sure the data we receive is in correct PCM format.
    // Our wav file writer only supports SignedInt sample type.
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setSampleRate(48000);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    m_decoder.setAudioFormat(format);

    connect(&m_decoder, SIGNAL(bufferReady()), this, SLOT(bufferReady()));
    connect(&m_decoder, SIGNAL(error(QAudioDecoder::Error)), this, SLOT(error(QAudioDecoder::Error)));
    connect(&m_decoder, SIGNAL(stateChanged(QAudioDecoder::State)), this, SLOT(stateChanged(QAudioDecoder::State)));
    connect(&m_decoder, SIGNAL(finished()), this, SLOT(finished()));
    connect(&m_decoder, SIGNAL(positionChanged(qint64)), this, SLOT(updateProgress()));
    connect(&m_decoder, SIGNAL(durationChanged(qint64)), this, SLOT(updateProgress()));

    m_progress = -1.0;
}

void AudioDecoder::setSourceFilename(const QString &fileName)
{
    m_decoder.setSourceFilename(fileName);
}

void AudioDecoder::start()
{
    m_decoder.start();
}

void AudioDecoder::stop()
{
    m_decoder.stop();
}

void AudioDecoder::setTargetFilename(const QString &fileName)
{
    m_targetFilename = fileName;
}

void AudioDecoder::bufferReady()
{
    // read a buffer from audio decoder
    QAudioBuffer buffer = m_decoder.read();
    if (!buffer.isValid())
        return;

//    if (!m_fileWriter.isOpen() && !m_fileWriter.open(m_targetFilename, buffer.format())) {
//        m_decoder.stop();
//        return;
//    }

//    m_fileWriter.write(buffer);
}

void AudioDecoder::error(QAudioDecoder::Error error)
{
    switch (error) {
    case QAudioDecoder::NoError:
        return;
    case QAudioDecoder::ResourceError:
        m_cout << "Resource error" << endl;
        break;
    case QAudioDecoder::FormatError:
        m_cout << "Format error" << endl;
        break;
    case QAudioDecoder::AccessDeniedError:
        m_cout << "Access denied error" << endl;
        break;
    case QAudioDecoder::ServiceMissingError:
        m_cout << "Service missing error" << endl;
        break;
    }

//    emit done();
}

void AudioDecoder::stateChanged(QAudioDecoder::State newState)
{
    switch (newState) {
    case QAudioDecoder::DecodingState:
        m_cout << "Decoding..." << endl;
        break;
    case QAudioDecoder::StoppedState:
        m_cout << "Decoding stopped" << endl;
        break;
    }
}

void AudioDecoder::finished()
{
//    if (!m_fileWriter.close())
//        m_cout << "Failed to finilize output file" << endl;

    m_cout << "Decoding finished" << endl;

//    emit done();
}

void AudioDecoder::updateProgress()
{
    qint64 position = m_decoder.position();
    qint64 duration = m_decoder.duration();
    qreal progress = m_progress;
    if (position >= 0 && duration > 0)
        progress = position / (qreal)duration;

    if (progress > m_progress + 0.1) {
        m_cout << "Decoding progress: " << (int)(progress * 100.0) << "%" << endl;
        m_progress = progress;
    }
}


void FTSound::load(){

    m_fileaudioformat = QAudioFormat(); // Clear the format

//    QAudioFormat desiredFormat;
//    desiredFormat.setChannelCount(2);
//    desiredFormat.setCodec("audio/x-raw");
//    desiredFormat.setSampleType(QAudioFormat::UnSignedInt);
//    desiredFormat.setSampleRate(48000);
//    desiredFormat.setSampleSize(16);

    QAudioDecoder *decoder = new QAudioDecoder(this);
//    decoder->setAudioFormat(desiredFormat);
    decoder->setSourceFilename(fileFullPath);

    QAudioFormat format = decoder->audioFormat();

//    connect(decoder, SIGNAL(bufferReady()), this, SLOT(readBuffer()));
    decoder->start();

//    qDebug() << format;

    // Now wait for bufferReady() signal and call decoder->read()


//    while((readcount = sf_read_double (infile, data, BUFFER_LEN))) {
//        for(int n=0; n<readcount; n++)
//            wav.push_back(data[n]);
//    };
}
