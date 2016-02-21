/*
Copyright (C) 2015  Gilles Degottex <gilles.degottex@gmail.com>

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

#include "stftcomputethread.h"

#include <QtGlobal>

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ftsound.h"
#include "../external/libqxt/qxtspanslider.h"

#include "qaecolormap.h"
#include "qaesigproc.h"
#include "qaehelpers.h"

STFTComputeThread::STFTParameters::STFTParameters(FTSound* reqnd, const std::vector<FFTTYPE>& reqwin, int reqstepsize, int reqdftlen, int reqcepliftorder, bool reqcepliftpresdc){
    clear();

    snd = reqnd;
    ampscale = reqnd->m_giWavForWaveform->gain();
    delay = reqnd->m_giWavForWaveform->delay();
    win = reqwin;
    stepsize = reqstepsize;
    dftlen = reqdftlen;
    cepliftorder = reqcepliftorder;
    cepliftpresdc = reqcepliftpresdc;
}

bool STFTComputeThread::STFTParameters::operator==(const STFTParameters& param) const {
    if(snd!=param.snd)
        return false;
    if(ampscale!=param.ampscale)
        return false;
    if(delay!=param.delay)
        return false;
    if(stepsize!=param.stepsize)
        return false;
    if(dftlen!=param.dftlen)
        return false;
    if(cepliftorder!=param.cepliftorder)
        return false;
    if(cepliftpresdc!=param.cepliftpresdc)
        return false;
    if(win.size()!=param.win.size())
        return false;
    for(size_t n=0; n<win.size(); n++)
        if(win[n]!=param.win[n])
            return false;

    return true;
}

STFTComputeThread::STFTComputeThread(QObject* parent)
    : QThread(parent)
{
    m_fft = new qae::FFTwrapper();
//    setPriority(QThread::IdlePriority);
}

void STFTComputeThread::compute(ImageParameters reqImgSTFTParams) {
//    DCOUT << "STFTComputeThread::compute" << std::endl;

    if(reqImgSTFTParams.stftparams.win.size()<2)
        throw QString("Window's length is too short");

    if(reqImgSTFTParams.stftparams.snd->wav.empty())
        return; // TODO
//        throw QString("Sound is empty");

    m_mutex_changingparams.lock();

//    DCOUT << "STFTComputeThread::compute winlen=" << reqImgSTFTParams.stftparams.win.size() << " stepsize=" << reqImgSTFTParams.stftparams.stepsize << " dftlen=" << reqImgSTFTParams.stftparams.dftlen << std::endl;

    // Check if this is necessary to re-compute the STFT.
    // Maybe updating the image is sufficient.
    reqImgSTFTParams.stftparams.computestft = reqImgSTFTParams.stftparams.snd->m_stftparams.isEmpty()
            || (reqImgSTFTParams.stftparams.snd->m_stftparams!=reqImgSTFTParams.stftparams);

//    DCOUT << "Compute STFT " << reqImgSTFTParams.stftparams.computestft << std::endl;
    if(m_mutex_computing.tryLock()) {
        // Currently not computing, so start it!

        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->show();
        gMW->ui->pbSpectrogramSTFTUpdate->hide();

        m_params_current = reqImgSTFTParams;
        m_computing = true;
        start(); // Start computing
    }
    else {
        // Currently computing something
        // So cancel it and run the new params
        if(reqImgSTFTParams!=m_params_current && reqImgSTFTParams!=m_params_todo) {
            m_params_todo = reqImgSTFTParams;  // Ask to compute a new one, once the current computation is finished
            gMW->ui->pbSTFTComputingCancel->setChecked(true);
        }
    }

    m_mutex_changingparams.unlock();

//    DCOUT << "STFTComputeThread::~compute" << std::endl;
}

STFTComputeThread::~STFTComputeThread(){
    delete m_fft;
}

void STFTComputeThread::run() {
//    DCOUT << "STFTComputeThread::run" << std::endl;

    // Prepare some usefull variables
//    std::vector<WAVTYPE>::iterator pstft;
    WAVTYPE* pstft;

    bool canceled = false;
    do{
        m_mutex_changingparams.lock();
        ImageParameters params_running = m_params_current;
        m_mutex_changingparams.unlock();

        try{
            int stepsize = params_running.stftparams.stepsize;
            int dftlen = params_running.stftparams.dftlen;
            int dftsize = int(params_running.stftparams.dftlen/2+1);
            WAVTYPE* &stftpa = params_running.stftparams.snd->m_stftpa;
            WAVTYPE* stftfrpa = NULL; // Pointer to a single frame

            // If asked, update the STFT
            if(params_running.stftparams.computestft){
                emit stftComputingStateChanged(SCSDFT);

                m_fft->resize(params_running.stftparams.dftlen);

                m_mutex_changingstft.lock();

                std::vector<FFTTYPE>& win = params_running.stftparams.win;
                int winlen = int(win.size());
                int fs = params_running.stftparams.snd->fs;
                FFTTYPE stftmin = std::numeric_limits<FFTTYPE>::infinity();
                FFTTYPE stftmax = -std::numeric_limits<FFTTYPE>::infinity();
                qreal gain = params_running.stftparams.ampscale;
                qint64 snddelay = params_running.stftparams.snd->m_giWavForWaveform->delay();
                std::vector<FFTTYPE>& stftts = params_running.stftparams.snd->m_stftts;
                std::vector<WAVTYPE>* wav = &params_running.stftparams.snd->wav;

                int maxsampleindex = int(wav->size())-1 + int(params_running.stftparams.snd->m_giWavForWaveform->delay());
                maxsampleindex = std::min(maxsampleindex, int(gFL->getFs()*gFL->getMaxLastSampleTime()));

                int minsampleindex = int(params_running.stftparams.snd->m_giWavForWaveform->delay());
                minsampleindex = std::max(minsampleindex, 0);
                int minsi = int(minsampleindex/stepsize);

                // Allocate everything
                int stftlen = 0;
                for(int si=minsi; int(si*stepsize)<maxsampleindex; ++si)
                    stftlen++;
                stftts.resize(stftlen);
                int stfttsi = 0;
                for(int si=minsi; int(si*stepsize)<maxsampleindex; ++si){
                    stftts[stfttsi] = (si*stepsize+(winlen-1)/2.0)/fs;
                    stfttsi++;
                }
                if(stftpa)
                    delete stftpa;
                stftpa = new WAVTYPE[stftlen*dftsize];
                m_mutex_changingstft.unlock();

                WAVTYPE value;
                int ni=0;
                for(int si=minsi; int(si*stepsize)<maxsampleindex && !gMW->ui->pbSTFTComputingCancel->isChecked(); ++si){

                    // Set the DFT's input
                    int n = 0;
                    int wn = 0;
                    bool hasnonzerovalues = false;
                    for(; n<int(win.size()); ++n){
                        wn = si*stepsize+n - snddelay;
                        value = 0.0;
                        if(wn>=0 && wn<int(wav->size())) {
                            value = gain*(*(wav))[wn];

                            if(value>1.0)       value = 1.0;
                            else if(value<-1.0) value = -1.0;

                            value *= win[n];

                            if(std::abs(value)>0.0)
                                hasnonzerovalues = true;
                        }
                        m_fft->setInput(n, value);
                    }

                    if(hasnonzerovalues){
                        // Zero-pad the DFT's input
                        for(; n<dftlen; ++n)
                            m_fft->setInput(n, 0.0);

                        m_fft->execute(false); // Compute the DFT

                        // Retrieve DFT's output
                        stftfrpa = stftpa+ni*dftsize;
                        *stftfrpa = std::log(std::abs(m_fft->getDCOutput()));
                        for(n=1; n<dftlen/2; ++n, stftfrpa++)
                            *stftfrpa = std::log(std::abs(m_fft->getMidOutput(n)));
                        *stftfrpa = std::log(std::abs(m_fft->getNyquistOutput()));

    //                    for(n=0; n<=m_params_current.stftparams.dftlen/2; n++)
    //                        m_params_current.stftparams.snd->m_stft[ni][n] = std::log(std::abs(m_fft->out[n]));
    //                        m_params_current.stftparams.snd->m_stft[ni][n] = std::log(std::abs(m_fft->getMidOutput(n)));

                        if(params_running.stftparams.cepliftorder>0){
                            // Prepare the window for cepstral smoothing
                            std::vector<FFTTYPE> win = qae::hamming(params_running.stftparams.cepliftorder*2+1);
                            std::vector<FFTTYPE> cc;
                            // First, fix possible Inf amplitudes to avoid ending up with NaNs.
                            if(qIsInf(stftpa[ni*dftsize+0]))
                                stftpa[ni*dftsize+0] = stftpa[ni*dftsize+1]; // TOOD Use extrap ??
                            for(int n=1; n<dftlen/2+1; ++n) {
                                if(qIsInf(stftpa[ni*dftsize+n]))
                                    stftpa[ni*dftsize+n] = stftpa[ni*dftsize+n-1]; // TOOD Use extrap ??
                            }
//                            // If we need to compute less coefficients than log(N),
//                            // don't use the FFT, just compute these coefs.
//                            // Could do: Optimize threshold according to time
//                            // This is actually slower than computing all the coefs
//                            // using the FFTW3 ...
//                            if(params_running.stftparams.cepliftorder<std::log(dftlen)-1){
//                                // Compute the Fourier coefs manually
//                                std::vector<WAVTYPE> &frame = (stft[ni]);
//                                std::vector<double> coses(dftlen/2+1);
//                                for(int cci=1; cci<1+params_running.stftparams.cepliftorder; ++cci){
//                                    double cc = frame[0];
//                                    coses[0] = 1.0;
//                                    for(int n=1; n<dftlen/2; ++n){
//                                        coses[n] = cos(2.0*M_PI*cci*n/dftlen);
//                                        cc += 2*frame[n]*coses[n];
//                                    }
//                                    coses[dftlen/2] = cos(M_PI*cci);
//                                    cc += frame[dftlen/2]*coses[dftlen/2];
//                                    cc /= dftlen;
//                                    cc *= 2.0;

//                                    for(int n=0; n<dftlen/2+1; ++n)
//                                        frame[n] -= (1.0-win[cci-1])*cc*coses[n];
//                                }
//                                if(!params_running.stftparams.cepliftpresdc){
//                                    double ccdc = frame[0];
//                                    for(int n=1; n<dftlen/2; ++n)
//                                        ccdc += 2*frame[n];
//                                    ccdc += frame[dftlen/2];
//                                    ccdc /= dftlen;
//                                    for(int n=0; n<dftlen/2+1; ++n)
//                                        frame[n] -= ccdc;
//                                }
//                            }
//                            else{
                                std::vector<FFTTYPE> values(stftpa+ni*dftsize, stftpa+ni*dftsize+dftsize);
                                hspec2rcc(values, m_fft, cc);
                                for(int cci=1; cci<1+params_running.stftparams.cepliftorder && cci<int(cc.size()); ++cci)
                                    cc[cci] *= win[cci-1];
                                if(!params_running.stftparams.cepliftpresdc)
                                    cc[0] = 0.0;
                                rcc2hspec(cc, m_fft, values);
                                for(int n=0; n<dftlen/2+1; n++)
                                    stftpa[ni*dftsize+n] = values[n];
//                            }
                        }

                        // Convert to [dB] and compute min and max magnitudes[dB]
                        stftfrpa = stftpa+ni*dftsize;
                        for(n=0; n<dftlen/2+1; n++, stftfrpa++) {
                            FFTTYPE value = qae::log2db*(*stftfrpa);

                            if(qIsNaN(value))
                                value = -std::numeric_limits<FFTTYPE>::infinity();

                            *stftfrpa = value;

                            // Do not consider Inf values as well as DC and Nyquist (Too easy to degenerate)
                            if(n!=0 && n!=dftlen/2 && !qIsInf(value)) {
                                stftmin = std::min(stftmin, value);
                                stftmax = std::max(stftmax, value);
                            }
                        }
                    }
                    else{
                        stftfrpa = stftpa+ni*dftsize;
                        for(n=0; n<dftsize; n++, stftfrpa++)
                            *stftfrpa = -std::numeric_limits<FFTTYPE>::infinity();
                    }

                    emit stftProgressing(int(100*double(ni*stepsize)/(maxsampleindex-minsampleindex)));

                    ni++;
                }

                if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
                    // The STFT is done, update the min & max
                    m_mutex_changingparams.lock();

                    params_running.stftparams.snd->m_stftparams = params_running.stftparams;

                    if(qIsInf(stftmin) && qIsInf(stftmax)){
                        stftmax = 0.0; // Default 0dB
                        stftmin = -1.0; // Default -1dB
                    }
                    else if(qIsInf(stftmin))
                        stftmin = stftmax - 1.0;
                    else if(qIsInf(stftmax))
                        stftmax = stftmin + 1.0;

                    m_mutex_changingstft.lock();
                    params_running.stftparams.snd->m_stft_min = stftmin;
                    params_running.stftparams.snd->m_stft_max = stftmax;
                    m_mutex_changingstft.unlock();

                    m_mutex_changingparams.unlock();
                }
            }

            // Update the STFT image
            if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
                emit stftComputingStateChanged(SCSIMG);

                m_mutex_imageallocation.lock();
                if(int(params_running.stftparams.snd->m_stftts.size())==0){
                    m_mutex_imageallocation.unlock();
                }
                else{
                    int stftlen = int(params_running.stftparams.snd->m_stftts.size());
                    int halfdftlen = params_running.stftparams.dftlen/2;
                    *(params_running.imgstft) = QImage(stftlen, dftsize, QImage::Format_ARGB32);
                    m_mutex_imageallocation.unlock();
                    if(params_running.imgstft->isNull())
                        throw std::bad_alloc();
                    bool colormap_reversed = params_running.colormap_reversed;

                    QAEColorMap& cmap = QAEColorMap::getAt(params_running.colormap_index);
                    cmap.setColor(params_running.stftparams.snd->getColor());

                    QRgb* pimgb = (QRgb*)(params_running.imgstft->bits());

                    FFTTYPE ymin = 0.0; // Init shouldn't be used
                    FFTTYPE ymax = 1.0; // Init shouldn't be used
                    if(params_running.colorrangemode==0){
                        ymin = params_running.stftparams.snd->m_stft_min+(params_running.stftparams.snd->m_stft_max-params_running.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0;
                        ymax = params_running.stftparams.snd->m_stft_min+(params_running.stftparams.snd->m_stft_max-params_running.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0;
                    }
                    else if(params_running.colorrangemode==1){
                        ymin = gMW->m_qxtSpectrogramSpanSlider->lowerValue(); // Min of color range [dB]
                        ymax = gMW->m_qxtSpectrogramSpanSlider->upperValue(); // Max of color range [dB]
                    }

                    bool uselw = params_running.loudnessweighting;
                    FFTTYPE divmaxmmin = 1.0/(ymax-ymin);
        //            QRgb red = qRgb(int(255*1), int(255*0), int(255*0));
                    QRgb c0 = colormap_reversed?cmap(1.0):cmap(0.0);
                    QRgb c1 = colormap_reversed?cmap(0.0):cmap(1.0);
                    FFTTYPE y;
                    QRgb c;
                    FFTTYPE v;

                    // Prepare the loudness curve
                    std::vector<WAVTYPE> elc;
                    if(uselw) {
                        elc = std::vector<WAVTYPE>(dftsize, 0.0);
                        for(size_t u=0; u<elc.size(); ++u) {
                            elc[u] = -qae::equalloudnesscurvesISO226(params_running.stftparams.snd->fs*double(u)/params_running.stftparams.dftlen, 0);
                        }
                    }

                    for(int si=0; si<stftlen && !gMW->ui->pbSTFTComputingCancel->isChecked(); si++, pimgb++){
                        stftfrpa = stftpa+si*dftsize;
                        for(int n=0; n<dftsize; n++, pstft++, stftfrpa++) {

                            if(qIsInf(*stftfrpa)){
                                c = c0;
                            }
                            else {
                                v = *stftfrpa;
                                if(uselw) v += elc[n]; // Modification according to loudness curve
                                y = (v-ymin)*divmaxmmin;

                                if(y<=0.0)
                                    c = c0;
                                else if(y>=1.0)
                                    c = c1;
                                else {
                                    if(colormap_reversed)
                                        y = 1.0-y;

                                    c = cmap(y);
                                }
                            }

    //                        *(pimgb + n*stftlen) = c;
                            *(pimgb + (halfdftlen-n)*stftlen) = c; // This one has reversed y

    //                         params_running.imgstft->setPixel(si, n, c); // Bit slower, though can take adv. of hardware optim ?
                        }
                        emit stftProgressing((100*si)/stftlen);
                    }

                    // SampleSize is not always reliable
        //            m_params_current.stftparams.snd->m_stft_min = std::max(FFTTYPE(-2.0*20*std::log10(std::pow(2.0,m_params_current.stftparams.snd->format().sampleSize()))), m_params_current.stftparams.snd->m_stft_min); Why doing this ??
        //            COUTD << "Image Spent: " << starttime.elapsed() << std::endl;
                }

                m_mutex_changingparams.lock();
                params_running.stftparams.snd->m_imgSTFTParams = m_params_current;
                m_mutex_changingparams.unlock();
            }
        }
        catch(std::bad_alloc err){
            m_mutex_changingstft.unlock();
            m_mutex_changingstft.lock();
            params_running.stftparams.snd->m_stftts.clear();
//            params_running.stftparams.snd->m_stft.clear();
            delete params_running.stftparams.snd->m_stftpa;
            params_running.stftparams.snd->m_stftpa = NULL;
            m_mutex_changingstft.unlock();

            emit stftComputingStateChanged(SCSMemoryFull);
            gMW->ui->pbSTFTComputingCancel->setChecked(true);
        }

        canceled = gMW->ui->pbSTFTComputingCancel->isChecked();
        if(canceled){
            m_mutex_changingparams.lock();
            if(params_running.stftparams.snd->m_stftparams != params_running.stftparams) {
                m_mutex_changingstft.lock();
                params_running.stftparams.snd->m_stftts.clear();
//                params_running.stftparams.snd->m_stft.clear();
                params_running.stftparams.snd->m_stftparams.clear();
                m_mutex_changingstft.unlock();
                m_mutex_imageallocation.lock();
                *(params_running.imgstft) = QImage(1, 1, QImage::Format_ARGB32);
                params_running.imgstft->fill(Qt::white);
                m_mutex_imageallocation.unlock();
            }
            m_mutex_changingparams.unlock();
            gMW->ui->pbSTFTComputingCancel->setChecked(false);
        }

        // Check if it has to compute another
        m_mutex_changingparams.lock();
        if(!m_params_todo.isEmpty()){
            m_params_current = m_params_todo;
            m_params_todo.clear();
        }
        else{
            m_params_current.clear();
            m_computing = false;
        }
        m_mutex_changingparams.unlock();
    }
    while(m_computing);

    m_mutex_computing.unlock();

    if(canceled){
        gMW->ui->lblSpectrogramInfoTxt->show();
        emit stftComputingStateChanged(SCSCanceled);
    }
    else
        emit stftComputingStateChanged(SCSFinished);

//    DCOUT << "STFTComputeThread::~run" << std::endl;
}

void STFTComputeThread::cancelCurrentComputation(bool waittoend) {
//    DCOUT << "STFTComputeThread::cancelCurrentComputation" << std::endl;
    gMW->ui->pbSTFTComputingCancel->setChecked(true);
    if(waittoend){
        m_mutex_computing.lock();
        m_mutex_computing.unlock();
    }
}

void STFTComputeThread::cancelComputation(FTSound* snd, bool closing) {
//    DCOUT << "STFTComputeThread::cancelComputation" << std::endl;
    // Remove it from the STFT waiting queue
    m_mutex_changingparams.lock();
    if(!m_params_todo.isEmpty()
        && m_params_todo.stftparams.snd==snd){
        m_params_todo.clear();
    }
    m_mutex_changingparams.unlock();

    // Or cancel its STFT computation
    while(isRunning()
          && getCurrentParameters().stftparams.snd==snd){
        cancelCurrentComputation(true);
        if(closing){
            gMW->ui->lblSpectrogramInfoTxt->hide();
            emit stftComputingStateChanged(SCSFinished);
        }
//        QThread::msleep(20);
    }
}
