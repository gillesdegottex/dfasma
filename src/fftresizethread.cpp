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

#include "fftresizethread.h"

#include "qaesigproc.h"

#include "qaehelpers.h"
#include <QTextStream>

FFTResizeThread::FFTResizeThread(qae::FFTwrapper* fft, QObject* parent)
    : QThread(parent)
    , m_fft(fft)
    , m_size_resizing(-1)
    , m_size_todo(-1)
{
//    setPriority(QThread::IdlePriority);
}

void FFTResizeThread::cancelCurrentComputation(bool waittoend) {
    if(waittoend) {
        m_mutex_resizing.lock();
        m_mutex_resizing.unlock();
    }
}

void FFTResizeThread::resize(int newsize) {

    m_mutex_changingsizes.lock();
//    COUTD << "FFTResizeThread::resize: m_mutex_changingsizes.locked()" << std::endl;

//    std::cout << "FFTResizeThread::resize resizing=" << m_size_resizing << " m_size_todo=" << m_size_todo << " requested=" << newsize << std::endl;

    if(m_mutex_resizing.tryLock()){
        // The thread is not resizing

        if(newsize==m_fft->size()){
            m_mutex_resizing.unlock(); // If the size is already the good one, just give up.
        }
        else{
            m_size_resizing = newsize;
//            COUTD << "Start resizing " << m_fft->size() << "=>" << m_size_resizing << std::endl;
            emit fftResizing(m_fft->size(), m_size_resizing);
            while(isRunning()) // Wait for any previous run to end, otherwise
                msleep(10);    // start() does nothing.

            start();
        }
    }
    else{
        // The thread is already resizing

        if(newsize!=m_size_resizing) {
            m_size_todo = newsize;  // Ask to resize again, once the current resizing is finished
//            COUTD << "Change resizing " << m_size_resizing << "=>" << m_size_todo << std::endl;
            emit fftResizing(m_size_resizing, m_size_todo); // This also gives the impression to the user that the FFT is resizing to m_size_todo whereas, at this point, it is still resizing to m_size_resizing
        }
    }

//    COUTD << "FFTResizeThread::resize: m_mutex_changingsizes.unlocking()" << std::endl;
    m_mutex_changingsizes.unlock();
}

void FFTResizeThread::run() {
//    COUTD << "FFTResizeThread::run" << std::endl;

    int prevSize = -1;

    bool resize = true;
    do{
        prevSize = m_fft->size();

//        COUTD << "FFTResizeThread::run " << prevSize << "=>" << m_size_resizing << std::endl;
//        COUTD << "FFTResizeThread::run ask resize" << std::endl;
        m_fft->resize(m_size_resizing);
//        COUTD << "FFTResizeThread::run resize finished" << std::endl;

        // Check if it has to be resized again
        m_mutex_changingsizes.lock();
//        COUTD << "FFTResizeThread::run: m_mutex_changingsizes.locked()" << endl;
        if(m_size_todo!=-1){
//            m_mutex_resizing.unlock();
            m_size_resizing = m_size_todo;
            m_size_todo = -1;
    //        emit fftResizing(m_cfftw3->size(), m_size_resizing);
//            COUTD << "FFTResizeThread::run: m_mutex_changingsizes.unlocking()" << std::endl;
            m_mutex_changingsizes.unlock();
//            COUTD << "FFTResizeThread::run continue resizing " << m_size_resizing << std::endl;
        }
        else{
//            m_mutex_resizing.unlock();
            m_size_resizing = -1;
            resize = false;
        }
    }
    while(resize);
    m_mutex_resizing.unlock();

//    COUTD << "FFTResizeThread::run emit fftResized(" << prevSize << "," << m_fft->size() << ")" << std::endl;
    emit fftResized(prevSize, m_fft->size());

//    COUTD << "FFTResizeThread::~run m_size_resizing=" << m_size_resizing << " m_size_todo=" << m_size_todo << std::endl;

//    COUTD << "FFTResizeThread::run: m_mutex_changingsizes.unlocking()" << std::endl;
    m_mutex_changingsizes.unlock();
}
