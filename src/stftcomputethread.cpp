#include "stftcomputethread.h"

#include "sigproc.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ftsound.h"

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

    Parameters reqparams;
    reqparams.snd = snd;
    reqparams.win = win;
    reqparams.stepsize = stepsize;
    reqparams.dftlen = dftlen;
    reqparams.nbsteps = std::floor((WMainWindow::getMW()->getMaxLastSampleTime()*snd->fs - (reqparams.win.size()-1)/2)/stepsize);

    if(m_mutex_computing.tryLock()){
        // Currently not computing, but it will be very soon ...

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
//    std::cout << "STFTComputeThread::run" << std::endl;

    bool compute = true;
    do{
//        std::cout << "STFTComputeThread::run resizing" << std::endl;

        m_fft->resize(m_params_current.dftlen);

        std::vector<WAVTYPE>* wav = m_params_current.snd->wavtoplay;

//        std::cout << "STFTComputeThread::run resize finished" << std::endl;

        m_params_current.snd->m_stft.resize(m_params_current.nbsteps);

    //        m_imgSTFT = QImage(m_nbsteps, m_dftlen/2+1, QImage::Format_RGB32);

        WMainWindow::getMW()->ui->pgbSpectrogramSTFTCompute->hide();
        WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText(QString("DFT size=%1").arg(m_params_current.dftlen));

        m_params_current.snd->m_stft_min = std::numeric_limits<double>::infinity();
        m_params_current.snd->m_stft_max = -std::numeric_limits<double>::infinity();

        for(int si=0; si<m_params_current.nbsteps; si++){
            m_params_current.snd->m_stft[si].resize(m_params_current.dftlen/2+1);

            int n = 0;
            int wn = 0;
//                cout << si*stepsize/WMainWindow::getMW()->getFs() << endl;
            for(; n<m_params_current.win.size(); n++){
                wn = si*m_params_current.stepsize+n - m_params_current.snd->m_delay;
                if(wn>=0 && wn<int(wav->size())) // TODO temp
                    m_fft->in[n] = (*(wav))[wn]*m_params_current.win[n];
                else
                    m_fft->in[n] = 0.0;
            }
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
//        std::cout << "STFTComputeThread::run compute finished" << std::endl;

//        std::cout << "STFTComputeThread::run check for computing again ..." << std::endl;
        // Check if it has to be computed again
        m_mutex_changingparams.lock();
        if(!m_params_todo.isEmpty()){
//            std::cout << "STFTComputeThread::run something to compute again !" << std::endl;

            // m_mutex_computing.unlock();
            m_params_current = m_params_todo;
            m_params_todo.clear();
            m_mutex_changingparams.unlock();
        }
        else{
//            std::cout << "STFTComputeThread::run nothing else to compute" << std::endl;
            // m_mutex_computing.unlock();
            m_params_current.clear();
            m_mutex_changingparams.unlock();
            compute = false;
        }

//        std::cout << "STFTComputeThread::run while ..." << std::endl;
    }
    while(compute);
//    std::cout << "STFTComputeThread::run m_mutex_computing.unlock " << std::endl;
    m_mutex_computing.unlock();

//    std::cout << "STFTComputeThread::run emit " << std::endl;
    emit stftComputed();

//    std::cout << "STFTComputeThread::~run" << std::endl;
}
