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
#include <QProgressDialog>
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrum.h"
#include "gvwaveform.h"
#include "qaesigproc.h"
#include "qaehelpers.h"

#include "../external/libqaudioextra/external/mkfilter/mkfilter.h"

#include "ui_wdialogsettings.h"
#include "gvspectrogram.h"
#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"


bool FTSound::s_playwin_use = false;
std::vector<WAVTYPE> FTSound::s_avoidclickswindow;

double FTSound::s_fs_common = 0; // Initially, fs is undefined
WAVTYPE FTSound::s_play_power = 0;
std::deque<WAVTYPE> FTSound::s_play_power_values;


bool FTSound::WavParameters::operator==(const WavParameters& param) const {
    if(winpixdelay!=param.winpixdelay)
        return false;
    if(delay!=param.delay)
        return false;
    if(gain!=param.gain)
        return false;
    if(wav!=param.wav)
        return false;
    if(fullpixrect!=param.fullpixrect)
        return false;
    if(viewrect!=param.viewrect)
        return false;
    if(lastreadtime!=param.lastreadtime)
        return false;

    return true;
}

FTSound::DFTParameters::DFTParameters(unsigned int _nl, unsigned int _nr, int _winlen, int _wintype, int _normtype, const std::vector<FFTTYPE>& _win, int _dftlen, std::vector<FFTTYPE>*_wav, qreal _ampscale, qint64 _delay){
    clear();

    nl = _nl;
    nr = _nr;
    winlen = _winlen;
    wintype = _wintype;
    normtype = _normtype;
    win = _win;
    dftlen = _dftlen;

    wav = _wav;
    ampscale = _ampscale;
    delay = _delay;
}

FTSound::DFTParameters& FTSound::DFTParameters::operator=(const DFTParameters &params) {
    nl = params.nl;
    nr = params.nr;
    if(params.wintype>7      // TODO Replace with a Shape Parameters structure
       || !(wintype==params.wintype
            && normtype==params.normtype
            && winlen==params.winlen))
        win = params.win;
    winlen = params.winlen;
    wintype = params.wintype;
    normtype = params.normtype;
    dftlen = params.dftlen;

    wav = params.wav;
    ampscale = params.ampscale;
    delay = params.delay;

    return *this;
}


bool FTSound::DFTParameters::operator==(const DFTParameters& param) const {
    if(wav!=param.wav)
        return false;
    if(nl!=param.nl)
        return false;
    if(nr!=param.nr)
        return false;
    if(winlen!=param.winlen)
        return false;
    if(wintype!=param.wintype)
        return false;
    if(normtype!=param.normtype)
        return false;
    if(dftlen!=param.dftlen)
        return false;
    if(wintype>7){ // If this is a parametrizable window, check every sample // TODO 7
        if(win.size()!=param.win.size())
            return false;
        for(size_t n=0; n<win.size(); n++)
            if(win[n]!=param.win[n])
                return false;
    }

    return true;
}


void FTSound::constructor_internal() {
    connect(m_actionShow, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));

    m_channelid = 0;
    m_isclipped = false;
    m_isfiltered = false;
    m_isplaying = false;
    wavtoplay  = &wav;
    m_filteredmaxamp = 0.0;
    m_ampscale = 1.0;
    m_delay = 0;
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;

    m_stft_min = std::numeric_limits<FFTTYPE>::infinity();
    m_stft_max = -std::numeric_limits<FFTTYPE>::infinity();

    m_actionInvPolarity = new QAction("Inverse polarity", this);
    m_actionInvPolarity->setStatusTip(tr("Inverse the polarity of the sound"));
    m_actionInvPolarity->setCheckable(true);
    m_actionInvPolarity->setChecked(false);
    connect(m_actionInvPolarity, SIGNAL(triggered()), this, SLOT(needDFTUpdate()));

    m_actionResetAmpScale = new QAction("Reset amplitude", this);
    m_actionResetAmpScale->setStatusTip(tr("Reset the amplitude scaling to 1"));
    connect(m_actionResetAmpScale, SIGNAL(triggered()), this, SLOT(needDFTUpdate()));
    connect(m_actionResetAmpScale, SIGNAL(triggered()), this, SLOT(resetAmpScale()));

    m_actionResetDelay = new QAction("Reset the delay", this);
    m_actionResetDelay->setStatusTip(tr("Reset the delay to 0s"));
    connect(m_actionResetDelay, SIGNAL(triggered()), this, SLOT(needDFTUpdate()));
    connect(m_actionResetDelay, SIGNAL(triggered()), this, SLOT(resetDelay()));

    m_actionResetFiltering = new QAction("Remove filtering effects", this);
    m_actionResetFiltering->setStatusTip(tr("Reset to original signal without filtering effects"));
    connect(m_actionResetFiltering, SIGNAL(triggered()), this, SLOT(needDFTUpdate()));
    connect(m_actionResetFiltering, SIGNAL(triggered()), this, SLOT(resetFiltering()));
}

void FTSound::constructor_external() {
    FileType::constructor_external();

    connect(m_actionInvPolarity, SIGNAL(triggered()), gMW, SLOT(allSoundsChanged()));
    connect(m_actionResetAmpScale, SIGNAL(triggered()), gFL, SLOT(fileInfoUpdate()));
    connect(m_actionResetDelay, SIGNAL(triggered()), gFL, SLOT(fileInfoUpdate()));

    gFL->ftsnds.push_back(this);

    m_giWaveform = new GraphicItemWaveform(this);
    gMW->m_gvWaveform->m_scene->addItem(m_giWaveform);
}

FTSound::FTSound(const QString& _fileName, QObject *parent, int channelid)
    : QIODevice(parent)
    , FileType(FTSOUND, _fileName, this)
{
    FTSound::constructor_internal();

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        try{
            load(channelid);
            load_finalize();
        }
        catch(std::bad_alloc err){
            QMessageBox::critical(NULL, "Memory full!", "There is not enough free memory for loading this file.");
            throw QString("Memory cannot hold this file!");
        }
    }
    FTSound::constructor_external();

//    QIODevice::open(QIODevice::ReadOnly);
}

FTSound::FTSound(const FTSound& ft)
    : QIODevice(ft.parent())
    , FileType(FTSOUND, ft.fileFullPath, this)
{
    FTSound::constructor_internal();

    QFileInfo fileInfo(fileFullPath);
    setText(fileInfo.fileName());
//    COUTD << fileInfo.fileName().toLatin1().constData() << " (" << text().toLatin1().constData() << ")" << endl;

    wav = ft.wav;
    m_wavmaxamp = ft.m_wavmaxamp;
    fs = ft.fs;
    m_fileaudioformat.setSampleRate(fs);
    m_fileaudioformat.setSampleType(QAudioFormat::Float);
    m_fileaudioformat.setSampleSize(8*sizeof(WAVTYPE));

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    FTSound::constructor_external();
}

void FTSound::load_finalize() {
    m_wavmaxamp = 0.0;
    for(unsigned int n=0; n<wav.size(); ++n)
        m_wavmaxamp = std::max(m_wavmaxamp, abs(wav[n]));

    if(s_avoidclickswindow.size()==0)
        FTSound::setAvoidClicksWindowDuration(gMW->m_dlgSettings->ui->sbPlaybackAvoidClicksWindowDuration->value());

//    std::cout << "INFO: " << wav.size() << " samples loaded (" << wav.size()/fs << "s max amplitude=" << m_wavmaxamp << ")" << endl;

    m_lastreadtime = QDateTime::currentDateTime();
    needDFTUpdate();
    setStatus();
}

void FTSound::setVisible(bool shown){
    FileType::setVisible(shown);
    if(!shown)
        m_wavparams.clear();
}

void FTSound::setDrawIcon(QPixmap& pm){
    FileType::setDrawIcon(pm);
    if(isPlaying()){
        // If playing, draw an arrow in the icon
        QPainter p(&pm);
        QPolygon poly(3);
        poly.setPoint(0, 6,6);
        poly.setPoint(1, 26,16);
        poly.setPoint(2, 6,26);
        p.setBrush(Qt::white);
        p.setPen(Qt::black);
        p.drawConvexPolygon(poly);
    }
}

bool FTSound::reload() {
//    COUTD << "FTSound::reload" << endl;

    stopPlay();
    gMW->m_gvSpectrogram->m_stftcomputethread->cancelComputation(this);

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return false;

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
    m_stft.clear();
    m_stftts.clear();
    m_stftparams.clear();

    // ... and reload the data from the file
    try{
        load();
        load_finalize();
    }
    catch(std::bad_alloc err){
        QMessageBox::critical(NULL, "Memory full!", "There is not enough free memory for computing this STFT");

        throw QString("Memory cannot hold this file!");
    }

//    COUTD << "FTSound::~reload" << endl;
    return true;
}

FileType* FTSound::duplicate(){
    return new FTSound(*this);
}

QString FTSound::info() const {
    QString str = FileType::info();

    str += "Duration: "+QString::number(getDuration())+"s ("+QString::number(wav.size())+")<br/>";

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
        str += "SQNR="+QString::number(20*std::log10(std::pow(2.0,m_fileaudioformat.sampleSize())))+"dB<br/>";
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

    if(m_ampscale!=1.0)
        str += "<b>Scaled: "+QString::number(20*std::log10(m_ampscale), 'f', 4)+"dB ("+QString::number(m_ampscale, 'f', 4)+")</b><br/>";
    if(isClipped())
        str += "<font color=\"red\"><b>CLIPPED</b></font><br/>";
    if(m_delay!=0.0)
        str += "<b>Delayed: "+QString("%1").arg(double(m_delay)/fs, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())+"s ("+QString::number(m_delay)+")</b><br/>";

    return str;
}

void FTSound::setAvoidClicksWindowDuration(double halfduration) {
    s_avoidclickswindow = qae::hann(2*int(2*halfduration*s_fs_common/2)+1); // Use Xms half-windows on each side
    double winmax = s_avoidclickswindow[(s_avoidclickswindow.size()-1)/2];
    for(size_t n=0; n<s_avoidclickswindow.size(); n++)
        s_avoidclickswindow[n] /= winmax;
}

void FTSound::fillContextMenu(QMenu& contextmenu) {

    FileType::fillContextMenu(contextmenu);

    contextmenu.setTitle("Sound");

    contextmenu.addAction(gMW->ui->actionPlay);
    contextmenu.addAction(m_actionResetFiltering);
    contextmenu.addAction(m_actionInvPolarity);
    m_actionResetAmpScale->setText(QString("Reset amplitude scaling (%1dB) to 0dB").arg(20*log10(m_ampscale), 0, 'g', 3));
    m_actionResetAmpScale->setDisabled(m_ampscale==1.0);
    contextmenu.addAction(m_actionResetAmpScale);
    m_actionResetDelay->setText(QString("Reset delay (%1s) to 0s").arg(m_delay/gFL->getFs(), 0, 'g', gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()));
    m_actionResetDelay->setDisabled(m_delay==0);
    contextmenu.addAction(m_actionResetDelay);

    contextmenu.addSeparator();
    contextmenu.addAction(gMW->ui->actionEstimationF0);
}

void FTSound::needDFTUpdate() {
    m_stftparams.clear();
}

void FTSound::setFiltered(bool filtered){
    m_isfiltered = filtered;
    setStatus();
}

void FTSound::resetFiltering(){
    wavtoplay = &wav;
    gMW->m_gvWaveform->m_giFilteredSelection->hide();
    gMW->m_gvAmplitudeSpectrum->updateDFTs();
    gMW->m_gvAmplitudeSpectrum->m_filterresponse.clear();
    setFiltered(false);
    m_filteredmaxamp = 0.0;
    updateClippedState();
}

void FTSound::resetAmpScale(){
    if(m_ampscale!=1.0){
        m_ampscale = 1.0;

        setStatus();

        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvAmplitudeSpectrum->updateDFTs();
        gMW->ui->pbSpectrogramSTFTUpdate->show();
        if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
            gMW->m_gvSpectrogram->updateSTFTSettings();
    }
}
void FTSound::resetDelay(){
    if(m_delay!=0.0){
        m_delay = 0.0;

        setStatus();

        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvAmplitudeSpectrum->updateDFTs();
        gMW->ui->pbSpectrogramSTFTUpdate->show();
        if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
            gMW->m_gvSpectrogram->updateSTFTSettings();
    }
}

double FTSound::getLastSampleTime() const {
    return (wav.size()-1+m_delay)/fs;
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
}

void FTSound::setSamplingRate(double _fs){

    fs = _fs;

    // Check if fs is the same for all files
    if(s_fs_common==0) {
        // The system has no defined sampling rate
        s_fs_common = fs;
        FTSound::setAvoidClicksWindowDuration(gMW->m_dlgSettings->ui->sbPlaybackAvoidClicksWindowDuration->value());
    }
    else {
        // Check if fs is the same as that of the other files
        if(s_fs_common!=fs){
            throw QString("The sampling rate of this file ("+QString::number(fs)+"Hz) is not the same as that of the files already loaded. DFasma manages only one sampling rate per instance. Please use another instance of DFasma.");
        }
    }
}

double FTSound::setPlay(const QAudioFormat& format, double tstart, double tstop, double fstart, double fstop)
{
//    cout << "FTSound::setPlay" << endl;

    m_outputaudioformat = format;

    m_isplaying = true;

    // Draw an arrow in the icon
    updateIcon();

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
    if(m_start>qint64(wav.size()-1)+m_delay) m_start=wav.size()-1+m_delay;
    if(m_end<0) m_end=0;
    if(m_end>qint64(wav.size()-1)+m_delay) m_end=wav.size()-1+m_delay;

    int delayedstart = m_start-m_delay;
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size())-1) delayedstart=int(wavtoplay->size())-1;
    int delayedend = m_end-m_delay;
    if(delayedend<0) delayedend=0;
    if(delayedend>int(wavtoplay->size())-1) delayedend=int(wavtoplay->size())-1;

    // Fix frequency cutoffs
    if(fstart>fstop){
        double tmp = fstop;
        fstop = fstart;
        fstart = tmp;
    }

    // Cannot ensure the numerical stability for very low or very high cutoff
    // Thus, clip the given values
    if(fstart<10) fstart=0;
    if(fstart>fs/2-10) fstart=fs/2;
    if(fstop<10) fstop=0;
    if(fstop>fs/2-10) fstop=fs/2;

    bool doLowPass = fstop>0.0 && fstop<fs/2;
    bool doHighPass = fstart>0.0 && fstart<fs/2;

    if ((fstart<fstop) && (doLowPass || doHighPass)) {
        try{
            wavfiltered = wav; // Is it acceptable for big files ? Reason of issue #117 also ?

            // Compute the energy of the non-filtered signal
            double enerwav = 0.0;
            if(gMW->m_dlgSettings->ui->cbPlaybackFilteringCompensateEnergy->isChecked()){
                for(int n=delayedstart; n<=delayedend; n++)
                    enerwav += wav[n]*wav[n];
                enerwav = std::sqrt(enerwav);
            }

            int butterworth_order = gMW->m_dlgSettings->ui->sbPlaybackButterworthOrder->value();
            gMW->m_gvAmplitudeSpectrum->m_filterresponse = std::vector<FFTTYPE>(BUTTERRESPONSEDFTLEN/2+1,1.0);
            std::vector< std::vector<double> > num, den;
            std::vector<double> filterresponse;

            if (doLowPass) {
                // Compute the Butterworth filter coefficients
                mkfilter::make_butterworth_filter_biquad(butterworth_order, fstop/fs, true, num, den, &filterresponse, BUTTERRESPONSEDFTLEN);

                // Update the filter response
                for(size_t k=0; k<filterresponse.size(); k++){
                    if(filterresponse[k] < 2*std::numeric_limits<FFTTYPE>::min())
                        filterresponse[k] = std::numeric_limits<FFTTYPE>::min();
                    gMW->m_gvAmplitudeSpectrum->m_filterresponse[k] *= filterresponse[k];
                }

                gMW->globalWaitingBarMessage(QString("Low-pass filtering (cutoff=")+QString::number(fstop)+"Hz)");

                cout << "LP-filtering (cutoff=" << fstop << ", size=" << wavfiltered.size() << ")" << endl;
                // Filter the signal
                for(size_t bi=0; bi<num.size(); bi++)
                    qae::filtfilt<WAVTYPE>(wavfiltered, num[bi], den[bi], wavfiltered, delayedstart, delayedend);

                gMW->globalWaitingBarClear();
            }

            if (doHighPass) {
                // Compute the Butterworth filter coefficients
                mkfilter::make_butterworth_filter_biquad(butterworth_order, fstart/fs, false, num, den, &filterresponse, BUTTERRESPONSEDFTLEN);

                // Update the filter response
                for(size_t k=0; k<filterresponse.size(); k++){
                    if(filterresponse[k] < 2*std::numeric_limits<FFTTYPE>::min())
                        filterresponse[k] = std::numeric_limits<FFTTYPE>::min();
                    gMW->m_gvAmplitudeSpectrum->m_filterresponse[k] *= filterresponse[k];
                }

                gMW->globalWaitingBarMessage(QString("Low-pass filtering (cutoff=")+QString::number(fstop)+"Hz)");

                cout << "HP-filtering (cutoff=" << fstart << ", size=" << wavfiltered.size() << ")" << endl;

                // Filter the signal
                for(size_t bi=0; bi<num.size(); bi++)
                    qae::filtfilt<WAVTYPE>(wavfiltered, num[bi], den[bi], wavfiltered, delayedstart, delayedend);

                gMW->globalWaitingBarClear();
            }

            if(gMW->m_dlgSettings->ui->cbPlaybackFilteringCompensateEnergy->isChecked()){
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
            needDFTUpdate();
            updateClippedState();

            // The filter response has been computed here above.
            // Convert it to dB and multiply by 2 bcs the filtfilt doubled the effect.
            for(size_t k=0; k<gMW->m_gvAmplitudeSpectrum->m_filterresponse.size(); k++)
                gMW->m_gvAmplitudeSpectrum->m_filterresponse[k] = 2*20*log10(gMW->m_gvAmplitudeSpectrum->m_filterresponse[k]);
            gMW->m_gvAmplitudeSpectrum->m_scene->invalidate();

            // It seems the filtering went well, we can use the filtered sound
            wavtoplay = &wavfiltered;

//            gMW->m_gvWaveform->m_scene->update();
            gMW->m_gvAmplitudeSpectrum->updateDFTs();
            gMW->m_gvAmplitudeSpectrum->m_scene->update();
        }
        catch(QString err){
            m_isplaying = false;
            updateIcon();
            QMessageBox::warning(NULL, "Problem when filtering the sound to be played", QString("The sound cannot be filtered as given by the selection in the spectrum view.\n\nReason:\n")+err);
//            wavtoplay = &wav;
            throw QString("Problem when filtering the sound to be played");
        }
    }
    else {
        gMW->m_gvAmplitudeSpectrum->m_filterresponse.resize(0);
        wavtoplay = &wav;
        gMW->m_gvAmplitudeSpectrum->updateDFTs();
    }

    QIODevice::open(QIODevice::ReadOnly);

    double tobeplayed = double(m_end-m_pos+1)/fs;

//    std::cout << "DSSound::start [" << tstart << "s(" << m_pos << "), " << tstop << "s(" << m_end << ")] " << tobeplayed << "s" << endl;
//    cout << "FTSound::~setPlay" << endl;

    return tobeplayed;
}

void FTSound::stopPlay()
{
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;
    QIODevice::close();
    m_isplaying = false;
    updateIcon();
}

qint64 FTSound::readData(char *data, qint64 askedlen)
{
//    std::cout << "DSSound::readData requested=" << askedlen << endl;

    qint64 writtenbytes = 0; // [bytes]

    const int channelBytes = m_outputaudioformat.sampleSize() / 8;
    unsigned char *ptr = reinterpret_cast<unsigned char *>(data);
    int delayedstart = m_start-m_delay;
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size()-1)) delayedstart=int(wavtoplay->size())-1;
    int delayedend = m_end-m_delay;
    if(delayedend<0) delayedend=0;
    if(delayedend>int(wavtoplay->size()-1)) delayedend=int(wavtoplay->size())-1;

    // Polarity apparently matters in very particular cases
    // so take it into account when playing.
    double gain = m_ampscale;
    if(m_actionInvPolarity->isChecked())
        gain *= -1;

    // Write as many bits has requested by the call
    while(writtenbytes<askedlen) {
        qint16 value = 0;

        if(s_playwin_use && (m_avoidclickswinpos<qint64(s_avoidclickswindow.size()-1)/2)) {
            value = qint16((gain*(*wavtoplay)[delayedstart]*s_avoidclickswindow[m_avoidclickswinpos++])*32767);
        }
        else if(s_playwin_use && (m_pos>m_end) && m_avoidclickswinpos<qint64(s_avoidclickswindow.size()-1)) {
            value = qint16((gain*(*wavtoplay)[delayedend]*s_avoidclickswindow[1+m_avoidclickswinpos++])*32767);
        }
        else if (m_pos<=m_end) {
            int delayedpos = m_pos - m_delay;
            if(delayedpos>=0 && delayedpos<int(wavtoplay->size())){
        //        WAVTYPE e = samples[m_pos]*samples[m_pos];
                WAVTYPE e = abs(gain*(*wavtoplay)[delayedpos]);
//                s_play_power += e;
                s_play_power_values.push_front(e);
                while(s_play_power_values.size()/fs>1.0){ // Has to correspond to the delay between readData calls
//                    s_play_power -= s_play_power_values.back();
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
//        COUTD << "No more data to send to the sound device!" << endl;
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
    stopPlay();
    if(gMW->m_gvSpectrogram) gMW->m_gvSpectrogram->m_stftcomputethread->cancelComputation(this);
    QIODevice::close();

    gFL->ftsnds.erase(std::find(gFL->ftsnds.begin(), gFL->ftsnds.end(), this));
}


// -----------------------------------------------------------------------------

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

// GraphicItem for Waveform ----------------------------------------------------

FTSound::GraphicItemWaveform::GraphicItemWaveform(FTSound* snd)
    : m_snd(snd)
{
    // This cache mode is slower than using m_wavp{x|y}_min
    // And, exposedRect is not inconsistent with this cache mode enabled.
//     setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
//    setOpacity(0.5); // TODO ?
}

QRectF FTSound::GraphicItemWaveform::boundingRect() const {
    return QRectF(m_snd->m_delay/m_snd->fs, -1.0, m_snd->getLastSampleTime(), 2.0);
}

void FTSound::GraphicItemWaveform::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget) {
//    COUTD << "FTSound::GraphicItemWaveform::paint update_rect=" << option->exposedRect << endl;

//    void QGVWaveform::draw_waveform(QPainter* painter, const QRectF& rect, FTSound* snd){
    if(!m_snd->m_actionShow->isChecked() // TODO Need this ?
        || m_snd->wavtoplay->empty())
        return;

    painter->setClipRect(option->exposedRect);

    double fs = m_snd->fs;

    QRectF viewrect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
//    COUTD << "viewrect=" << viewrect << endl;
    QRectF rect = option->exposedRect;
//    COUTD << "rect to update=" << rect << endl;

    double samppixdensity = (viewrect.right()-viewrect.left())*fs/gMW->m_gvWaveform->viewport()->rect().width();
    // COUTD << samppixdensity << endl;

//    samppixdensity=3;
    if(samppixdensity<4) {
//        COUTD << "draw_waveform: Draw lines between each sample" << endl;

        WAVTYPE a = m_snd->m_ampscale;
        if(m_snd->m_actionInvPolarity->isChecked())
            a *=-1.0;

        QPen wavpen(m_snd->getColor());
        wavpen.setWidth(0);
        painter->setPen(wavpen);

        int delay = m_snd->m_delay;
        int nleft = int((rect.left())*fs)-delay;
        int nright = int((rect.right())*fs)+1-delay;
        nleft = std::max(nleft, -delay);
        nleft = std::max(nleft, 0);
        nleft = std::min(nleft, int(m_snd->wav.size()-1)-delay);
        nleft = std::min(nleft, int(m_snd->wav.size()-1));
        nright = std::max(nright, -delay);
        nright = std::max(nright, 0);
        nright = std::min(nright, int(m_snd->wav.size()-1)-delay);
        nright = std::min(nright, int(m_snd->wav.size()-1));

        // Draw a line between each sample value
        WAVTYPE dt = 1.0/fs;
        WAVTYPE prevx = (nleft+delay)*dt;
        a *= -1;
        WAVTYPE* data = m_snd->wavtoplay->data();
        WAVTYPE prevy = a*(*(data+nleft));

        WAVTYPE x, y;
        for(int n=nleft; n<=nright; ++n){
            x = (n+delay)*dt;
            y = a*(*(data+n));

            if(y>1.0)       y = 1.0;
            else if(y<-1.0) y = -1.0;

            painter->drawLine(QLineF(prevx, prevy, x, y));

            prevx = x;
            prevy = y;
        }

        // When resolution is big enough, draw tick marks at each sample
        double samppixdensity_dotsthr = 0.125;
        if(samppixdensity<samppixdensity_dotsthr){
            qreal markhalfheight = gMW->m_gvWaveform->m_ampzoom*(1.0/10)*((samppixdensity_dotsthr-samppixdensity)/samppixdensity_dotsthr);

            for(int n=nleft; n<=nright; n++){
                x = (n+delay)*dt;
                y = a*(*(data+n));
                painter->drawLine(QLineF(x, y-markhalfheight, x, y+markhalfheight));
            }
        }

        painter->drawLine(QLineF(double(m_snd->wav.size()-1)/fs, -1.0, double(m_snd->wav.size()-1)/fs, 1.0));
    }
    else {
//        COUTD << "draw_waveform: Plot only one line per pixel" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        double gain = m_snd->m_ampscale;
        if(m_snd->m_actionInvPolarity->isChecked())
            gain *= -1.0;

        QPen outlinePen(m_snd->getColor());
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        QRect fullpixrect = gMW->m_gvWaveform->mapFromScene(viewrect).boundingRect();
        QRect pixrect = gMW->m_gvWaveform->mapFromScene(rect).boundingRect();
        pixrect = pixrect.intersected(fullpixrect);

        double s2p = -(fullpixrect.height()-1)/viewrect.height(); // Scene to pixel
        double p2n = fs*double(viewrect.width())/double(fullpixrect.width()-1); // Pixel to scene
        double yzero = fullpixrect.height()/2;

        WAVTYPE* yp = m_snd->wavtoplay->data();

        int winpixdelay = gMW->m_gvWaveform->horizontalScrollBar()->value(); // - 1.0/p2n; // The magic value to make everything plot at the same place whatever the scroll

        int snddelay = m_snd->m_delay;

        FTSound::WavParameters reqparams(fullpixrect, viewrect, winpixdelay, m_snd);
        if(m_snd->wavtoplay==&(m_snd->wav) && reqparams==m_snd->m_wavparams){
//             COUTD << "Using existing buffer " << pixrect << endl;
            for(int i=pixrect.left(); i<=pixrect.right(); i++){
//            for(int i=0; i<=snd->m_wavpx_min.size(); i++)
                if(m_snd->m_wavpx_min[i]<1e10) // TODO Clean this dirty fix
                    painter->drawLine(QLineF(i, yzero+m_snd->m_wavpx_min[i], i, yzero+m_snd->m_wavpx_max[i]));
            }
        }
        else {
//            COUTD << "Re compute buffer and draw " << pixrect << endl;
            if(int(m_snd->m_wavpx_min.size())!=reqparams.fullpixrect.width()){
//                COUTD << "Resize" << endl;
                m_snd->m_wavpx_min.resize(reqparams.fullpixrect.width());
                m_snd->m_wavpx_max.resize(reqparams.fullpixrect.width());
                pixrect = reqparams.fullpixrect;
            }
            int ns = int((pixrect.left()+winpixdelay)*p2n)-snddelay;
            for(int i=pixrect.left(); i<=pixrect.right(); i++) {

                int ne = int((i+1+winpixdelay)*p2n)-snddelay;

                if(ns>=0 && ne<int(m_snd->wav.size())) {
                    WAVTYPE ymin = 1.0;
                    WAVTYPE ymax = -1.0;
                    WAVTYPE* ypp = yp+ns;
                    WAVTYPE y;
                    for(int n=ns; n<=ne; n++) {
                        y = *ypp;
                        ymin = std::min(ymin, y);
                        ymax = std::max(ymax, y);
                        ypp++;
                    }
                    ymin = gain*ymin;
                    if(ymin>1.0)       ymin = 1.0;
                    else if(ymin<-1.0) ymin = -1.0;
                    ymax = gain*ymax;
                    if(ymax>1.0)       ymax = 1.0;
                    else if(ymax<-1.0) ymax = -1.0;
                    ymin *= s2p;
                    ymax *= s2p;
                    ymin = int(ymin-2);
                    ymax = int(ymax-2);
                    m_snd->m_wavpx_min[i] = ymin;
                    m_snd->m_wavpx_max[i] = ymax;
                    painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
                }
                else {
                    m_snd->m_wavpx_min[i] = 1e20; // TODO Clean this dirty fix
                    m_snd->m_wavpx_max[i] = 1e20; // TODO Clean this dirty fix
                }

                ns = ne;
            }
        }
        m_snd->m_wavparams = reqparams;

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }
}

// Might be usefull for #419
//void QGVWaveform::draw_allwaveforms(QPainter* painter, const QRectF& rect){
////    COUTD << "QGVWaveform::draw_allwaveforms " << rect << endl;
////    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVWaveform::draw_waveform [" << rect.width() << "]" << endl;

//    // First shift the visible waveforms according to the previous scrolling
//    // (This cannot be done in scrollContentsBy because this &^$%#@ function is called when resizing the
//    //  spectrogram, because they are synchronized horizontally (Using: connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int))); onnect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));)).
////    COUTD << m_scrolledx << endl;
//    if(m_currentAction!=CAZooming && gMW->m_gvSpectrogram->m_currentAction!=QGVSpectrogram::CAZooming){
//        for(size_t fi=0; fi<gFL->ftsnds.size(); fi++){
//            if(!gFL->ftsnds[fi]->m_actionShow->isChecked())
//                continue;

//            if(gFL->ftsnds[fi]->m_wavpx_min.size()>0){
//                int ddx = m_scrolledx;
//                if(ddx>0){
//                    while(ddx>0){
//                        gFL->ftsnds[fi]->m_wavpx_min.pop_back();
//                        gFL->ftsnds[fi]->m_wavpx_max.pop_back();
//                        gFL->ftsnds[fi]->m_wavpx_min.push_front(0.0);
//                        gFL->ftsnds[fi]->m_wavpx_max.push_front(0.0);
//                        ddx--;
//                    }
//                }
//                else if(ddx<0){
//                    while(ddx<0){
//                        gFL->ftsnds[fi]->m_wavpx_min.pop_front();
//                        gFL->ftsnds[fi]->m_wavpx_max.pop_front();
//                        gFL->ftsnds[fi]->m_wavpx_min.push_back(0.0);
//                        gFL->ftsnds[fi]->m_wavpx_max.push_back(0.0);
//                        ddx++;
//                    }
//                }
//            }
//        }
//    }
//    m_scrolledx = 0;
//}
