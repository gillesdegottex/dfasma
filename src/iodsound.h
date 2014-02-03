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

#ifndef IODSOUND_H
#define IODSOUND_H

#include <deque>
#include <vector>
#include <complex>

#include <QString>
#include <QColor>
#include <QAudioFormat>
#include <QAction>

class CFFTW3;

class IODSound : public QIODevice
{
    Q_OBJECT

    void load(const QString& _fileName);

    QAudioFormat m_outputaudioformat; // Temporary copy for readData

    void setSamplingRate(float _fs); // Used by implementations of load

public:
    IODSound(const QString& _fileName, QObject* parent);
    static QString getAudioFileReadingDescription();

    std::deque<float> wav;
    float m_wavmaxamp;
    float fs; // [Hz]
    static float fs_common;  // [Hz]
    static float s_play_power;
    static std::deque<float> s_play_power_values;
    QColor color;
    QString fileName;
    float getDuration() const {return wav.size()/fs;}
    float getLastSampleTime() const {return (wav.size()-1)/fs;}

    // Spectrum
    std::vector<std::complex<double> > m_dft;

    // QIODevice
    double setPlay(const QAudioFormat& format, double tstart=0.0, double tstop=0.0);
    void stop();
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
//    qint64 bytesAvailable() const;
    qint64 m_start; // [sample index]
    qint64 m_pos;   // [sample index]
    qint64 m_end;   // [sample index]

    qreal m_ampscale; // [linear]
    qreal m_delay;    // [s]

    // Visualization
    QAction* m_actionShow;
    QAction* m_actionInvPolarity;
    QAction* m_actionResetAmpScale;
    QAction* m_actionResetDelay;

    ~IODSound();
};

#endif // IODSOUND_H
