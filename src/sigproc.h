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


#ifndef SIGPROC_H
#define SIGPROC_H

#include <vector>
#include <limits>

namespace sigproc {

// Interpolate value given time vector and corresponding data vector
template<typename DataType, typename ContainerTimes, typename ContainerData>
inline DataType nearest(const ContainerTimes& ts, const ContainerData& data, double t, const DataType& defvalue) {

    DataType d = defvalue;

    if(t <= ts.front())
        d = data.front();
    else if(t >= ts.back())
        d = data.back();
    else {
        typename ContainerTimes::const_iterator it = std::lower_bound(ts.begin(), ts.end(), t);

        size_t i = std::distance(ts.begin(), it);

        if(i<data.size())
            d = data[i];
    }

    return d;
}


inline std::vector<double> hann(int n)
{
    std::vector<double> win(n);

    for(size_t n=0; n<win.size(); n++)
        win[n] = (1-cos(2.0*M_PI*n/(win.size()-1))) / (win.size()-1);

    return win;
}

inline std::vector<double> hamming(int n)
{
    std::vector<double> win(n);

    double s = 0.0;
    for(size_t n=0; n<win.size(); n++) {
        win[n] = (0.54-0.46*cos(2*M_PI*n/(win.size()-1)));
        s += win[n];
    }
    for(size_t n=0; n<win.size(); n++)
        win[n] /= s;

    return win;
}

// TODO Blackman

// filtfilt using Direct Form II
template<typename DataType, typename ContainerIn, typename ContainerOut> inline void filtfilt(const ContainerIn& wav, const std::vector<double>& num, const std::vector<double>& den, ContainerOut& filteredwav, int nstart=-1, int nend=-1)
{
    size_t order = den.size()-1; // Filter order
    size_t maxorder = std::max(num.size(), den.size())+1;

    if (nstart==-1)
        nstart = 0;
    if (nend==-1)
        nend = wav.size()-1;

    filteredwav.resize(wav.size());

    std::deque<DataType> states(order+1, 0.0);


    // Extrapolate before nstart and after nend in order to reduce
    // boundary effects
    int margin = 200; // extrapolation segments are maring times bigger than the max order
    // Deal with begining and end

    // The exrapolated segmented will be windowed
    std::vector<double> win = sigproc::hann(margin*maxorder*2+1);
    double winmax = win[(win.size()-1)/2];
    for(size_t n=0; n<win.size(); n++)
        win[n] /= winmax;

    // Before nstart
    std::vector<DataType> preextrap(margin*maxorder, wav[nstart]);
    int n=0;
    // The following doesnt work when using multi-pass filtering like biquads
    // for(; nstart-1-n>=0 && n<preextrap.size(); ++n) // Fill with the values before nstart
    //     preextrap[preextrap.size()-1-n] = wav[nstart-1-n];
    // Add reversed signal in order to preserve the derivatives
    for(; nstart+n+1<wav.size() && n<preextrap.size(); ++n)
        preextrap[preextrap.size()-1-n] = 2*wav[nstart]-wav[nstart+1+n];
    for(int m=n; m<preextrap.size(); ++m)
        preextrap[preextrap.size()-1-m] = preextrap[preextrap.size()-1-n+1];
    // Add the window
    for(int n=0; n<preextrap.size(); ++n)
        preextrap[n] *= win[n];

    // After nend
    std::vector<DataType> postextrap(margin*maxorder, wav[nend]);
    n=0;
    // The following doesnt work when using multi-pass filtering like biquads
    // for(; nend+1+n<wav.size() && n<postextrap.size(); ++n) // Fill with the values after nend
    //     postextrap[n] = wav[nend+1+n];
    // Add reversed signal in order to preserve the derivatives
    for(; nend-n-1>=0 && n<postextrap.size(); ++n)
        postextrap[n] = 2*wav[nend]-wav[nend-n-1];
    for(int m=n; m<postextrap.size(); ++m)
        postextrap[m] = postextrap[n];
    // Add the window
    for(int n=0; n<postextrap.size(); ++n)
        postextrap[n] *= win[1+(win.size()-1)/2+n];


    // Forward pass --------------------------

    // Filter the pre-extrapolation
    for(size_t n=0; n<preextrap.size(); ++n) {

        states.pop_back();
        states.push_front(preextrap[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * states[k];

        preextrap[n] = f;
    }

    // Filter the input
    for(size_t n=nstart; n<=nend; ++n) {

        states.pop_back();
        states.push_front(wav[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * states[k];

        filteredwav[n] = f;
    }

    // Filter the post-extrapolation
    for(size_t n=0; n<postextrap.size(); ++n) {

        states.pop_back();
        states.push_front(postextrap[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * states[k];

        postextrap[n] = f;
    }

    // Backward pass --------------------------

    // Do not reset the states

    // Back-filter the post-extrapolation
    for(int n=postextrap.size()-1; n>=0; --n) {

        states.pop_back();
        states.push_front(postextrap[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * states[k];

        postextrap[n] = f;
    }

    // Back-filter the input
    for(int n=nend; n>=nstart; --n) {

        states.pop_back();
        states.push_front(filteredwav[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; k++)
            f += num[k] * states[k];

        filteredwav[n] = f;

        if(f!=f) throw QString("filter is numerically unstable");
    }

    // Back-filter the pre-extrapolation
    // (not really necessary)
    for(int n=preextrap.size()-1; n>=0; --n) {

        states.pop_back();
        states.push_front(preextrap[n]);

        for(size_t k=1; k<=order; ++k)
            states[0] += den[k] * states[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * states[k];

        preextrap[n] = f;
    }

//        int nn=0;
//////        for(int n=0;n<win.size(); n++)
//////            filteredwav[nn++] = win[n];
//        for(int n=0; n<preextrap.size(); n++)
//            filteredwav[nn++] = preextrap[n];
//        for(int n=nstart; n<=nend; n++)
//            filteredwav[nn++] = wav[n];
//        for(int n=0; n<postextrap.size(); n++)
//            filteredwav[nn++] = postextrap[n];
}

}

#endif // SIGPROC_H
