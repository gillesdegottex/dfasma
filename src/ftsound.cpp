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

#include "ftsound.h"

#include <iostream>
#include <limits>
#include <deque>
using namespace std;

#include <qmath.h>
#include <qendian.h>
#include <QMenu>
#include <QMessageBox>
#include <QFileInfo>
#include <QGraphicsRectItem>
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrum.h"
#include "gvwaveform.h"
#include "sigproc.h"

#include "../external/mkfilter/mkfilter.h"

#include "ui_wdialogsettings.h"

bool FTSound::sm_playwin_use = false;
std::vector<WAVTYPE> FTSound::sm_avoidclickswindow;

double FTSound::fs_common = 0; // Initially, fs is undefined TODO put in wmainwindow
WAVTYPE FTSound::s_play_power = 0;
std::deque<WAVTYPE> FTSound::s_play_power_values;

void FTSound::init(){
    m_channelid = 0;
    m_isclipped = false;
    m_isfiltered = false;
    wavtoplay  = &wav;
    m_filteredmaxamp = 0.0;
    m_ampscale = 1.0;
    m_delay = 0;
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;

    m_actionInvPolarity = new QAction("Inverse polarity", this);
    m_actionInvPolarity->setStatusTip(tr("Inverse the polarity of the sound"));
    m_actionInvPolarity->setCheckable(true);
    m_actionInvPolarity->setChecked(false);

    m_actionResetAmpScale = new QAction("Reset amplitude", this);
    m_actionResetAmpScale->setStatusTip(tr("Reset the amplitude scaling to 1"));

    m_actionResetDelay = new QAction("Reset the delay", this);
    m_actionResetDelay->setStatusTip(tr("Reset the delay to 0s"));

    m_actionResetFiltering = new QAction("Remove filtering effects", this);
    m_actionResetFiltering->setStatusTip(tr("Reset to original signal without filtering effects"));
    connect(m_actionResetFiltering, SIGNAL(triggered()), this, SLOT(resetFiltering()));

    connect(m_actionReload, SIGNAL(triggered()), this, SLOT(reload()));
}

FTSound::FTSound(const QString& _fileName, QObject *parent, int channelid)
    : QIODevice(parent)
    , FileType(FTSOUND, _fileName, this)
{
    init();

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        load(channelid);
        load_finalize();
    }

    WMainWindow::getMW()->ftsnds.push_back(this);

//    QIODevice::open(QIODevice::ReadOnly);
}

FTSound::FTSound(const FTSound& ft)
    : QIODevice(ft.parent())
    , FileType(FTSOUND, ft.fileFullPath, this)
{
    init();

    wav = ft.wav;
    m_wavmaxamp = ft.m_wavmaxamp;
    fs = ft.fs;
    m_fileaudioformat.setSampleRate(fs);
    m_fileaudioformat.setSampleType(QAudioFormat::Float);
    m_fileaudioformat.setSampleSize(8*sizeof(WAVTYPE));

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    WMainWindow::getMW()->ftsnds.push_back(this);
}

void FTSound::load_finalize() {
    m_wavmaxamp = 0.0;
    for(unsigned int n=0; n<wav.size(); ++n)
        m_wavmaxamp = std::max(m_wavmaxamp, abs(wav[n]));

    if(sm_avoidclickswindow.size()==0)
        FTSound::setAvoidClicksWindowDuration(WMainWindow::getMW()->m_dlgSettings->ui->sbAvoidClicksWindowDuration->value());

//    std::cout << "INFO: " << wav.size() << " samples loaded (" << wav.size()/fs << "s max amplitude=" << m_wavmaxamp << ")" << endl;

    m_lastreadtime = QDateTime::currentDateTime();
    setStatus();
}

void FTSound::reload() {
//    cout << "FTSound::reload" << endl;

    stop();

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return;

    // Reset everything ...
    wavtoplay = &wav;
    m_ampscale = 1.0;
    m_delay = 0;
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;
    wav.clear();
    wavfiltered.clear();
    resetFiltering();

    // ... and reload the data from the file
    load();
    load_finalize();
}

FileType* FTSound::duplicate(){
    return new FTSound(*this);
}

QString FTSound::info() const {
    QString str = "";

    QString datestr = m_lastreadtime.toString("HH:mm:ss ddMMM");
    if(m_modifiedtime>m_lastreadtime) datestr = "<b>"+datestr+"</b>";
    str += "Loaded at "+datestr+"<br/>";

    if(m_modifiedtime==QDateTime())
        str += "<b>Inaccessible</b><br/>";
    else{
        datestr = m_modifiedtime.toString("HH:mm:ss ddMMM");
        if(m_modifiedtime>m_lastreadtime) datestr = "<b>"+datestr+"</b>";
        str += "Modified at "+datestr+"<br/>";
    }

    str += "Duration: "+QString::number(getDuration())+"s ("+QString::number(wav.size())+")<br/>";

    if(m_ampscale!=1.0)
        str += "Scaled: "+QString::number(20*std::log10(m_ampscale), 'f', 4)+"dB ("+QString::number(m_ampscale, 'f', 4)+")<br/>";
    if(isClipped())
        str += "<font color=\"red\"><b>CLIPPED</b></font><br/>";
    if(m_delay!=0.0)
        str += "Delayed: "+QString::number(double(m_delay)/fs, 'f', 4)+"s ("+QString::number(m_delay)+")<br/>";

    QString codecname = m_fileaudioformat.codec();
//    if(codecname.isEmpty()) codecname = "unknown type";
//    str += "Codec: "+codecname+"<br/>";
    if(m_fileaudioformat.channelCount()>1){
        if(m_channelid>0)         str += "Channel: "+QString::number(m_channelid)+"/"+QString::number(m_fileaudioformat.channelCount())+"<br/>";
        else if(m_channelid==-2)  str += "Channel: "+QString::number(m_fileaudioformat.channelCount())+" summed<br/>";
    }
    str += "Sampling: "+QString::number(fs)+"Hz<br/>";
    if(m_fileaudioformat.sampleSize()!=-1) {
        str += "Sample type: "+QString::number(m_fileaudioformat.sampleSize())+"b ";
        QAudioFormat::SampleType sampletype = m_fileaudioformat.sampleType();
        if(sampletype==QAudioFormat::Unknown)
            str += "(unknown type)";
        else if(sampletype==QAudioFormat::SignedInt)
            str += "signed integer";
        else if(sampletype==QAudioFormat::UnSignedInt)
            str += "unsigned interger";
        else if(sampletype==QAudioFormat::Float)
            str += "float";
        str += "<br/>";
        str += "SQNR="+QString::number(20*std::log10(std::pow(2,m_fileaudioformat.sampleSize())))+"dB<br/>";
//        if(sampletype!=QAudioFormat::Unknown) {
//            double smallest=1.0;
//            if(sampletype==QAudioFormat::SignedInt)
//                smallest = 1.0/std::pow(2.0,m_fileaudioformat.sampleSize()-1);
//            else if(sampletype==QAudioFormat::UnSignedInt)
//                smallest = 2.0/std::pow(2.0,m_fileaudioformat.sampleSize());
//            else if(sampletype==QAudioFormat::Float) {
//                if(m_fileaudioformat.sampleSize()==8*sizeof(float))
//                    smallest = std::numeric_limits<float>::min();
//                else if(m_fileaudioformat.sampleSize()==8*sizeof(double))
//                    smallest = std::numeric_limits<double>::min();
//                else if(m_fileaudioformat.sampleSize()==8*sizeof(long double))
//                    smallest = std::numeric_limits<long double>::min();
//            }
//            if(smallest!=1.0)
//                str += "Smallest amplitude: "+QString::number(20*log10(smallest))+"dB";
//        }
    }

    return str;
}

void FTSound::setAvoidClicksWindowDuration(double halfduration) {
    sm_avoidclickswindow = sigproc::hann(2*int(2*halfduration*fs_common/2)+1); // Use 50ms half-windows on each side
    double winmax = sm_avoidclickswindow[(sm_avoidclickswindow.size()-1)/2];
    for(size_t n=0; n<sm_avoidclickswindow.size(); n++)
        sm_avoidclickswindow[n] /= winmax;
}

void FTSound::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {

    FileType::fillContextMenu(contextmenu, mainwindow);

    connect(m_actionReload, SIGNAL(triggered()), mainwindow, SLOT(soundsChanged()));

    contextmenu.setTitle("Sound");

    contextmenu.addAction(mainwindow->ui->actionPlay);
    contextmenu.addAction(m_actionResetFiltering);
    contextmenu.addAction(m_actionInvPolarity);
    connect(m_actionInvPolarity, SIGNAL(toggled(bool)), mainwindow, SLOT(soundsChanged()));
    m_actionResetAmpScale->setText(QString("Reset amplitude scaling (%1dB) to 0dB").arg(20*log10(m_ampscale), 0, 'g', 3));
    m_actionResetAmpScale->setDisabled(m_ampscale==1.0);
    contextmenu.addAction(m_actionResetAmpScale);
    connect(m_actionResetAmpScale, SIGNAL(triggered()), this, SLOT(resetAmpScale()));
    m_actionResetDelay->setText(QString("Reset delay (%1s) to 0s").arg(m_delay/mainwindow->getFs(), 0, 'g', 3));
    m_actionResetDelay->setDisabled(m_delay==0);
    contextmenu.addAction(m_actionResetDelay);
    connect(m_actionResetDelay, SIGNAL(triggered()), this, SLOT(resetDelay()));
}

void FTSound::setFiltered(bool filtered){
    m_isfiltered = filtered;
    setStatus();
}

void FTSound::resetFiltering(){
    wavtoplay = &wav;
    WMainWindow::getMW()->m_gvWaveform->m_giFilteredSelection->hide();
//    WMainWindow::getMW()->m_gvWaveform->m_scene->invalidate();
    WMainWindow::getMW()->m_gvSpectrum->computeDFTs();
//    WMainWindow::getMW()->m_gvSpectrum->m_scene->invalidate();
    WMainWindow::getMW()->m_gvSpectrum->m_filterresponse.clear();
    setFiltered(false);
    m_filteredmaxamp = 0.0;
    updateClippedState();
}

void FTSound::resetAmpScale(){
    m_ampscale = 1.0;

    setStatus();

    WMainWindow::getMW()->soundsChanged();
    WMainWindow::getMW()->fileInfoUpdate();
}
void FTSound::resetDelay(){
    m_delay = 0.0;

    setStatus();

    WMainWindow::getMW()->soundsChanged();
    WMainWindow::getMW()->fileInfoUpdate();
}

double FTSound::getLastSampleTime() const {
    return (wav.size()-1)/fs;
}
bool FTSound::isModified() {
    return m_delay!=0.0 || m_ampscale!=1.0;
}

void FTSound::setStatus(){
    FileType::setStatus();

    updateClippedState();
}
void FTSound::updateClippedState(){
    m_isclipped = (m_wavmaxamp*m_ampscale>1.0) || (m_isfiltered && (m_filteredmaxamp*m_ampscale>1.0));
    if(m_isclipped)
        setBackgroundColor(QColor(255,0,0));
    else if(m_isfiltered)
        setBackgroundColor(QColor(255,192,192));
    else
        setBackgroundColor(QColor(255,255,255));

    WMainWindow::getMW()->fileInfoUpdate();
}

void FTSound::setSamplingRate(double _fs){

    fs = _fs;

    // Check if fs is the same for all files
    if(fs_common==0) {
        // The system has no defined sampling rate
        fs_common = fs;
        FTSound::setAvoidClicksWindowDuration(WMainWindow::getMW()->m_dlgSettings->ui->sbAvoidClicksWindowDuration->value());
    }
    else {
        // Check if fs is the same as that of the other files
        if(fs_common!=fs)
            throw QString("The sampling rate is not the same as that of the files already loaded.");
    }
}

double FTSound::setPlay(const QAudioFormat& format, double tstart, double tstop, double fstart, double fstop)
{
//    cout << "FTSound::setPlay" << endl;

    m_outputaudioformat = format;

    s_play_power = 0;
    s_play_power_values.clear();
    m_avoidclickswinpos = 0;

    // Fix and make time selection
    if(tstart>tstop){
        double tmp = tstop;
        tstop = tstart;
        tstart = tmp;
    }

    if(tstart==0.0 && tstop==0.0){
        m_start = 0;
        m_pos = m_start;
        m_end = wav.size()-1;
    }
    else{
        m_start = int(0.5+tstart*fs);
        m_pos = m_start;
        m_end = int(0.5+tstop*fs);
    }

    if(m_start<0) m_start=0;
    if(m_start>qint64(wav.size()-1)) m_start=wav.size()-1;
    if(m_end<0) m_end=0;
    if(m_end>qint64(wav.size()-1)) m_end=wav.size()-1;

    int delayedstart = m_start-m_delay;
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size()-1)) delayedstart=wavtoplay->size()-1;
    int delayedend = m_end-m_delay;
    if(delayedend<0) delayedend=0;
    if(delayedend>int(wavtoplay->size()-1)) delayedend=wavtoplay->size()-1;

    // Fix frequency cutoffs
    if(fstart>fstop){
        double tmp = fstop;
        fstop = fstart;
        fstart = tmp;
    }

    // Cannot ensure the numerical stability for very small cutoff
    // Thus, clip the given values
    if(fstart<10) fstart=0;
    if(fstart>fs/2-10) fstart=fs/2;
    if(fstop<10) fstop=0;
    if(fstop>fs/2-10) fstop=fs/2;

    bool doLowPass = fstop>0.0 && fstop<fs/2;
    bool doHighPass = fstart>0.0 && fstart<fs/2;

    if ((fstart<fstop) && (doLowPass || doHighPass)) {
        try{
            wavfiltered = wav;

            // Compute the energy of the non-filtered signal
            double enerwav = 0.0;
            if(WMainWindow::getMW()->m_dlgSettings->ui->cbFilteringCompensateEnergy->isChecked()){
                for(int n=delayedstart; n<=delayedend; n++)
                    enerwav += wav[n]*wav[n];
                enerwav = std::sqrt(enerwav);
            }

            int butterworth_order = WMainWindow::getMW()->m_dlgSettings->ui->sbButterworthOrder->value();
            WMainWindow::getMW()->m_gvSpectrum->m_filterresponse = std::vector<FFTTYPE>(BUTTERRESPONSEDFTLEN/2+1,1.0);
            std::vector< std::vector<double> > num, den;
            std::vector<double> filterresponse;

            if (doLowPass) {
                // Compute the Butterworth filter coefficients
                mkfilter::make_butterworth_filter_biquad(butterworth_order, fstop/fs, true, num, den, &filterresponse, BUTTERRESPONSEDFTLEN);

                // Update the filter response
                for(size_t k=0; k<filterresponse.size(); k++){
                    if(filterresponse[k] < 2*std::numeric_limits<FFTTYPE>::min())
                        filterresponse[k] = std::numeric_limits<FFTTYPE>::min();
                    WMainWindow::getMW()->m_gvSpectrum->m_filterresponse[k] *= filterresponse[k];
                }

                WMainWindow::getMW()->m_globalWaitingBarLabel->setText(QString("Low-pass filtering (cutoff=")+QString::number(fstop)+"Hz)");
                WMainWindow::getMW()->m_globalWaitingBarLabel->show();
                WMainWindow::getMW()->m_globalWaitingBar->setMinimum(0);
                WMainWindow::getMW()->m_globalWaitingBar->setMaximum(0);
                WMainWindow::getMW()->m_globalWaitingBar->show();
                WMainWindow::getMW()->ui->statusBar->repaint();

                cout << "LP-filtering (cutoff=" << fstop << ", size=" << wavfiltered.size() << ")" << endl;
                // Filter the signal
                for(size_t bi=0; bi<num.size(); bi++)
                    sigproc::filtfilt<WAVTYPE>(wavfiltered, num[bi], den[bi], wavfiltered, delayedstart, delayedend);

                WMainWindow::getMW()->m_globalWaitingBarLabel->hide();
                WMainWindow::getMW()->m_globalWaitingBar->hide();
            }

            if (doHighPass) {
                // Compute the Butterworth filter coefficients
                mkfilter::make_butterworth_filter_biquad(butterworth_order, fstart/fs, false, num, den, &filterresponse, BUTTERRESPONSEDFTLEN);

                // Update the filter response
                for(size_t k=0; k<filterresponse.size(); k++){
                    if(filterresponse[k] < 2*std::numeric_limits<FFTTYPE>::min())
                        filterresponse[k] = std::numeric_limits<FFTTYPE>::min();
                    WMainWindow::getMW()->m_gvSpectrum->m_filterresponse[k] *= filterresponse[k];
                }

                WMainWindow::getMW()->m_globalWaitingBarLabel->setText(QString("High-pass filtering (cutoff=")+QString::number(fstart)+"Hz)");
                WMainWindow::getMW()->m_globalWaitingBarLabel->show();
                WMainWindow::getMW()->m_globalWaitingBar->setMinimum(0);
                WMainWindow::getMW()->m_globalWaitingBar->setMaximum(0);
                WMainWindow::getMW()->m_globalWaitingBar->show();
                WMainWindow::getMW()->ui->statusBar->repaint();

                cout << "HP-filtering (cutoff=" << fstart << ", size=" << wavfiltered.size() << ")" << endl;

                // Filter the signal
                for(size_t bi=0; bi<num.size(); bi++)
                    sigproc::filtfilt<WAVTYPE>(wavfiltered, num[bi], den[bi], wavfiltered, delayedstart, delayedend);

                WMainWindow::getMW()->m_globalWaitingBarLabel->hide();
                WMainWindow::getMW()->m_globalWaitingBar->hide();
            }

            if(WMainWindow::getMW()->m_dlgSettings->ui->cbFilteringCompensateEnergy->isChecked()){
                // Compute the energy of the filtered signal ...
                double enerfilt = 0.0;
                for(int n=delayedstart; n<=delayedend; n++)
                    enerfilt += wavfiltered[n]*wavfiltered[n];
                enerfilt = std::sqrt(enerfilt);

                // ... and equalize the energy with the non-filtered signal
                enerwav = enerwav/enerfilt; // Pre-compute the ratio
                m_filteredmaxamp = 0.0;
                for(int n=delayedstart; n<=delayedend; n++){
                    wavfiltered[n] *= enerwav;
                    m_filteredmaxamp = std::max(m_filteredmaxamp, std::abs(wavfiltered[n]));
                }
            }
            updateClippedState();

            // Convert to dB and multiply by 2 bcs of the filtfilt
            for(size_t k=0; k<WMainWindow::getMW()->m_gvSpectrum->m_filterresponse.size(); k++)
                WMainWindow::getMW()->m_gvSpectrum->m_filterresponse[k] = 2*20*log10(WMainWindow::getMW()->m_gvSpectrum->m_filterresponse[k]);
            WMainWindow::getMW()->m_gvSpectrum->m_scene->invalidate();

            // It seems the filtering went well, we can use the filtered sound
            wavtoplay = &wavfiltered;

            WMainWindow::getMW()->m_gvWaveform->m_scene->invalidate();
            WMainWindow::getMW()->m_gvSpectrum->computeDFTs();
            WMainWindow::getMW()->m_gvSpectrum->m_scene->invalidate();
        }
        catch(QString err){
            QMessageBox::warning(NULL, "Problem when filtering the sound to be played", QString("The sound cannot be filtered as given by the selection in the spectrum view.\n\nReason:\n")+err);
//            wavtoplay = &wav;
            throw QString("Problem when filtering the sound to be played");
        }
    }
    else {
        WMainWindow::getMW()->m_gvSpectrum->m_filterresponse.resize(0);
        WMainWindow::getMW()->m_gvSpectrum->m_scene->invalidate();
        bool dofrefresh = wavtoplay != &wav;
        wavtoplay = &wav;
        if (dofrefresh) {
            WMainWindow::getMW()->m_gvWaveform->m_scene->invalidate();
            WMainWindow::getMW()->m_gvSpectrum->computeDFTs();
            WMainWindow::getMW()->m_gvSpectrum->m_scene->invalidate();
        }
    }

    QIODevice::open(QIODevice::ReadOnly);

    double tobeplayed = double(m_end-m_pos+1)/fs;

//    std::cout << "DSSound::start [" << tstart << "s(" << m_pos << "), " << tstop << "s(" << m_end << ")] " << tobeplayed << "s" << endl;
//    cout << "FTSound::~setPlay" << endl;

    return tobeplayed;
}

void FTSound::stop()
{
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;
    QIODevice::close();
}

qint64 FTSound::readData(char *data, qint64 askedlen)
{
//    std::cout << "DSSound::readData requested=" << askedlen << endl;

    qint64 writtenbytes = 0; // [bytes]

    const int channelBytes = m_outputaudioformat.sampleSize() / 8;
    unsigned char *ptr = reinterpret_cast<unsigned char *>(data);
    int delayedstart = m_start-m_delay;
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size()-1)) delayedstart=wavtoplay->size()-1;
    int delayedend = m_end-m_delay;
    if(delayedend<0) delayedend=0;
    if(delayedend>int(wavtoplay->size()-1)) delayedend=wavtoplay->size()-1;

    // Polarity apparently matters in very particular cases
    // so take it into account when playing.
    double gain = m_ampscale;
    if(m_actionInvPolarity->isChecked())
        gain *= -1;

    // Write as many bits has requested by the call
    while(writtenbytes<askedlen) {
        qint16 value = 0;

        if(sm_playwin_use && (m_avoidclickswinpos<qint64(sm_avoidclickswindow.size()-1)/2)) {
            value = qint16((gain*(*wavtoplay)[delayedstart]*sm_avoidclickswindow[m_avoidclickswinpos++])*32767);
        }
        else if(sm_playwin_use && (m_pos>m_end) && m_avoidclickswinpos<qint64(sm_avoidclickswindow.size()-1)) {
            value = qint16((gain*(*wavtoplay)[delayedend]*sm_avoidclickswindow[1+m_avoidclickswinpos++])*32767);
        }
        else if (m_pos<=m_end) {
            int delayedpos = m_pos - m_delay;
            if(delayedpos>=0 && delayedpos<int(wavtoplay->size())){
        //        WAVTYPE e = samples[m_pos]*samples[m_pos];
        //        s_play_power += e;
                WAVTYPE e = abs(gain*(*wavtoplay)[delayedpos]);
                s_play_power_values.push_front(e);
                while(s_play_power_values.size()/fs>0.1){
                    s_play_power -= s_play_power_values.back();
                    s_play_power_values.pop_back();
                }

        //        cout << 20*log10(sqrt(s_play_power/s_play_power_values.size())) << endl;

                // Assuming the output audio device has been open in 16bits ...
                // TODO Manage more output formats
                WAVTYPE w = gain*(*wavtoplay)[delayedpos];

                if(w>1.0)       w = 1.0;
                else if(w<-1.0) w = -1.0;

                value=qint16(w*32767);
            }

            m_pos++;
        }

        qToLittleEndian<qint16>(value, ptr);
        ptr += channelBytes;
        writtenbytes += channelBytes;
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

qint64 FTSound::writeData(const char *data, qint64 askedlen){

    Q_UNUSED(data)
    Q_UNUSED(askedlen)

    throw QString("DSSound::writeData: There is no reason to call this function.");

    return 0;
}

FTSound::~FTSound(){
    QIODevice::close();
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
