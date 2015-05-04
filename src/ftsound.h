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

#ifndef FTSOUND_H
#define FTSOUND_H

#include <deque>
#include <vector>
#include <complex>

#include <QString>
#include <QColor>
#include <QAudioFormat>
#include <QAction>

#include "filetype.h"

#define WAVTYPE double

#define BUTTERRESPONSEDFTLEN 2048


class CFFTW3;
#include "stftcomputethread.h"

class FTSound : public QIODevice, public FileType
{
    Q_OBJECT

    void init(); // Called only once upon creation

    void load(int channelid=1);       // Implementation depends on the used file library (sox, lisndfile, ...)
    void load_finalize();             // Independent of the used file lib.

    QAudioFormat m_fileaudioformat;   // Format of the audio data

    QAudioFormat m_outputaudioformat; // Temporary copy for readData

    void setSamplingRate(double _fs); // Used by implementations of load

    int m_channelid;  //-2:channels merged; -1:error; 0:no channel; >0:id
    bool m_isclipped;
    bool m_isfiltered;

public:

    FTSound(const QString& _fileName, QObject* parent, int channelid=1);
    FTSound(const FTSound& ft);
    virtual FileType* duplicate();

    static double fs_common;  // [Hz] Sampling frequency of the sound player // TODO put in sound player
    static std::vector<WAVTYPE> sm_avoidclickswindow;

    double fs; // [Hz] Sampling frequency of this specific wav file
    std::vector<WAVTYPE> wav;
    std::vector<WAVTYPE> wavfiltered;
    std::vector<WAVTYPE>* wavtoplay;
    WAVTYPE m_wavmaxamp;
    WAVTYPE m_filteredmaxamp;

    qreal m_ampscale; // [linear]
    qint64 m_delay;   // [sample index]

    // Spectrum
    std::vector<std::complex<WAVTYPE> > m_dft; // Store the _log_ of the DFT
    QTime m_dft_lastupdate; // Use a simple time checking system for updating the least DFTs
                            // (Could use a Parameters system (like for the STFT), but this would be
                            //  a bit heavy since we don't need to avoid absolutely all re-computations)
    qreal m_dft_min;
    qreal m_dft_max;

    // Spectrogram
    std::deque<std::vector<WAVTYPE> > m_stft;
    std::deque<double> m_stftts;
    STFTComputeThread::STFTParameters m_stftparams;
//    QTime m_stft_lastupdate; // Use a simple time checking system for updating the least DFTs
    qreal m_stft_min;
    qreal m_stft_max;
//    QTime m_stftimg_lastupdate; // Use a simple time checking system for updating the least DFTs

    // QIODevice
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
//    qint64 bytesAvailable() const;
    qint64 m_start; // [sample index]
    qint64 m_pos;   // [sample index]
    qint64 m_end;   // [sample index]
    qint64 m_avoidclickswinpos;// [sample index] position in the pre and post windows

    static WAVTYPE s_play_power;
    static std::deque<WAVTYPE> s_play_power_values;
    static bool sm_playwin_use;

    // Visualization
    QAction* m_actionInvPolarity;
    QAction* m_actionResetAmpScale;
    QAction* m_actionResetDelay;
    QAction* m_actionResetFiltering;

    // To keep public
    QAudioFormat format() const {return m_fileaudioformat;}
    virtual QString info() const;
    void setFiltered(bool filtered);
    inline bool isFiltered() const {return m_isfiltered;}
    void updateClippedState();
    inline bool isClipped() const {return m_isclipped;}

    double getDuration() const {return wav.size()/fs;}
    virtual double getLastSampleTime() const;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);
    virtual bool isModified();
    virtual void setStatus();

    double setPlay(const QAudioFormat& format, double tstart=0.0, double tstop=0.0, double fstart=0.0, double fstop=0.0);
    void stopPlay();

    static void setAvoidClicksWindowDuration(double halfduration);
    static QString getAudioFileReadingDescription();
    static int getNumberOfChannels(const QString& filePath);

    ~FTSound();

public slots:
    void reload();
    void needDFTUpdate();
    void resetFiltering();
    void resetAmpScale();
    void resetDelay();
    void setVisible(bool shown);
};

#endif // FTSOUND_H
