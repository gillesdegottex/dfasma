#include "fftresizethread.h"

#include "sigproc.h"

FFTResizeThread::FFTResizeThread(sigproc::FFTwrapper* fft, QObject* parent)
    : QThread(parent)
    , m_fft(fft)
    , m_size_resizing(-1)
    , m_size_todo(-1)
{
//    setPriority(QThread::IdlePriority);
}

/*int FFTResizeThread::size() {

    m_mutex_changingsizes.lock();

    int s = m_size_current;

    m_mutex_changingsizes.unlock();

    return s;
}*/

void FFTResizeThread::resize(int newsize) {

    m_mutex_changingsizes.lock();

//    std::cout << "FFTResizeThread::resize resizing=" << m_size_resizing << " todo=" << newsize << std::endl;

    if(m_mutex_resizing.tryLock()){
        // Not resizing

        if(newsize==m_fft->size()){
            m_mutex_resizing.unlock();
        }
        else{
            m_size_resizing = newsize;
            emit fftResizing(m_fft->size(), m_size_resizing);
            start();
        }
    }
    else{
        // Resizing

        if(newsize!=m_size_resizing) {
            m_size_todo = newsize;  // Ask to resize again, once the current resizing is finished
            emit fftResizing(m_size_resizing, m_size_todo); // This also gives the impression to the user that the FFT is resizing to m_size_todo whereas, at this point, it is still resizing to m_size_resizing
        }
    }

//    std::cout << "~FFTResizeThread::resize" << std::endl;

    m_mutex_changingsizes.unlock();
}

void FFTResizeThread::run() {
//    std::cout << "FFTResizeThread::run" << std::endl;

    bool resize = true;
    do{
        int prevSize = m_fft->size();

//        std::cout << "FFTResizeThread::run " << prevSize << "=>" << m_size_resizing << std::endl;

        m_fft->resize(m_size_resizing);

//        std::cout << "FFTResizeThread::run resize finished" << std::endl;

        // Check if it has to be resized again
        m_mutex_changingsizes.lock();
        if(m_size_todo!=-1){
//            m_mutex_resizing.unlock();
            m_size_resizing = m_size_todo;
            m_size_todo = -1;
            m_mutex_changingsizes.unlock();
//            std::cout << "FFTResizeThread::run emit fftResizing " << m_size_resizing << std::endl;
    //        emit fftResizing(m_cfftw3->size(), m_size_resizing);
        }
        else{
//            m_mutex_resizing.unlock();
            m_size_resizing = -1;
            m_mutex_changingsizes.unlock();
            emit fftResized(prevSize, m_fft->size());
            resize = false;
        }

    }
    while(resize);
    m_mutex_resizing.unlock();

//    std::cout << "FFTResizeThread::~run m_size_resizing=" << m_size_resizing << " m_size_todo=" << m_size_todo << std::endl;
}
