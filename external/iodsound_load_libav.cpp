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

libav is a pain because conversion to float values are necessary which are done automatically by libavresample. However, this library is currently not included in Ubuntu.

Another solution is to move to ffmpeg and libswresample.

A last solution is to wait for the war between libav and ffmpeg to be over and use something else in the meanwhile ... now using libsndfile, which seems to be a good standard for reading lossless audio files.

*/

#include "ftsound.h"

#include <iostream>
using namespace std;

//#include <qmath.h>
//#include <qendian.h>

extern "C" {
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
//#include <libavresample/avresample.h>
}

QString FTSound::getAudioFileReadingDescription(){
    return QString("<p>Using <a href='https://libav.org/'>libav</a></p>");
}

void FTSound::load(const QString& _fileName){
    cout << 1 << endl;
    // Load audio file
    fileName = _fileName;

    // Find the apropriate codec and open it
    AVFormatContext* container=avformat_alloc_context();
    if(avformat_open_input(&container,fileName.toLocal8Bit().constData(),NULL,NULL)<0)
        throw QString("libav: Could not open file (this file either doesn't exist or your don't have the proper permissions for reading this file).");

    cout << 2 << endl;

    if(avformat_find_stream_info(container, NULL)<0)
        throw QString("libav: The file information can't be found.");

    cout << 3 << endl;
    // TODO drop it to the standard output
    av_dump_format(container,0,fileName.toLocal8Bit().constData(),false);
//    std::cerr << flush;

    cout << 4 << endl;
    int stream_id=-1;
    unsigned int i;
    for(i=0;i<container->nb_streams;i++){
        if(container->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            stream_id=i;
            break;
        }
    }
    if(stream_id==-1)
        throw QString("No audio stream can be found.");

    cout << 5 << endl;
    AVCodecContext* codec_context = container->streams[stream_id]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    cout << 6 << endl;
    setSamplingRate(codec_context->sample_rate);

    cout << 7 << endl;
    if (!avcodec_open2(codec_context, codec, NULL) < 0)
        throw QString("libav: Could not find the needed codec.");

    cout << 8 << endl;
    AVPacket packet;
    while (av_read_frame(container, &packet)>=0) {

        cout << 9.1 << endl;
        // Decodes from Packet into Frame
        AVFrame* frame = avcodec_alloc_frame ();
        cout << 9.2 << endl;
        int decoded = 0;
        int len = avcodec_decode_audio4(codec_context, frame, &decoded, &packet);

        cout << 9.3 << endl;
        if(len<0)
            throw QString("libav: The audio stream can't be decoded.");

        //        float value;
//        int16_t* buffer = (int16_t*)(frame->data); // TODO manage 8,24,32,etc. bits
        // TODO is there any fn in libav which allow to read a simple int whatever the format ? (bits, LE/BE, etc.)
        for(int n=0; n<8*len/codec_context->bits_per_coded_sample; n++){ // TODO sizeof(uint8_t) ??
            signed short value = ((signed short*)(frame->extended_data[0]))[n];
//            std::cout << value << " ";
            wav.push_back(float(value)/32768.0);
        }
//        std::cout << endl;

        cout << 9.4 << endl;
        av_free(frame);
    }

    cout << 10 << endl;
    avformat_close_input(&container);

    cout << 11 << endl;
}

/*
void Generator::generateData(const QAudioFormat &format, qint64 durationUs, int sampleRate)
{
    const int channelBytes = format.sampleSize() / 8;
//    std::cout << channelBytes << endl;
    const int sampleBytes = format.channelCount() * channelBytes;

    qint64 length = (format.sampleRate() * format.channelCount() * (format.sampleSize() / 8))
                        * durationUs / 1000000;

    Q_ASSERT(length % sampleBytes == 0);
    Q_UNUSED(sampleBytes) // suppress warning in release builds

    m_buffer.resize(length);
    unsigned char *ptr = reinterpret_cast<unsigned char *>(m_buffer.data());
    int sampleIndex = 0;

    while (length) {
        const qreal x = qSin(2 * M_PI * sampleRate * qreal(sampleIndex % format.sampleRate()) / format.sampleRate());
        for (int i=0; i<format.channelCount(); ++i) {
            if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::UnSignedInt) {
                const quint8 value = static_cast<quint8>((1.0 + x) / 2 * 255);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::SignedInt) {
                const qint8 value = static_cast<qint8>(x * 127);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::UnSignedInt) {
                quint16 value = static_cast<quint16>((1.0 + x) / 2 * 65535);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<quint16>(value, ptr);
                else
                    qToBigEndian<quint16>(value, ptr);
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::SignedInt) {
                qint16 value = static_cast<qint16>(x * 32767);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<qint16>(value, ptr);
                else
                    qToBigEndian<qint16>(value, ptr);
            }

            ptr += channelBytes;
            length -= channelBytes;
        }
        ++sampleIndex;
    }
}
*/
