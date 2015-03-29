#include "stftcomputethread.h"

#include "sigproc.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ftsound.h"
#include "qthelper.h"


STFTComputeThread::Parameters::Parameters(FTSound* reqnd, const std::vector<FFTTYPE>& reqwin, int reqstepsize, int reqdftlen){
    clear();
    snd = reqnd;
    ampscale = reqnd->m_ampscale;
    delay = reqnd->m_delay;
    win = reqwin;
    stepsize = reqstepsize;
    dftlen = reqdftlen;
}

bool STFTComputeThread::Parameters::operator==(const Parameters& param){
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

void STFTComputeThread::compute(FTSound* snd, const std::vector<FFTTYPE>& win, int stepsize, int dftlen) {

    if(win.size()<2)
        throw QString("Window's length is too short");

    m_mutex_changingparams.lock();

//    std::cout << "STFTComputeThread::compute winlen=" << winlen << " stepsize=" << stepsize << " dftlen=" << dftlen << std::endl;

    Parameters reqparams(snd, win, stepsize, dftlen);

    if(m_mutex_computing.tryLock()){
        // Currently not computing, but it will be very soon ...

        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->show();

        m_params_current = reqparams;
        emit stftComputing();
        start(); // Start computing
    }
    else{
        // Currently already computing something

        if(reqparams!=m_params_current) {
            m_params_todo = reqparams;  // Ask to compute again, once the current computation is finished
            emit stftComputing(); // This also gives the impression to the user that the FFT is computing m_parameters_todo whereas, at this point, it is still computing resizing m_params_current
        }
    }

//    std::cout << "~STFTComputeThread::compute" << std::endl;

    m_mutex_changingparams.unlock();
}

void STFTComputeThread::run() {
//    COUTD << "STFTComputeThread::run" << std::endl;

    bool compute = true;
    do{
        m_params_current.snd->m_stftparams = m_params_current;

        m_fft->resize(m_params_current.dftlen);

        qreal gain = m_params_current.snd->m_ampscale;

        std::vector<WAVTYPE>* wav = m_params_current.snd->wavtoplay;

//        COUTD << "STFTComputeThread::run resize finished" << std::endl;

        m_params_current.snd->m_stft.clear();
        m_params_current.snd->m_stftts.clear();

        m_params_current.snd->m_stft_min = std::numeric_limits<double>::infinity();
        m_params_current.snd->m_stft_max = -std::numeric_limits<double>::infinity();

        int maxsampleindex = int(wav->size())-1 + int(m_params_current.snd->m_delay);

        for(int si=0; !gMW->ui->pbSTFTComputingCancel->isChecked(); si++){
            if(int(si*m_params_current.stepsize+m_params_current.win.size())-1 > maxsampleindex)
                break;

            m_params_current.snd->m_stft.push_back(std::vector<WAVTYPE>(m_params_current.dftlen/2+1));
            m_params_current.snd->m_stftts.push_back((si*m_params_current.stepsize+(m_params_current.win.size()-1)/2.0)/m_params_current.snd->fs);

            int n = 0;
            int wn = 0;
            bool hasfilledvalues = false;
            for(; n<int(m_params_current.win.size()); n++){
                wn = si*m_params_current.stepsize+n - m_params_current.snd->m_delay;
                if(wn>=0 && wn<int(wav->size())) {
                    WAVTYPE value = gain*(*(wav))[wn];

                    if(value>1.0)       value = 1.0;
                    else if(value<-1.0) value = -1.0;

                    m_fft->in[n] = value*m_params_current.win[n];
                    hasfilledvalues = true;
                }
                else
                    m_fft->in[n] = 0.0;
            }

            if(hasfilledvalues){
                for(; n<m_params_current.dftlen; n++)
                    m_fft->in[n] = 0.0;

                m_fft->execute(); // Compute the DFT

                for(n=0; n<m_params_current.dftlen/2+1; n++) {
                    double y = sigproc::log2db*std::log(std::abs(m_fft->out[n]));

                    m_params_current.snd->m_stft[si][n] = y;

                    m_params_current.snd->m_stft_min = std::min(m_params_current.snd->m_stft_min, y);
                    m_params_current.snd->m_stft_max = std::max(m_params_current.snd->m_stft_max, y);
                }
            }
            else{
                for(n=0; n<m_params_current.dftlen/2+1; n++)
                    m_params_current.snd->m_stft[si][n] = -std::numeric_limits<double>::infinity();
            }

            emit stftProgressing(int(100*double(si*m_params_current.stepsize)/maxsampleindex));
        }

        if(gMW->ui->pbSTFTComputingCancel->isChecked()){
            m_params_current.snd->m_stft.clear();
            m_params_current.snd->m_stftts.clear();
            m_params_current.snd->m_stftparams.clear();
        }

        m_params_current.snd->m_stft_min = std::max(-2.0*20*std::log10(std::pow(2,m_params_current.snd->format().sampleSize())), m_params_current.snd->m_stft_min);
//        COUTD << "STFTComputeThread::run compute finished" << std::endl;

//        COUTD << "STFTComputeThread::run check for computing again ..." << std::endl;
        // Check if it has to be computed again
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
    emit stftFinished(gMW->ui->pbSTFTComputingCancel->isChecked());

    gMW->ui->pbSTFTComputingCancel->setChecked(false);

//    COUTD << "STFTComputeThread::~run" << std::endl;
}
