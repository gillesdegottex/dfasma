#include "stftcomputethread.h"

#include "sigproc.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ftsound.h"
#include "qthelper.h"
#include "../external/libqxt/qxtspanslider.h"
#include "colormap.h"

STFTComputeThread::STFTParameters::STFTParameters(FTSound* reqnd, const std::vector<FFTTYPE>& reqwin, int reqstepsize, int reqdftlen, int reqcepliftorder, bool reqcepliftpresdc){
    clear();

    snd = reqnd;
    ampscale = reqnd->m_ampscale;
    delay = reqnd->m_delay;
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
    m_fft = new sigproc::FFTwrapper();
//    setPriority(QThread::IdlePriority);
}

void STFTComputeThread::compute(ImageParameters reqImgSTFTParams) {

    if(reqImgSTFTParams.stftparams.win.size()<2)
        throw QString("Window's length is too short");

    m_mutex_changingparams.lock();

//    COUTD << "STFTComputeThread::compute winlen=" << winlen << " stepsize=" << stepsize << " dftlen=" << dftlen << std::endl;

    // Check if this is necessary to re-compute the STFT.
    // Maybe updating the image is sufficient.
    reqImgSTFTParams.stftparams.computestft = reqImgSTFTParams.stftparams.snd->m_stftparams.isEmpty()
            || (reqImgSTFTParams.stftparams.snd->m_stftparams!=reqImgSTFTParams.stftparams);

//    COUTD << "Compute STFT " << reqImgSTFTParams.stftparams.computestft << std::endl;
    if(m_mutex_computing.tryLock()) {
        // Currently not computing, so start it!

        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->show();

        m_params_current = reqImgSTFTParams;
        start(); // Start computing
    }
    else {
        // Currently already computing something
        // So cancel it and run the new params
        if(reqImgSTFTParams!=m_params_current && reqImgSTFTParams!=m_params_todo) {
            gMW->ui->pbSTFTComputingCancel->setChecked(true);
            m_params_todo = reqImgSTFTParams;  // Ask to compute a new one, once the current computation is finished
        }
    }

//    std::cout << "~STFTComputeThread::compute" << std::endl;

    m_mutex_changingparams.unlock();
}

void STFTComputeThread::run() {
//    COUTD << "STFTComputeThread::run" << std::endl;

    bool canceled = false;
    bool compute = true;
    do{
        // If asked, update the STFT
//        COUTD << m_params_current.stftparams.computestft << std::endl;
        if(m_params_current.stftparams.computestft){
            emit stftComputingStateChanged(SCSDFT);

            m_fft->resize(m_params_current.stftparams.dftlen);

            qreal gain = m_params_current.stftparams.ampscale;

            std::vector<WAVTYPE>* wav = &m_params_current.stftparams.snd->wav;

//        COUTD << "STFTComputeThread::run resize finished" << std::endl;

            m_params_current.stftparams.snd->m_stft.clear();
            m_params_current.stftparams.snd->m_stftts.clear();

            m_params_current.stftparams.snd->m_stft_min = std::numeric_limits<double>::infinity();
            m_params_current.stftparams.snd->m_stft_max = -std::numeric_limits<double>::infinity();

            int maxsampleindex = int(wav->size())-1 + int(m_params_current.stftparams.snd->m_delay);

            for(int si=0; !gMW->ui->pbSTFTComputingCancel->isChecked(); si++){
                if(int(si*m_params_current.stftparams.stepsize+m_params_current.stftparams.win.size())-1 > maxsampleindex)
                    break;

                m_params_current.stftparams.snd->m_stft.push_back(std::vector<WAVTYPE>(m_params_current.stftparams.dftlen/2+1));
                m_params_current.stftparams.snd->m_stftts.push_back((si*m_params_current.stftparams.stepsize+(m_params_current.stftparams.win.size()-1)/2.0)/m_params_current.stftparams.snd->fs);

                int n = 0;
                int wn = 0;
                bool hasfilledvalues = false;
                for(; n<int(m_params_current.stftparams.win.size()); n++){
                    wn = si*m_params_current.stftparams.stepsize+n - m_params_current.stftparams.snd->m_delay;
                    if(wn>=0 && wn<int(wav->size())) {
                        WAVTYPE value = gain*(*(wav))[wn];

                        if(value>1.0)       value = 1.0;
                        else if(value<-1.0) value = -1.0;

                        m_fft->in[n] = value*m_params_current.stftparams.win[n];
                        hasfilledvalues = true;
                    }
                    else
                        m_fft->in[n] = 0.0;
                }

                if(hasfilledvalues){
                    for(; n<m_params_current.stftparams.dftlen; n++)
                        m_fft->in[n] = 0.0;

                    m_fft->execute(); // Compute the DFT

                    for(n=0; n<m_params_current.stftparams.dftlen/2+1; n++)
                        m_params_current.stftparams.snd->m_stft[si][n] = std::log(std::abs(m_fft->out[n]));

                    if(m_params_current.stftparams.cepliftorder>0){
                        std::vector<FFTTYPE> cc;
                        hspec2rcc(m_params_current.stftparams.snd->m_stft[si], m_fft, cc);
                        std::vector<FFTTYPE> win = sigproc::hamming(m_params_current.stftparams.cepliftorder*2+1);
                        for(int n=0; n<m_params_current.stftparams.cepliftorder && n<int(cc.size())-1; ++n)
                            cc[n+1] *= win[n];
                        if(!m_params_current.stftparams.cepliftpresdc)
                            cc[0] = 0.0;
                        rcc2hspec(cc, m_fft, m_params_current.stftparams.snd->m_stft[si]);
                    }

                    for(n=0; n<m_params_current.stftparams.dftlen/2+1; n++) {
                        double y = sigproc::log2db*m_params_current.stftparams.snd->m_stft[si][n];

                        m_params_current.stftparams.snd->m_stft[si][n] = y;

                        m_params_current.stftparams.snd->m_stft_min = std::min(m_params_current.stftparams.snd->m_stft_min, y);
                        m_params_current.stftparams.snd->m_stft_max = std::max(m_params_current.stftparams.snd->m_stft_max, y);
                    }
                }
                else{
                    for(n=0; n<m_params_current.stftparams.dftlen/2+1; n++)
                        m_params_current.stftparams.snd->m_stft[si][n] = -std::numeric_limits<double>::infinity();
                }

                emit stftProgressing(int(100*double(si*m_params_current.stftparams.stepsize)/maxsampleindex));
            }

            if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
                // This is done
                m_mutex_changingparams.lock();
                m_params_current.stftparams.snd->m_stftparams = m_params_current.stftparams;
                m_mutex_changingparams.unlock();
            }
        }

        if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
            emit stftComputingStateChanged(SCSIMG);

            // Update the STFT image
//            m_params_current.snd->m_stftimg_lastupdate = QTime::currentTime();
            *(m_params_current.imgstft) = QImage(int(m_params_current.stftparams.snd->m_stft.size()), int(m_params_current.stftparams.snd->m_stft[0].size()), QImage::Format_RGB32);

            ColorMap& cmap = ColorMap::getAt(m_params_current.colormap_index);

    //                        COUTD << "---" << endl;
    //                        COUTD << "[" << csnd->m_stft_min << "," << csnd->m_stft_max << "]" << endl;
    //                        COUTD << "[" << gMW->m_qxtspanslider->lowerValue() << "," << gMW->m_qxtspanslider->upperValue() << "]" << endl;
            double ymin = m_params_current.stftparams.snd->m_stft_min+(m_params_current.stftparams.snd->m_stft_max-m_params_current.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0; // Min of color range [dB]
            double ymax = m_params_current.stftparams.snd->m_stft_min+(m_params_current.stftparams.snd->m_stft_max-m_params_current.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0; // Max of color range [dB]
    //                        COUTD << "[" << ymin << "," << ymax << "]" << endl;
//            COUTD << "size=" << m_params_current.stftparams.snd->m_stft.size() << std::endl;
            for(int si=0; si<int(m_params_current.stftparams.snd->m_stft.size()) && !gMW->ui->pbSTFTComputingCancel->isChecked(); si++){
                for(int n=0; n<int(m_params_current.stftparams.snd->m_stft[si].size()); n++) {
                    double y = m_params_current.stftparams.snd->m_stft[si][n];

                    y = (y-ymin)/(ymax-ymin);

                    y = std::min(std::max(y, 0.0), 1.0);

                    if(m_params_current.colormap_reversed)
                        y = 1.0-y;

                    m_params_current.imgstft->setPixel(QPoint(si,m_params_current.stftparams.dftlen/2-n), cmap(y));
                }
                emit stftProgressing(int(100*double(si)/m_params_current.stftparams.snd->m_stft.size()));
            }

            m_params_current.stftparams.snd->m_stft_min = std::max(-2.0*20*std::log10(std::pow(2,m_params_current.stftparams.snd->format().sampleSize())), m_params_current.stftparams.snd->m_stft_min);
    //        COUTD << "STFTComputeThread::run compute finished" << std::endl;
        }

        canceled = gMW->ui->pbSTFTComputingCancel->isChecked();
        if(gMW->ui->pbSTFTComputingCancel->isChecked()){
            m_mutex_changingparams.lock();
            if(m_params_current.stftparams.snd->m_stftparams != m_params_current.stftparams) {
                m_params_current.stftparams.snd->m_stft.clear();
                m_params_current.stftparams.snd->m_stftts.clear();
                m_params_current.stftparams.snd->m_stftparams.clear();
            }
            m_mutex_changingparams.unlock();
//            m_params_current.snd->m_stft_lastupdate = QTime();
//            m_params_current.snd->m_stftimg_lastupdate = QTime();
            gMW->ui->pbSTFTComputingCancel->setChecked(false);
        }

//        COUTD << "STFTComputeThread::run check for computing again ..." << std::endl;
        // Check if it has to compute another
        m_mutex_changingparams.lock();
        if(!m_params_todo.isEmpty()){
//            COUTD << "STFTComputeThread::run something to compute again !" << std::endl;

            // m_mutex_computing.unlock();
            m_params_current = m_params_todo;
            m_params_todo.clear();
            m_mutex_changingparams.unlock();
        }
        else{
//            COUTD << "STFTComputeThread::run nothing else to compute" << std::endl;
            // m_mutex_computing.unlock();
            m_params_current.clear();
            m_mutex_changingparams.unlock();
            compute = false;
        }

//        COUTD << "STFTComputeThread::run while ..." << std::endl;
    }
    while(compute);

//    COUTD << "STFTComputeThread::run m_mutex_computing.unlock " << std::endl;
    m_mutex_computing.unlock();

//    COUTD << "STFTComputeThread::run emit " << std::endl;
    emit stftComputingStateChanged(SCSIdle);
    emit stftFinished(canceled);

//    COUTD << "STFTComputeThread::~run" << std::endl;
}
