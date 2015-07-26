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

    bool m_computing;

    void run(); //Q_DECL_OVERRIDE

public:
    enum STFTComputingState {SCSIdle, SCSDFT, SCSIMG, SCSFinished, SCSCanceled, SCSMemoryFull};
    void cancelComputation(FTSound* snd);

    inline bool isComputing() const {return m_computing;}

signals:
    void stftComputingStateChanged(int state);
    void stftProgressing(int);

public slots:
    void cancelComputation(bool waittoend=false);

public:
    STFTComputeThread(QObject* parent);

    class STFTParameters{
    public:
        bool computestft;// Need to keep ?

        // STFT related
        FTSound* snd;
        FFTTYPE ampscale; // [linear]
        qint64 delay;   // [sample index]
        std::vector<FFTTYPE> win;
        int stepsize;
        int dftlen;
        int cepliftorder;
        bool cepliftpresdc;

        void clear(){
            computestft = true;
            snd = NULL;
            ampscale = 1.0;
            delay = 0;
            win.clear();
            stepsize = -1;
            dftlen = -1;
            cepliftorder = -1;
            cepliftpresdc = false;
        }

        STFTParameters(){
            clear();
        }
        STFTParameters(FTSound* reqnd, const std::vector<FFTTYPE>& reqwin, int reqstepsize, int reqdftlen, int reqcepliftorder, bool reqcepliftpresdc);

//        bool is_stftpart_equal(const Parameters& param) const;
        bool operator==(const STFTParameters& param) const;
        bool operator!=(const STFTParameters& param) const{
            return !((*this)==param);
        }

        inline bool isEmpty() const {return snd==NULL;}
    };

    class ImageParameters{
    public:
        STFTParameters stftparams;
        QImage* imgstft;
        int colormap_index;
        bool colormap_reversed;
        FFTTYPE lower;
        FFTTYPE upper;
        bool loudnessweighting;

        void clear(){
            stftparams.clear();
            colormap_index = -1;
            colormap_reversed = false;
            lower = -1;
            upper = -1;
            loudnessweighting = false;
        }

        ImageParameters(){
            clear();
        }
        ImageParameters(STFTComputeThread::STFTParameters reqSTFTparams, QImage* reqImgSTFT, int reqcolormap_index, bool reqcolormap_reversed, FFTTYPE reqlower, FFTTYPE requpper, bool reqloudnessweighting){
            clear();
            stftparams = reqSTFTparams;
            imgstft = reqImgSTFT;
            colormap_index = reqcolormap_index;
            colormap_reversed = reqcolormap_reversed;
            lower = reqlower;
            upper = requpper;
            loudnessweighting = reqloudnessweighting;
        }

        bool operator==(const ImageParameters& param){
            if(stftparams!=param.stftparams)
                return false;
            if(imgstft!=param.imgstft)
                return false;
            if(colormap_index!=param.colormap_index)
                return false;
            if(colormap_reversed!=param.colormap_reversed)
                return false;
            if(lower!=param.lower)
                return false;
            if(upper!=param.upper)
                return false;
            if(loudnessweighting!=param.loudnessweighting)
                return false;

            return true;
        }
        bool operator!=(const ImageParameters& param){
            return !((*this)==param);
        }

        inline bool isEmpty(){return stftparams.isEmpty() || colormap_index==-1;}
    };


    void compute(ImageParameters reqImgParams);     // Entry point

    mutable QMutex m_mutex_computing;      // To protect the access to the FFT and external variables
    mutable QMutex m_mutex_changingparams; // To protect the access to the parameters above
    mutable QMutex m_mutex_imageallocation; // To protect the access to the image when allocating

    inline const ImageParameters& getCurrentParameters() const {return m_params_current;}

    ImageParameters m_params_todo;      // The params which has to be done by the thread
    ImageParameters m_params_current;   // The params which is in preparation by the thread
    ImageParameters m_params_last;      // The last params which have been done
};

#endif // STFTCOMPUTETHREAD_H
