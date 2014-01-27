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

#include "iodsound.h"
#include "external/CFFTW3.h"

#include <iostream>
#include <deque>
using namespace std;

#include <qmath.h>
#include <qendian.h>

static int sg_colors_loaded = 0;
static deque<QColor> sg_colors;

QColor GetNextColor(){

    // If empty, initialize the colors
    if(sg_colors.empty()){
        if(sg_colors_loaded==1){
            sg_colors.push_back(QColor(64, 64, 64).lighter());
            sg_colors.push_back(QColor(0, 0, 255).lighter());
            sg_colors.push_back(QColor(0, 127, 0).lighter());
            sg_colors.push_back(QColor(255, 0, 0).lighter());
            sg_colors.push_back(QColor(0, 192, 192).lighter());
            sg_colors.push_back(QColor(192, 0, 192).lighter());
            sg_colors.push_back(QColor(192, 192, 0).lighter());
        }
        else if(sg_colors_loaded==2){
            sg_colors.push_back(QColor(64, 64, 64).darker());
            sg_colors.push_back(QColor(0, 0, 255).darker());
            sg_colors.push_back(QColor(0, 127, 0).darker());
            sg_colors.push_back(QColor(255, 0, 0).darker());
            sg_colors.push_back(QColor(0, 192, 192).darker());
            sg_colors.push_back(QColor(192, 0, 192).darker());
            sg_colors.push_back(QColor(192, 192, 0).darker());
        }
        else{
            sg_colors.push_back(QColor(64, 64, 64));
            sg_colors.push_back(QColor(0, 0, 255));
            sg_colors.push_back(QColor(0, 127, 0));
            sg_colors.push_back(QColor(255, 0, 0));
            sg_colors.push_back(QColor(0, 192, 192));
            sg_colors.push_back(QColor(192, 0, 192));
            sg_colors.push_back(QColor(192, 192, 0));
        }
        sg_colors_loaded++;
    }

    QColor color = sg_colors.front();

    sg_colors.pop_front();

//    cout << "1) R:" << color.redF() << " G:" << color.greenF() << " B:" << color.blueF() << " m:" << (color.redF()+color.greenF()+color.blueF())/3.0 << " L:" << color.lightnessF() << " V:" << color.valueF() << endl;

    return color;
}

float IODSound::fs_common = 0; // Initially, fs is undefined
float IODSound::s_play_power = 0;
std::deque<float> IODSound::s_play_power_values;

IODSound::IODSound(const QString& _fileName, QObject *parent)
    : QIODevice(parent)
    , m_pos(0)
    , m_end(0)
    , m_ampscale(1.0)
{
    m_actionShow = new QAction("Show", this);
    m_actionShow->setStatusTip(tr("Show the sound in the views"));
    m_actionShow->setCheckable(true);
    m_actionShow->setChecked(true);

    m_actionInvPolarity = new QAction("Inverse polarity", this);
    m_actionInvPolarity->setStatusTip(tr("Inverse the polarity of the sound"));
    m_actionInvPolarity->setCheckable(true);
    m_actionInvPolarity->setChecked(false);

    load(_fileName);
    std::cout << wav.size() << " samples loaded (" << wav.size()/fs << "s)" << endl;

    color = GetNextColor();

//    QIODevice::open(QIODevice::ReadOnly);
}

void IODSound::setSamplingRate(float _fs){

    fs = _fs;

    // Check if fs is the same for all files
    if(fs_common==0){
        // The system has no defined sampling rate
        fs_common = fs;
    }
    else {
        // Check if fs is the same as that of the other files
        if(fs_common!=fs)
            throw QString("The sampling rate is not the same as that of the files already loaded.");
    }
}

double IODSound::start(const QAudioFormat& format, double tstart, double tstop)
{
    m_outputaudioformat = format;

    s_play_power = 0;
    s_play_power_values.clear();

    if(tstart>tstop){
        double tmp = tstop;
        tstop = tstart;
        tstart = tmp;
    }

    if(tstart==0.0 && tstop==0.0){
        m_start = 0;
        m_pos = 0;
        m_end = wav.size()-1;
    }
    else{
        m_start = max(0, min(int(wav.size()-1), int(tstart*fs)));
        m_pos = m_start;
        m_end = max(0, min(int(wav.size()-1), int(tstop*fs)));
    }

    QIODevice::open(QIODevice::ReadOnly);

    double tobeplayed = double(m_end-m_pos+1)/fs;

//    std::cout << "DSSound::start [" << tstart << "s(" << m_pos << "), " << tstop << "s(" << m_end << ")] " << tobeplayed << "s" << endl;

    return tobeplayed;
}

void IODSound::stop()
{
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    QIODevice::close();
}

// Assuming the output audio device has been open in 16bits ...
// TODO Manage more output formats
const qint16  PCMS16MaxValue     =  32767;
const quint16 PCMS16MaxAmplitude =  32768; // because minimum is -32768
inline qreal pcmToReal(qint16 pcm) {
    return qreal(pcm) / PCMS16MaxAmplitude;
}
inline qint16 realToPcm(qreal real) {
    return real * PCMS16MaxValue;
}

qint64 IODSound::readData(char *data, qint64 len)
{
//    std::cout << "DSSound::readData requested=" << len << endl;

    qint64 writtenbytes = 0; // [bytes]

/*    while (len - writtenbytes > 0) {
        const qint64 chunk = qMin((m_buffer.size() - m_pos), len - writtenbytes);
        memcpy(data + writtenbytes, m_buffer.constData() + m_pos, chunk);
        m_pos = (m_pos + chunk) % m_buffer.size();
        writtenbytes += chunk;
    }*/

    const int channelBytes = m_outputaudioformat.sampleSize() / 8;

    unsigned char *ptr = reinterpret_cast<unsigned char *>(data);

    // Polarity apparently matters in very particular cases
    // so take it into account when playing.
    int pol = 1;
    if(m_actionInvPolarity->isChecked())
        pol = -1;

    while(writtenbytes<len) {

//        float e = wav[m_pos]*wav[m_pos];
//        s_play_power += e;
        float e = abs(wav[m_pos]);
        s_play_power_values.push_front(e);
        while(s_play_power_values.size()/fs>0.1){
            s_play_power -= s_play_power_values.back();
            s_play_power_values.pop_back();
        }

//        cout << 20*log10(sqrt(s_play_power/s_play_power_values.size())) << endl;

        qint16 value;
        if(m_pos<=m_end) value=realToPcm(pol*wav[m_pos]);
        else             value=realToPcm(0.0);

        qToLittleEndian<qint16>(value, ptr);
        ptr += channelBytes;
        writtenbytes += channelBytes;

        if(m_pos<m_end) m_pos++;
    }

    s_play_power = 0;
    for(unsigned int i=0; i<s_play_power_values.size(); i++)
        s_play_power = max(s_play_power, s_play_power_values[i]);

//    std::cout << "~DSSound::readData writtenbytes=" << writtenbytes << " m_pos=" << m_pos << " m_end=" << m_end << endl;

    if(m_pos>=m_end){
//        std::cout << "STOP!!!" << endl;
//        QIODevice::close(); // TODO do this instead ??
//        emit readChannelFinished();
        return writtenbytes;
    }
    else{
        return writtenbytes;
    }
}

qint64 IODSound::writeData(const char *data, qint64 len){

    Q_UNUSED(data)
    Q_UNUSED(len)

    std::cerr << "DSSound::writeData: There is no reason to call this function.";

    return 0;
}

IODSound::~IODSound(){
    QIODevice::close();

    delete m_actionShow;

    sg_colors.push_back(color);
}

//qint64 DSSound::bytesAvailable() const
//{
//    std::cout << "DSSound::bytesAvailable " << QIODevice::bytesAvailable() << endl;

//    return QIODevice::bytesAvailable();
////    return m_buffer.size() + QIODevice::bytesAvailable();
//}

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
