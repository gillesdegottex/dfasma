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

#include "../src/qthelper.h"

QString FTSound::getAudioFileReadingDescription() {
    return QString("Using builtin <a href='http://qt-project.org/doc/qt-5.0/qtmultimedia/audiooverview.html'>Qt audio decoder</a>");
}
QStringList FTSound::getAudioFileReadingSupportedFormats() {
    QStringList list;

    list.append("(no idea)");

    return list;
}

AudioDecoder::AudioDecoder()
    : m_decoder(this)
{
    // Make sure the data we receive is in correct PCM format.
    // Our wav file writer only supports SignedInt sample type.
    QAudioFormat format;
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setSampleRate(44100);
    format.setCodec("audio/pcm");
//    format.setCodec("audio/x-raw");
    format.setSampleType(QAudioFormat::SignedInt);
//    format.setSampleType(QAudioFormat::UnSignedInt);

    m_decoder.setAudioFormat(format);


    connect(&m_decoder, SIGNAL(bufferReady()), this, SLOT(readBuffer()));
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

void AudioDecoder::readBuffer()
{
    FLAG

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
        std::cout << "Resource error" << endl;
        break;
    case QAudioDecoder::FormatError:
        std::cout << "Format error" << endl;
        break;
    case QAudioDecoder::AccessDeniedError:
        std::cout << "Access denied error" << endl;
        break;
    case QAudioDecoder::ServiceMissingError:
        std::cout << "Service missing error" << endl;
        break;
    }

//    emit done();
}

void AudioDecoder::stateChanged(QAudioDecoder::State newState)
{
    switch (newState) {
    case QAudioDecoder::DecodingState:
        std::cout << "Decoding..." << endl;
        break;
    case QAudioDecoder::StoppedState:
        std::cout << "Decoding stopped" << endl;
        break;
    }
}

void AudioDecoder::finished()
{
//    if (!m_fileWriter.close())
//        m_cout << "Failed to finilize output file" << endl;

    std::cout << "Decoding finished" << endl;

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
        std::cout << "Decoding progress: " << (int)(progress * 100.0) << "%" << endl;
        m_progress = progress;
    }
}



int FTSound::getNumberOfChannels(const QString &filePath){
    COUTD << filePath.toLatin1().constData() << endl;


    AudioDecoder *decoder = new AudioDecoder();
    decoder->setSourceFilename(filePath);

//    QAudioFormat format = decoder->audioFormat();

    QAudioFormat format = decoder->m_decoder.audioFormat();

    COUTD << format << endl;

//    connect(decoder, SIGNAL(bufferReady()), this, SLOT(readBuffer()));
    decoder->start();


    COUTD << format << endl;

    int nchan = format.channelCount();

    delete decoder;

    if(nchan==-1)
        throw QString("Qt file reader: Cannot read the number of channels in this file.");

    return nchan;
}

void FTSound::load(int channelid){
    if(channelid>1)
        throw QString("Qt file reader: Can read only the first and unique channel of the file.");

    m_fileaudioformat = QAudioFormat(); // Clear the format

//    QAudioFormat desiredFormat;
//    desiredFormat.setChannelCount(2);
//    desiredFormat.setCodec("audio/x-raw");
//    desiredFormat.setSampleType(QAudioFormat::UnSignedInt);
//    desiredFormat.setSampleRate(48000);
//    desiredFormat.setSampleSize(16);

    AudioDecoder *decoder = new AudioDecoder();
//    decoder->setAudioFormat(desiredFormat);
    decoder->setSourceFilename(fileFullPath);

    QAudioFormat format = decoder->m_decoder.audioFormat();

//    connect(decoder, SIGNAL(bufferReady()), this, SLOT(readBuffer()));
    decoder->start();

//    qDebug() << format;

    // Now wait for bufferReady() signal and call decoder->read()


//    while((readcount = sf_read_double (infile, data, BUFFER_LEN))) {
//        for(int n=0; n<readcount; n++)
//            wav.push_back(data[n]);
//    };

    delete decoder;
}
