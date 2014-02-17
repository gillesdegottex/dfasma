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

#ifndef _fftwrapper_h_
#define _fftwrapper_h_

#include <complex>
#include <vector>
#include <deque>
#ifdef FFT_FFTW3
    #include <fftw3.h>
#elif FFT_FFTREAL
    #include "external/FFTReal/FFTReal.h"
#endif

class FFTwrapper
{
	int m_size;
    bool m_forward;

#ifdef FFT_FFTW3
    fftw_plan m_fftw3_plan;
    fftw_complex *m_fftw3_in, *m_fftw3_out;
#elif FFT_FFTREAL
    ffft::FFTReal<double> *m_fftreal_fft;
    double* m_fftreal_out;
#endif

public:
    FFTwrapper(bool forward=true);
    FFTwrapper(int n);
	void resize(int n);

	int size(){return m_size;}

	std::vector<double> in;
	std::vector<std::complex<double> > out;

	void execute(const std::vector<double>& in, std::vector<std::complex<double> >& out);
	void execute();

    ~FFTwrapper();
};

#endif
