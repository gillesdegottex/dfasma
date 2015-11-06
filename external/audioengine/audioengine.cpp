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
This example has been widely adapted for the purpose of DFasma.
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

#include "qaehelpers.h"

const int    NotifyIntervalMs       = 100;

//-----------------------------------------------------------------------------
// Constructor and destructor
//-----------------------------------------------------------------------------

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
    , m_fs(0)
    , m_state(QAudio::StoppedState)
    , m_audioOutput(NULL)
    , m_ftsound(NULL)
{
    m_rtinfo_timer.setSingleShot(false);
    m_rtinfo_timer.setInterval(1000*1/12.0);  // Ask for 24 refresh per second
    connect(&m_rtinfo_timer, SIGNAL(timeout()), this, SLOT(sendRealTimeInfo()));
}

void AudioEngine::selectAudioOutputDevice(const QString& devicename) {
    if(devicename=="default") {
        setAudioOutputDevice(QAudioDeviceInfo::defaultOutputDevice());
    }
    else {
        QList<QAudioDeviceInfo> audioDevices = availableAudioOutputDevices();
        for(int di=0; di<audioDevices.size(); di++) {
            if(audioDevices[di].deviceName()==devicename) {
                setAudioOutputDevice(audioDevices[di]);
                return;
            }
        }

        setAudioOutputDevice(QAudioDeviceInfo::defaultOutputDevice());
    }
}


AudioEngine::~AudioEngine()
{
    DFLAG
    if(m_audioOutput) {
        DFLAG
        m_audioOutput->stop();
        DFLAG
        delete m_audioOutput;
        DFLAG
    }
    DFLAG
}

//-----------------------------------------------------------------------------
// Public slots
//-----------------------------------------------------------------------------

void AudioEngine::startPlayback(FTSound* dssound, double tstart, double tstop, double fstart, double fstop)
{
    DLOG << "AudioEngine::startPlayback";

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
            stopPlayback();
            m_ftsound = dssound; // Select the new sound to play
//            connect(m_dssound, SIGNAL(readChannelFinished()), this, SLOT(readChannelFinished()));
            m_tobeplayed = m_ftsound->setPlay(m_format, tstart, tstop, fstart, fstop);
            // TODO Should check that the device is still available before starting it!
            // 2015-10-22 I cannot find a way to do it with current Qt library (5.2)
            m_audioOutput->start(m_ftsound);
            m_rtinfo_timer.start();
            m_starttime = QDateTime::currentMSecsSinceEpoch();
//            cout << "AudioEngine::startPlayback bufferSize: " << m_audioOutput->bufferSize() << endl;
        }
    }

//    std::cout << "~AudioEngine::startPlayback" << endl;
}

void AudioEngine::stopPlayback()
{
    if (m_audioOutput && m_state!=QAudio::StoppedState) {
        m_audioOutput->stop();
        QCoreApplication::instance()->processEvents();
//        if(m_dssound) m_dssound->stop();
        m_tobeplayed = 0.0;
        m_rtinfo_timer.stop();
        emit playPositionChanged(-1);
        emit localEnergyChanged(0);
    }
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
//            cout << "A:" << FTSound::s_play_power << " " << flush;
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
        stopPlayback();
    }
}

void AudioEngine::audioStateChanged(QAudio::State state)
{
    if (state==QAudio::StoppedState) {

        // Check error
        QAudio::Error error = m_audioOutput->error();

        if (QAudio::NoError != error) {
            reset();
            return;
        }
    }
    setState(state);
}

void AudioEngine::reset() {
    stopPlayback();
    setState(QAudio::StoppedState);
    setFormat(QAudioFormat());
    if(m_fs>0)
        initialize(m_fs);
}

bool AudioEngine::initialize(int fs) {
    m_fs = fs;
    m_audioOutput = NULL;
    m_ftsound = NULL;
    stopPlayback();
    setState(QAudio::StoppedState);
    setFormat(QAudioFormat());

    QAudioFormat prevformat = m_format;

    // Force fs and channel count to that of the file
    // TODO Try different audio output
    QAudioFormat format;
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleRate(m_fs);
    format.setChannelCount(1);

    DLOG << "Try format: " << format;
    if (!m_audioOutputDevice.isFormatSupported(format)){
        QString formatstr = formatToString(format);
        DLOG << "Format "+formatstr+" not supported. There will be no audio output!";
        throw QString("Audio output format "+formatstr+" not supported.");
    }

    setFormat(format);

    DLOG << "Format changed";

    // Clear any previous audio output
    if(m_audioOutput){
        m_audioOutput->disconnect(this);
        delete m_audioOutput;
        m_audioOutput = NULL;
    }

    // Build the new audio output
    m_audioOutput = new QAudioOutput(m_audioOutputDevice, m_format, this);
    m_audioOutput->setNotifyInterval(NotifyIntervalMs); // Use 100ms notification intervals
    if(m_audioOutput->notifyInterval()!=NotifyIntervalMs)
        qDebug() << "AudioEngine::initialize: Chosen notification intervals not used!" << m_audioOutput->notifyInterval() << "is used instead of" << NotifyIntervalMs;
    connect(m_audioOutput, SIGNAL(notify()), this, SLOT(audioNotify()));
    connect(m_audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioStateChanged(QAudio::State)));

    // qDebug() << "AudioEngine::initialize " << "format=" << m_format;

//    std::cout << "AudioEngine::initialize periodSize: " << m_audioOutput->periodSize() << endl;
//    std::cout << "AudioEngine: " << m_audioOutput->bufferSize() << endl;

    return true;
}

bool AudioEngine::isInitialized(){
    return !m_audioOutputDevice.isNull()
            && m_audioOutput!=NULL
            && m_fs>0;
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
