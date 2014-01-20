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

#ifndef _CFFTw3_h_
#define _CFFTw3_h_

#include <complex>
#include <vector>
#include <deque>
#include <fftw3.h>

class CFFTW3
{
	int m_size;

	fftw_plan m_plan;
	fftw_complex *m_in, *m_out;
	bool m_forward;

  public:
	CFFTW3(bool forward=true);
	CFFTW3(int n);
	void resize(int n);

	int size(){return m_size;}

	std::vector<double> in;
	std::vector<std::complex<double> > out;

	void execute(const std::vector<double>& in, std::vector<std::complex<double> >& out);
	void execute();

	~CFFTW3();
};

#endif
