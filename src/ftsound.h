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
#include <QGraphicsItem>

#include "filetype.h"
#include "stftcomputethread.h"

#include "qaegiuniformlysampledsignal.h"

#ifdef SIGPROC_FLOAT
#define WAVTYPE float
#else
#define WAVTYPE double
#endif

#define BUTTERRESPONSEDFTLEN 2048


class CFFTW3;
#include "stftcomputethread.h"

class GIWaveform;
class GISpectrumAmplitude;

class FTSound : public QIODevice, public FileType
{
    Q_OBJECT

private:
    void constructor_internal();
    void constructor_external();

    void load(int channelid=1);       // Implementation depends on the used file library (sox, lisndfile, ...)
    void load_finalize();             // Independent of the used file lib.

    QAudioFormat m_fileaudioformat;   // Format of the audio data
    void setSamplingRate(double _fs); // Used by implementations of load
    int m_channelid;  //-2:channels merged; -1:error; 0:no channel; >0:id
    bool m_isclipped;

    // Playback
    QAudioFormat m_outputaudioformat; // Temporary copy for readData
    bool m_isfiltered;
    bool m_isplaying;

public:
    static QString getAudioFileReadingDescription();
    static QStringList getAudioFileReadingSupportedFormats();
    static int getNumberOfChannels(const QString& filePath);
    static double s_fs_common;  // [Hz] Sampling frequency of the sound player // TODO put in sound player

    FTSound(const QString& _fileName, QObject* parent, int channelid=1);
    FTSound(const FTSound& ft);
    virtual FileType* duplicate();

    double fs; // [Hz] Sampling frequency of this specific wav file
    std::vector<WAVTYPE> wav;
    std::vector<WAVTYPE> wavfiltered;
    std::vector<WAVTYPE>* wavtoplay;
    WAVTYPE m_filteredmaxamp;
    QAEGIUniformlySampledSignal* m_giWavForWaveform;

    // Spectra
    class DFTParameters{
    public:
        // DFT related
        unsigned int nl; // [samples]
        unsigned int nr; // [samples]
        int winlen;
        int wintype;
        int normtype;
        std::vector<FFTTYPE> win; // Could avoid this by using classes of windows parameters
        int dftlen;

        // Sound specific parameters
        std::vector<WAVTYPE>* wav; // The used wav to compute the DFT on.
        WAVTYPE ampscale; // [linear]
        qint64 delay;   // [sample index]

        void clear(){
            nl = 0;
            nr = 0;
            winlen = 0;
            wintype = -1;
            normtype = -1;
            win.clear();
            dftlen = 0;
            wav = NULL;
            ampscale = 1.0;
            delay = 0;
        }

        DFTParameters(){
            clear();
        }
        DFTParameters(unsigned int _nl, unsigned int _nr, int _winlen, int _wintype, int _normtype, const std::vector<FFTTYPE>& _win=std::vector<FFTTYPE>(), int _dftlen=0, std::vector<FFTTYPE>* _wav=NULL, qreal _ampscale=1.0, qint64 _delay=0);

        DFTParameters& operator=(const DFTParameters &params);

        bool operator==(const DFTParameters& param) const;
        bool operator!=(const DFTParameters& param) const{return !((*this)==param);}

        inline bool isEmpty() const {return winlen==0 || dftlen==0 || wintype==-1 || normtype==-1;}
    };

    std::vector<FFTTYPE> m_dftamp; // [dB]
    QAEGIUniformlySampledSignal* m_giWavForSpectrumAmplitude;

    std::vector<FFTTYPE> m_dftphase; // [rad]
    QAEGIUniformlySampledSignal* m_giWavForSpectrumPhase;

    std::vector<FFTTYPE> m_dftgd; // [s]
    QAEGIUniformlySampledSignal* m_giWavForSpectrumGroupDelay;

    DFTParameters m_dftparams;

    // Spectrogram
    std::deque<std::vector<WAVTYPE> > m_stft;
    std::deque<FFTTYPE> m_stftts;
    STFTComputeThread::STFTParameters m_stftparams;
    FFTTYPE m_stft_min;
    FFTTYPE m_stft_max;

    // Play (from QIODevice)
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
//    qint64 bytesAvailable() const;
    qint64 m_start; // [sample index]
    qint64 m_pos;   // [sample index]
    qint64 m_end;   // [sample index]
    qint64 m_avoidclickswinpos;// [sample index] position in the pre and post windows

    static WAVTYPE s_play_power;
    static std::deque<WAVTYPE> s_play_power_values;
    static bool s_playwin_use;

    // Visualization
    QAction* m_actionInvPolarity;
    QAction* m_actionResetAmpScale;
    QAction* m_actionResetDelay;
    QAction* m_actionResetFiltering;

    // To keep public
    // The format is not necessarily reliable since it depends fully on the file-reading library
    QAudioFormat format() const {return m_fileaudioformat;}
    virtual QString info() const;
    void setFiltered(bool filtered);
    inline bool isFiltered() const {return m_isfiltered;}
    void updateClippedState();
    inline bool isClipped() const {return m_isclipped;}

    double getDuration() const {return wav.size()/fs;}
    virtual double getLastSampleTime() const;
    virtual void fillContextMenu(QMenu& contextmenu);
    virtual bool isModified();
    virtual void setStatus();
    virtual void setDrawIcon(QPixmap& pm);
    virtual void setColor(const QColor& _color);
    virtual void zposReset();
    virtual void zposBringForward();

    double setPlay(const QAudioFormat& format, double tstart=0.0, double tstop=0.0, double fstart=0.0, double fstop=0.0);
    bool isPlaying() const {return m_isplaying;}
    void stopPlay();

    static std::vector<WAVTYPE> s_avoidclickswindow;
    static void setAvoidClicksWindowDuration(double halfduration);

    ~FTSound();

public slots:
    bool reload();
    void needDFTUpdate();
    void resetAmpScale();
    void resetDelay();
    void setVisible(bool shown);
};

#endif // FTSOUND_H
