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

#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#define DEBUGSTRING std::cout << QThread::currentThreadId() << " " << QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("hh:mm:ss.zzz             ").toLocal8Bit().constData() << " "

#include <deque>
using namespace std;

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QVector>
#include <QTimer>

class FTSound;
QT_BEGIN_NAMESPACE
class QAudioInput;
class QAudioOutput;
QT_END_NAMESPACE

/**
 * This class interfaces with the QtMultimedia audio classes, and also with
 * the SpectrumAnalyser class.  Its role is to manage the capture and playback
 * of audio data, meanwhile performing real-time analysis of the audio level
 * and frequency spectrum.
 */
class AudioEngine : public QObject
{
    Q_OBJECT

    int m_fs;           // The sampling frequency of the output

    qint64 m_starttime;
    QTimer m_rtinfo_timer;

    QAudio::State       m_state;

    QAudioFormat        m_format; // The format of the audio output

    QList<QAudioDeviceInfo> m_availableAudioOutputDevices;
    QAudioDeviceInfo    m_audioOutputDevice;
    QAudioOutput*       m_audioOutput;

    FTSound* m_ftsound; // The selected sound to play
    double m_tobeplayed;

    bool initialize();
    bool selectFormat();
    void setState(QAudio::State state);
    void setFormat(const QAudioFormat &format);

private slots:
    void audioNotify();
    void audioStateChanged(QAudio::State state);
    void readChannelFinished();
    void sendRealTimeInfo();

public:
    explicit AudioEngine(int fs, QObject *parent = 0);
    ~AudioEngine();

    bool isInitialized();

    const QList<QAudioDeviceInfo> &availableAudioOutputDevices() const
                                    { return m_availableAudioOutputDevices; }
    const QAudioDeviceInfo& audioOutputDevice() const { return m_audioOutputDevice; }
    const QAudioFormat& format() const { return m_format; }
    QAudio::State state() const { return m_state; }

public slots:
    void setAudioOutputDevice(const QAudioDeviceInfo &device);
    void startPlayback(FTSound* sound, double tstart=0.0, double tstop=0.0, double fstart=0.0, double fstop=0.0);
    void stopPlayback();
    void reset();

signals:
    void stateChanged(QAudio::State state);
    void infoMessage(const QString &message, int durationMs);
    void errorMessage(const QString &heading, const QString &detail);
    void formatChanged(const QAudioFormat &format);
    void audioOutputDeviceChanged(const QAudioDeviceInfo& device);
    void playPositionChanged(double t);
    void localEnergyChanged(double e);
};

#endif // AUDIOENGINE_H
