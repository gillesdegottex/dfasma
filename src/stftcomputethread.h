#ifndef STFTCOMPUTETHREAD_H
#define STFTCOMPUTETHREAD_H

#include <QThread>
#include <QMutex>

#include "sigproc.h"

class FTSound;

class STFTComputeThread : public QThread
{
    Q_OBJECT

    sigproc::FFTwrapper* m_fft;   // The FFT transformer

    void run(); //Q_DECL_OVERRIDE

signals:
    void stftComputing();
    void stftComputed();

public:
    STFTComputeThread(QObject* parent);

    class Parameters{
    public:
        FTSound* snd;
        std::vector<FFTTYPE> win;
        int stepsize;
        int dftlen;

        void clear(){
            snd = NULL;
            win.clear();
            stepsize = -1;
            dftlen = -1;
        }

        Parameters(){
            clear();
        }

        bool operator==(const Parameters& param){
            if(snd!=param.snd)
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
        }
        bool operator!=(const Parameters& param){
            return !((*this)==param);
        }

        inline bool isEmpty(){return snd==NULL;}
    };

    void compute(FTSound* snd, const std::vector<FFTTYPE>& win, int stepsize, int dftlen);     // Entry point

    QMutex m_mutex_computing;      // To protect the access to the FFT and external variables
    QMutex m_mutex_changingparams; // To protect the access to the parameters above

private:
    Parameters m_params_current;   // The params which are in preparation by the thread
    Parameters m_params_todo;      // The params which have to be done by the thread
};

#endif // STFTCOMPUTETHREAD_H
