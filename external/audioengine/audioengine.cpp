/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
This example has been widely adapted for the purpose of the DFasma software.
 */

#include "audioengine.h"

#include <math.h>

#include <iostream>

#include <QAudioInput>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMetaObject>
#include <QSet>
#include <QThread>
#include <QtEndian>
#include <QDateTime>

#include <qmath.h>
#include <qendian.h>

#include "../../src/ftsound.h"

const int    NotifyIntervalMs       = 100;

//-----------------------------------------------------------------------------
// Constructor and destructor
//-----------------------------------------------------------------------------

AudioEngine::AudioEngine(int fs, QObject *parent)
    : QObject(parent)
    , m_fs(fs)
    , m_state(QAudio::StoppedState)
    , m_audioOutput(0)
    , m_ftsound(0)
{
    m_rtinfo_timer.setSingleShot(false);
    m_rtinfo_timer.setInterval(1000*1/12.0);  // Ask for 24 refresh per second
    connect(&m_rtinfo_timer, SIGNAL(timeout()), this, SLOT(sendRealTimeInfo()));

    m_availableAudioOutputDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    if(m_availableAudioOutputDevices.size()==1){
        m_audioOutputDevice = QAudioDeviceInfo::defaultOutputDevice();
        std::cout << "Only one audio device available. Select it: " << m_audioOutputDevice.deviceName().toLocal8Bit().constData() << endl;
        emit audioOutputDeviceChanged(m_audioOutputDevice);
    }
    else if(m_availableAudioOutputDevices.size()>1){
        m_audioOutputDevice = QAudioDeviceInfo::defaultOutputDevice();
        std::cout << m_availableAudioOutputDevices.size() << " audio device available. Select the one by default:" << m_audioOutputDevice.deviceName().toLocal8Bit().constData() << endl;
        emit audioOutputDeviceChanged(m_audioOutputDevice);
    }
    else{
        std::cout << "No audio device available!" << endl;
    }

    initialize();
}

AudioEngine::~AudioEngine()
{

}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

bool AudioEngine::isInitialized(){
    return !m_audioOutputDevice.isNull() && m_fs>0;
}

//-----------------------------------------------------------------------------
// Public slots
//-----------------------------------------------------------------------------

void AudioEngine::startPlayback(FTSound* dssound, double tstart, double tend)
{
//    DEBUGSTRING << "AudioEngine::startPlayback" << endl;

    if (m_audioOutput) {
        if (m_state==QAudio::SuspendedState) {
#ifdef Q_OS_WIN
            // The Windows backend seems to internally go back into ActiveState
            // while still returning SuspendedState, so to ensure that it doesn't
            // ignore the resume() call, we first re-suspend
            m_audioOutput->suspend();
#endif
            m_audioOutput->resume();
        } else {
//            DEBUGSTRING << "AudioEngine::startPlayback " << 1 << endl;
            stopPlayback();
//            DEBUGSTRING << "AudioEngine::startPlayback " << 1 << endl;
            m_ftsound = dssound; // Select the new sound to play
//            DEBUGSTRING << "AudioEngine::startPlayback " << 1 << endl;
//            connect(m_dssound, SIGNAL(readChannelFinished()), this, SLOT(readChannelFinished()));
//            DEBUGSTRING << "AudioEngine::startPlayback " << 1 << endl;
            m_tobeplayed = m_ftsound->setPlay(m_format, tstart, tend);
//            DEBUGSTRING << "AudioEngine::startPlayback " << 1 << endl;
            m_audioOutput->start(m_ftsound);
            m_rtinfo_timer.start();
            m_starttime = QDateTime::currentMSecsSinceEpoch();
        }
    }

//    DEBUGSTRING << "~AudioEngine::startPlayback" << endl;
}

void AudioEngine::stopPlayback()
{
//    DEBUGSTRING << "AudioEngine::stopPlayback" << endl;
    if (m_audioOutput) {
//        DEBUGSTRING << "AudioEngine::stopPlayback 1" << endl;
        m_audioOutput->stop();
//        DEBUGSTRING << "AudioEngine::stopPlayback 2" << endl;
        QCoreApplication::instance()->processEvents();
//        DEBUGSTRING << "AudioEngine::stopPlayback 3" << endl;
//        if(m_dssound) m_dssound->stop();
//        DEBUGSTRING << "AudioEngine::stopPlayback 4" << endl;
        m_tobeplayed = 0.0;
        m_rtinfo_timer.stop();
        emit playPositionChanged(-1);
        emit localEnergyChanged(0);
    }
//    DEBUGSTRING << "~AudioEngine::stopPlayback" << endl;
}

void AudioEngine::setAudioOutputDevice(const QAudioDeviceInfo &device)
{
    if (device.deviceName() != m_audioOutputDevice.deviceName()) {
        m_audioOutputDevice = device;
        reset();

        emit audioOutputDeviceChanged(m_audioOutputDevice);
    }
}

//-----------------------------------------------------------------------------
// Private slots
//-----------------------------------------------------------------------------

void AudioEngine::sendRealTimeInfo(){
//    double t = QDateTime::currentMSecsSinceEpoch();
//    static double lastt = 0.0;
//    std::cout << "AudioEngine::sendRealTimeInfo dt=" << (t-lastt) << " play_pos=" << 100*(m_audioOutput->processedUSecs()/1000000.0)/m_tobeplayed << "%" << endl;
//    lastt = t;
//    std::cout << "start=" << QDateTime::fromMSecsSinceEpoch(m_starttime).toString("hh:mm:ss.zzz             ").toLocal8Bit().constData() << " curr=" << QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("hh:mm:ss.zzz             ").toLocal8Bit().constData() << " AudioEngine::sendRealTimeInfo" << endl;

    if(m_ftsound){
        double t = double(m_ftsound->m_start/m_ftsound->fs) + (QDateTime::currentMSecsSinceEpoch() - m_starttime)/1000.0;
//        double t = double(m_dssound->m_start)/m_dssound->fs + m_audioOutput->processedUSecs()/1000000.0;

        if(t > double(m_ftsound->m_end/m_ftsound->fs)){
            emit playPositionChanged(-1);
            emit localEnergyChanged(0);
        }
        else{
            emit playPositionChanged(t);
//            emit localEnergyChanged(sqrt(FTSound::s_play_power/(m_fs*0.1)));
            emit localEnergyChanged(FTSound::s_play_power); // For max amplitude in window
        }
    }
}

void AudioEngine::audioNotify()
{
//    double t = QDateTime::currentMSecsSinceEpoch();
//    static double lastt = 0.0;
//    std::cout << "AudioEngine::audioNotify dt=" << (t-lastt) << " play_pos=" << 100*(m_audioOutput->processedUSecs()/1000000.0)/m_tobeplayed << "%" << endl;
//    lastt = t;

    // Add 0.5s in order to give time to the lowest level buffer to be played completely
    if (m_audioOutput->processedUSecs()/1000000.0 > m_tobeplayed + 0.5){
//        DEBUGSTRING << "AudioEngine::audioNotify trigger auto-stop" << endl;
        stopPlayback();
    }

//    DEBUGSTRING << "~AudioEngine::audioNotify" << endl;
}

void AudioEngine::audioStateChanged(QAudio::State state)
{
//    DEBUGSTRING << "AudioEngine::audioStateChanged from" << m_state << "to" << state << endl;

    if (state==QAudio::StoppedState) {

        // Check error
        QAudio::Error error = QAudio::NoError;
        error = m_audioOutput->error();

        if (QAudio::NoError != error) {
//            DEBUGSTRING << "AudioEngine::audioStateChanged: finished with errors!" << endl;
            reset();
            return;
        }
    }
    setState(state);

//    DEBUGSTRING << "~AudioEngine::audioStateChanged" << endl;
}

void AudioEngine::reset()
{
    stopPlayback();
    setState(QAudio::StoppedState);
    setFormat(QAudioFormat());
    initialize();
}

bool AudioEngine::initialize()
{
    bool result = false;

    QAudioFormat format = m_format;

    if (selectFormat()) {
        if (m_format != format) {
            if(m_audioOutput){
                m_audioOutput->disconnect(this);
                delete m_audioOutput;
                m_audioOutput = 0;
            }
            result = true;
            m_audioOutput = new QAudioOutput(m_audioOutputDevice, m_format, this);
            m_audioOutput->setNotifyInterval(NotifyIntervalMs); // Use 100ms notification intervals
            if(m_audioOutput->notifyInterval()!=NotifyIntervalMs)
                qDebug() << "AudioEngine::initialize: Chosen notification intervals not used!" << m_audioOutput->notifyInterval() << "is used instead of" << NotifyIntervalMs;
            connect(m_audioOutput, SIGNAL(notify()), this, SLOT(audioNotify()));
            connect(m_audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioStateChanged(QAudio::State)));
        }
    }
    else {
        emit errorMessage(tr("No common output format found"), "");
    }

    qDebug() << "AudioEngine::initialize" << "format" << m_format;

//    std::cout << "AudioEngine: " << m_audioOutput->bufferSize() << endl;

    return result;
}

bool AudioEngine::selectFormat()
{
    // Force fs and channel count to that of the file
    QAudioFormat format;
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleRate(m_fs);
    format.setChannelCount(1);

    const bool outputSupport = m_audioOutputDevice.isFormatSupported(format);

    if (!outputSupport){
        // TODO merge with function initialize and throw an exception
        qDebug() << "AudioEngine::initialize format" << format << "not supported!";
        qDebug() << "There will be no audio output!";
        format = QAudioFormat();
    }

    setFormat(format);

    return outputSupport;
}

void AudioEngine::setFormat(const QAudioFormat &format)
{
    const bool changed = (format != m_format);
    m_format = format;
    if (changed)
        emit formatChanged(m_format);
}

void AudioEngine::setState(QAudio::State state)
{
    const bool changed = (m_state != state);
    m_state = state;
    if (changed)
        emit stateChanged(m_state);
}

void AudioEngine::readChannelFinished(){
//    DEBUGSTRING << "AudioTest::readChannelFinished" << endl;
//    static bool runonce=true;
//    if(runonce){
//        runonce = false;
//        m_dssound->stop();
//        m_audioOutput->stop();
////        m_audioOutput->disconnect(this);
//    }
//    DEBUGSTRING << "~AudioTest::readChannelFinished" << endl;
}
