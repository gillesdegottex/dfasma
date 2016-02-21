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
#include "gvspectrumamplitude.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
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
    m_imgSTFT = QImage(1, 1, QImage::Format_ARGB32);
    m_imgSTFT.fill(Qt::white);

    m_giWavForWaveform = NULL;
    m_channelid = 0;
    m_isclipped = false;
    m_isfiltered = false;
    m_isplaying = false;
    wavtoplay  = &wav;
    m_filteredmaxamp = 0.0;
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;

    m_stftpa = NULL;
    m_stft_min = std::numeric_limits<FFTTYPE>::infinity();
    m_stft_max = -std::numeric_limits<FFTTYPE>::infinity();

    m_giSQNRForSpectrumAmplitude = new QGraphicsLineItem(0.0, 0.0, 44100.0/2, 0.0);
    m_giSQNRForSpectrumAmplitude->setVisible(false);

    connect(m_actionShow, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));

    m_actionInvPolarity = new QAction("Inverse polarity", this);
    m_actionInvPolarity->setStatusTip(tr("Inverse the polarity of the sound"));
    m_actionInvPolarity->setCheckable(true);
    m_actionInvPolarity->setChecked(false);
    connect(m_actionInvPolarity, SIGNAL(triggered()), this, SLOT(inversePolarity()));

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
    connect(m_actionResetFiltering, SIGNAL(triggered()), gMW, SLOT(resetFiltering()));
}

void FTSound::constructor_external() {
    FileType::constructor_external();

    connect(m_actionInvPolarity, SIGNAL(triggered()), gMW, SLOT(allSoundsChanged()));
    connect(m_actionResetAmpScale, SIGNAL(triggered()), gFL, SLOT(fileInfoUpdate()));
    connect(m_actionResetDelay, SIGNAL(triggered()), gFL, SLOT(fileInfoUpdate()));

    gFL->ftsnds.push_back(this);

    QPen pen(getColor());
    pen.setWidth(0);

    m_giWavForWaveform = new QAEGIUniformlySampledSignal(wavtoplay, fs, gMW->m_gvWaveform);
    m_giWavForWaveform->setPen(pen);
    m_giWavForWaveform->setClip(-1.0, 1.0);
    gMW->m_gvWaveform->m_scene->addItem(m_giWavForWaveform);

    m_giWavForSpectrumAmplitude = new QAEGIUniformlySampledSignal(&m_dftamp, 1.0, gMW->m_gvSpectrumAmplitude);
    m_giWavForSpectrumAmplitude->setPen(pen);
    gMW->m_gvSpectrumAmplitude->m_scene->addItem(m_giWavForSpectrumAmplitude);
    m_giSQNRForSpectrumAmplitude->setPen(pen);
    gMW->m_gvSpectrumAmplitude->m_scene->addItem(m_giSQNRForSpectrumAmplitude);

    m_giWavForSpectrumPhase = new QAEGIUniformlySampledSignal(&m_dftphase, 1.0, gMW->m_gvSpectrumPhase);
    m_giWavForSpectrumPhase->setPen(pen);
    gMW->m_gvSpectrumPhase->m_scene->addItem(m_giWavForSpectrumPhase);

    m_giWavForSpectrumGroupDelay = new QAEGIUniformlySampledSignal(&m_dftgd, 1.0, gMW->m_gvSpectrumGroupDelay);
    m_giWavForSpectrumGroupDelay->setPen(pen);
    gMW->m_gvSpectrumGroupDelay->m_scene->addItem(m_giWavForSpectrumGroupDelay);
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
            throw QString("There is not enough free memory to hold this file!");
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
    fs = ft.fs;
    m_fileaudioformat.setSampleRate(fs);
    m_fileaudioformat.setSampleType(QAudioFormat::Float);
    m_fileaudioformat.setSampleSize(8*sizeof(WAVTYPE));

    m_giSQNRForSpectrumAmplitude->setPos(0.0, 20*std::log10(std::pow(2.0,m_fileaudioformat.sampleSize())));

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    FTSound::constructor_external();
}

void FTSound::load_finalize() {
    if(s_avoidclickswindow.size()==0)
        FTSound::setAvoidClicksWindowDuration(gMW->m_dlgSettings->ui->sbPlaybackAvoidClicksWindowDuration->value());

//    std::cout << "INFO: " << wav.size() << " samples loaded (" << wav.size()/fs << "s max amplitude=" << m_wavmaxamp << ")" << endl;

    m_giSQNRForSpectrumAmplitude->setPos(0.0, 20*std::log10(std::pow(2.0,m_fileaudioformat.sampleSize())));

    m_lastreadtime = QDateTime::currentDateTime();
    needDFTUpdate();
    setStatus();
}

void FTSound::setVisible(bool shown){
    FileType::setVisible(shown);
    m_giWavForWaveform->setVisible(shown);
    m_giWavForSpectrumAmplitude->setVisible(shown);
    m_giWavForSpectrumPhase->setVisible(shown);
    m_giWavForSpectrumGroupDelay->setVisible(shown);
//    if(!shown)
//        m_giWaveform->m_wavparams.clear();
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

void FTSound::setColor(const QColor &_color){
    FileType::setColor(_color);

    QPen pen(getColor());
    pen.setWidth(0);
    m_giWavForWaveform->setPen(pen);
    m_giWavForSpectrumAmplitude->setPen(pen);
    m_giWavForSpectrumPhase->setPen(pen);
    m_giWavForSpectrumGroupDelay->setPen(pen);
    m_giSQNRForSpectrumAmplitude->setPen(pen);
}

void FTSound::zposReset(){
    m_giWavForWaveform->setZValue(0.0);
    m_giWavForSpectrumAmplitude->setZValue(0.0);
    m_giWavForSpectrumPhase->setZValue(0.0);
    m_giWavForSpectrumGroupDelay->setZValue(0.0);
    m_giSQNRForSpectrumAmplitude->setZValue(0.0);
}
void FTSound::zposBringForward(){
    m_giWavForWaveform->setZValue(1.0);
    m_giWavForSpectrumAmplitude->setZValue(1.0);
    m_giWavForSpectrumPhase->setZValue(1.0);
    m_giWavForSpectrumGroupDelay->setZValue(1.0);
    m_giSQNRForSpectrumAmplitude->setZValue(1.0);
}

bool FTSound::reload() {
//    COUTD << "FTSound::reload" << endl;

    stopPlay();
    gMW->m_gvSpectrogram->m_stftcomputethread->cancelComputation(this);

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return false;

    double fs_prev = fs;

    // Reset everything ...
    wavtoplay = &wav;
    m_giWavForWaveform->setSignal(wavtoplay);
//    m_ampscale = 1.0;
//    m_delay = 0;
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    m_avoidclickswinpos = 0;
    wav.clear();
    wavfiltered.clear();
    setFiltered(false);
    gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.lock();
//    m_stft.clear();
    if(m_stftpa){
        delete m_stftpa;
        m_stftpa = NULL;
    }
    m_stftts.clear();
    gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.unlock();
    m_imgSTFTParams.clear();
    m_stftparams.clear();

    // ... and reload the data from the file
    try{
        load();
        load_finalize();
        m_giWavForWaveform->updateMinMaxValues();

        if(fs_prev!=fs)
            throw QString("The sampling rate of the file has changed. This is currently not supported. Please open the file with another instance of DFasma.");
    }
    catch(std::bad_alloc err){
        QMessageBox::critical(NULL, "Memory full!", "There is not enough free memory for re-loading this file.");

        throw QString("The is not enough free memory for re-loading this file!");
    }

    m_giWavForWaveform->clearCache();

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

    if(std::abs(m_giWavForWaveform->gain())!=1.0)
        str += "<b>Scaled: "+QString::number(20*std::log10(std::abs(m_giWavForWaveform->gain())), 'f', 4)+"dB ("+QString::number(m_giWavForWaveform->gain(), 'f', 4)+")</b><br/>";
    if(isClipped())
        str += "<font color=\"red\"><b>CLIPPED</b></font><br/>";
    if(m_giWavForWaveform->delay()!=0.0)
        str += "<b>Delayed: "+QString("%1").arg(double(m_giWavForWaveform->delay())/fs, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())+"s ("+QString::number(m_giWavForWaveform->delay())+")</b><br/>";

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
    m_actionResetAmpScale->setText(QString("Reset amplitude scaling (%1dB) to 0dB").arg(20*log10(std::abs(m_giWavForWaveform->gain())), 0, 'g', 3));
    m_actionResetAmpScale->setDisabled(std::abs(m_giWavForWaveform->gain())==1.0);
    contextmenu.addAction(m_actionResetAmpScale);
    m_actionResetDelay->setText(QString("Reset delay (%1s) to 0s").arg(m_giWavForWaveform->delay()/gFL->getFs(), 0, 'g', gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()));
    m_actionResetDelay->setDisabled(m_giWavForWaveform->delay()==0);
    contextmenu.addAction(m_actionResetDelay);

    contextmenu.addSeparator();
    contextmenu.addAction(gMW->ui->actionEstimationF0);
}

void FTSound::needDFTUpdate() {
    m_stftparams.clear();
}

void FTSound::setFiltered(bool filtered){
    if(filtered!=m_isfiltered){
        if(filtered){
            wavtoplay = &wavfiltered;
            m_giWavForWaveform->setSignal(wavtoplay);
        }
        else{
            wavtoplay = &wav;
            m_giWavForWaveform->setSignal(wavtoplay);
            m_filteredmaxamp = 0.0;
            needDFTUpdate();
        }
        m_isfiltered = filtered;
        updateClippedState();
        gFL->fileInfoUpdate();
        setStatus();
    }
}

void FTSound::resetAmpScale(){
    if(m_giWavForWaveform->gain()!=1.0){
        m_giWavForWaveform->setGain(1.0);

        setStatus();

        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvSpectrumAmplitude->updateDFTs();
        gMW->ui->pbSpectrogramSTFTUpdate->show();
        if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
            gMW->m_gvSpectrogram->updateSTFTSettings();
    }
}
void FTSound::resetDelay(){
    if(m_giWavForWaveform->delay()!=0.0){
        m_giWavForWaveform->setDelay(0.0);

        setStatus();

        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvSpectrumAmplitude->updateDFTs();
        gMW->ui->pbSpectrogramSTFTUpdate->show();
        if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
            gMW->m_gvSpectrogram->updateSTFTSettings();
    }
}

void FTSound::inversePolarity(){
    m_giWavForWaveform->setGain(-m_giWavForWaveform->gain());
    m_giWavForWaveform->clearCache();
    gMW->m_gvSpectrumAmplitude->updateDFTs();
    gMW->m_gvSpectrumPhase->m_scene->update();
    gMW->m_gvSpectrumGroupDelay->m_scene->update();
}

double FTSound::getLastSampleTime() const {
    return (wav.size()-1+m_giWavForWaveform->delay())/fs;
}
bool FTSound::isModified() {
    if(m_giWavForWaveform==NULL)
        return false;

    return m_giWavForWaveform->delay()!=0.0 || m_giWavForWaveform->gain()!=1.0;
}

void FTSound::setStatus(){
    FileType::setStatus();

    updateClippedState();
}
void FTSound::updateClippedState(){
    if(m_giWavForWaveform==NULL)
        m_isclipped = false;
    else
        m_isclipped = (m_giWavForWaveform->getMaxAbsoluteValue()>1.0) || (m_isfiltered && (m_filteredmaxamp*m_giWavForWaveform->gain()>1.0));

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

double FTSound::setPlay(const QAudioFormat& format, double tstart, double tstop, double fstart, double fstop) {
//    COUTD << "FTSound::setPlay" << endl;
    DLOG << "FTSound::setPlay";

    m_outputaudioformat = format;

    if(format.sampleRate()!=fs)
        throw QString("The sampling frequency of the file is different from that of the audio engine. They have to be the same.");

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
    if(m_start>qint64(wav.size()-1)+m_giWavForWaveform->delay()) m_start=wav.size()-1+m_giWavForWaveform->delay();
    if(m_end<0) m_end=0;
    if(m_end>qint64(wav.size()-1)+m_giWavForWaveform->delay()) m_end=wav.size()-1+m_giWavForWaveform->delay();

    int delayedstart = m_start-m_giWavForWaveform->delay();
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size())-1) delayedstart=int(wavtoplay->size())-1;
    int delayedend = m_end-m_giWavForWaveform->delay();
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
        // Filtered play
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
            gMW->m_gvSpectrumAmplitude->m_filterresponse = std::vector<FFTTYPE>(BUTTERRESPONSEDFTLEN/2+1,1.0);
            std::vector< std::vector<double> > num, den;
            std::vector<double> filterresponse;

            if (doLowPass) {
                // Compute the Butterworth filter coefficients
                mkfilter::make_butterworth_filter_biquad(butterworth_order, fstop/fs, true, num, den, &filterresponse, BUTTERRESPONSEDFTLEN);

                // Update the filter response
                for(size_t k=0; k<filterresponse.size(); k++){
                    if(filterresponse[k] < 2*std::numeric_limits<FFTTYPE>::min())
                        filterresponse[k] = std::numeric_limits<FFTTYPE>::min();
                    gMW->m_gvSpectrumAmplitude->m_filterresponse[k] *= filterresponse[k];
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
                    gMW->m_gvSpectrumAmplitude->m_filterresponse[k] *= filterresponse[k];
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

            // It seems the filtering went well, we can use the filtered sound and update the views

            setFiltered(true);

            // The filter response has been computed
            // Convert it to dB and multiply by 2 bcs the filtfilt doubled the effect.
            for(size_t k=0; k<gMW->m_gvSpectrumAmplitude->m_filterresponse.size(); k++)
                gMW->m_gvSpectrumAmplitude->m_filterresponse[k] = 2*20*log10(gMW->m_gvSpectrumAmplitude->m_filterresponse[k]);
            gMW->m_gvSpectrumAmplitude->m_scene->update();

            m_giWavForWaveform->clearCache(); // TODO clear only the previous and current selection
            gMW->m_gvWaveform->m_scene->invalidate(gMW->m_gvWaveform->m_giFilteredSelection->rect());
        }
        catch(QString err){
            m_isplaying = false;
            updateIcon();
            QMessageBox::warning(NULL, "Problem when filtering the sound to be played", QString("The sound cannot be filtered as given by the selection in the spectrum view.\n\nReason:\n")+err);
//            wavtoplay = &wav;
//            m_giWaveform->setSequence(&wav);
            throw QString("Problem when filtering the sound to be played");
        }
    }
    else {
        // Play (non-filtered)
        setFiltered(false);
        gMW->m_gvSpectrumAmplitude->m_filterresponse.clear();
    }

    // Reset any previously filtered sounds
    if(gMW->m_lastFilteredSound && gMW->m_lastFilteredSound->isFiltered())
        if(gMW->m_lastFilteredSound!=this)
            gMW->m_lastFilteredSound->setFiltered(false);

    gMW->m_gvSpectrumAmplitude->updateDFTs();

    if(isFiltered()){
        if(gMW->m_gvWaveform->m_giSelection->rect().width()>0)
            gMW->m_gvWaveform->m_giFilteredSelection->setRect(gMW->m_gvWaveform->m_giSelection->rect());
        else
            gMW->m_gvWaveform->m_giFilteredSelection->setRect(-0.5/gFL->getFs(), -1.0, getLastSampleTime()+1.0/gFL->getFs(), 2.0);
        gMW->m_gvWaveform->m_giFilteredSelection->show();
        // SPEEDUP Could clear/update/invalidate only the concerned time selection
        m_giWavForWaveform->clearCache();
        gMW->m_gvWaveform->m_scene->update();
        gMW->m_lastFilteredSound = this;
    }
    else {
        gMW->m_gvWaveform->m_giFilteredSelection->hide();
        gMW->m_lastFilteredSound = NULL;
    }

    if(!m_actionShow->isChecked())
        gMW->statusBar()->showMessage("WARNING: Playing a hidden waveform!", 3000);
    else
        gMW->statusBar()->clearMessage();


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
    int delayedstart = m_start-m_giWavForWaveform->delay();
    if(delayedstart<0) delayedstart=0;
    if(delayedstart>int(wavtoplay->size()-1)) delayedstart=int(wavtoplay->size())-1;
    int delayedend = m_end-m_giWavForWaveform->delay();
    if(delayedend<0) delayedend=0;
    if(delayedend>int(wavtoplay->size()-1)) delayedend=int(wavtoplay->size())-1;

    // Polarity apparently matters in very particular cases
    // so take it into account when playing.
    double gain = m_giWavForWaveform->gain();
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
            int delayedpos = m_pos - m_giWavForWaveform->delay();
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
    if(gFL->m_prevSelectedSound==this)
        gFL->m_prevSelectedSound = NULL;

    stopPlay();
    if(gMW->m_gvSpectrogram)
        gMW->m_gvSpectrogram->m_stftcomputethread->cancelComputation(this, true);
    QIODevice::close();

    delete m_giWavForWaveform;
    delete m_giWavForSpectrumAmplitude;
    delete m_giWavForSpectrumPhase;
    delete m_giWavForSpectrumGroupDelay;

    gFL->ftsnds.erase(std::find(gFL->ftsnds.begin(), gFL->ftsnds.end(), this));

    if(m_stftpa){
        delete m_stftpa;
        m_stftpa = NULL;
    }

    delete m_actionResetFiltering;
    delete m_actionResetDelay;
    delete m_actionResetAmpScale;
    delete m_actionInvPolarity;
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
