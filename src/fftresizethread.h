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

#ifndef FFTRESIZETHREAD_H
#define FFTRESIZETHREAD_H

#include <QThread>
#include <QMutex>

#include "qaesigproc.h"

class FFTResizeThread : public QThread
{
    Q_OBJECT

    qae::FFTwrapper* m_fft;   // The FFT transformer

    int m_size_resizing;// The size which is in preparation by FFTResizeThread
    int m_size_todo;    // The next size which has to be done by FFTResizeThread asap

    void run(); //Q_DECL_OVERRIDE

signals:
    void fftResizing(int prevSize, int newSize);
    void fftResized(int prevSize, int newSize);

public:
    FFTResizeThread(qae::FFTwrapper* fft, QObject* parent);

    void resize(int newsize); // Entry point

    void cancelCurrentComputation(bool waittoend=false);

    mutable QMutex m_mutex_resizing;      // To protect the access to the FFT transformer
    mutable QMutex m_mutex_changingsizes; // To protect the access to the size variables above
};

#endif // FFTRESIZETHREAD_H
