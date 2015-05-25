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

#include "sigproc.h"
using namespace sigproc;

#include <assert.h>
#include <iostream>
using namespace std;

#include "qthelper.h"


#ifdef FFT_FFTW3
#define FFTW_VERSION "3" // Used interface's version
QMutex FFTwrapper::m_fftw3_planner_access; // To protect the access to the FFT and external variables
#ifdef SIGPROC_FLOAT
#define fftwg_set_timelimit fftwf_set_timelimit
#define fftwg_plan_dft_r2c_1d fftwf_plan_dft_r2c_1d
#define fftwg_plan_dft_c2r_1d fftwf_plan_dft_c2r_1d
#define fftwg_execute fftwf_execute
#define fftwg_destroy_plan fftwf_destroy_plan
#define fftwg_free fftwf_free
#else
#define fftwg_set_timelimit fftw_set_timelimit
#define fftwg_plan_dft_r2c_1d fftw_plan_dft_r2c_1d
#define fftwg_plan_dft_c2r_1d fftw_plan_dft_c2r_1d
#define fftwg_execute fftw_execute
#define fftwg_destroy_plan fftw_destroy_plan
#define fftwg_free fftw_free
#endif
#elif FFT_FFTREAL
    #define FFTREAL_VERSION "2.11" // This is the current built-in version
#endif


QString FFTwrapper::getLibraryInfo(){
    #ifdef FFT_FFTW3
        return QString("<a href=\"http://www.fftw.org\">FFTW")+QString(FFTW_VERSION)+"</a>";
    #elif FFT_FFTREAL
        return QString("<a href=\"http://ldesoras.free.fr/prod.html#src_audio\">FFTReal</a> version ")+QString(FFTREAL_VERSION);
    #endif
}

FFTwrapper::FFTwrapper(bool forward)
{
    m_size = -1;
    m_forward = forward;

    #ifdef FFT_FFTW3
        m_fftw3_out = NULL;
        m_fftw3_in = NULL;
        m_fftw3_plan = NULL;
        #ifdef FFTW3RESIZINGMAXTIMESPENT
            fftwg_set_timelimit(1.0); // From FFTW 3.1, no means exist to check version at compile time ...
        #endif
    #elif FFT_FFTREAL
        m_fftreal_out = NULL;
        m_fftreal_fft = NULL;
    #endif
}
void FFTwrapper::setTimeLimitForPlanPreparation(float t){
    #ifdef FFT_FFTW3
        #ifdef FFTW3RESIZINGMAXTIMESPENT
            fftwg_set_timelimit(t); // From FFTW 3.1, no means exist to check version at compile time ...
        #else
            // Do nothing
        #endif
    #endif
}

FFTwrapper::FFTwrapper(int n)
{
	resize(n);
}
void FFTwrapper::resize(int n)
{
    assert(n>0);

    if(n==m_size) return;

    m_size = n;

    #ifdef FFT_FFTW3

        delete[] m_fftw3_in;
        delete[] m_fftw3_out;
        m_fftw3_in = NULL;
        m_fftw3_out = NULL;

        m_fftw3_in = new FFTTYPE[m_size];
        m_fftw3_out = new fftwg_complex[m_size/2+1];
        m_fftw3_planner_access.lock();
        //  | FFTW_PRESERVE_INPUT
        unsigned int flags = FFTW_ESTIMATE;
        // The following is likely to generate non-deterministic runs !
        // See: http://www.fftw.org/faq/section3.html#nondeterministic
        // unsigned int flags = FFTW_MEASURE;
        if(m_forward){
            m_fftw3_plan = fftwg_plan_dft_r2c_1d(m_size, m_fftw3_in, m_fftw3_out, flags);
//            m_fftw3_plan = fftw_plan_dft_1d(m_size, m_fftw3_in, m_fftw3_out, FFTW_FORWARD, FFTW_MEASURE);
        }
        else{
            m_fftw3_plan = fftwg_plan_dft_c2r_1d(m_size, m_fftw3_out, m_fftw3_in, flags);
//            m_fftw3_plan = fftw_plan_dft_1d(m_size, m_fftw3_in, m_fftw3_out, FFTW_BACKWARD, FFTW_MEASURE);
        }
        m_fftw3_planner_access.unlock();

    #elif FFT_FFTREAL
        delete[] m_fftreal_out;
        m_fftreal_out = NULL;

        m_fftreal_fft = new ffft::FFTReal<FFTTYPE>(m_size);
        m_fftreal_out = new FFTTYPE[m_size];
    #endif

    in.resize(m_size);
    out.resize((m_size%2==1)?(m_size-1)/2+1:m_size/2+1);
}

void FFTwrapper::execute(bool useinternalcopy) {
    if(useinternalcopy)
        execute(in, out);
    else {
#ifdef FFT_FFTW3
        m_fftw3_planner_access.lock();
        fftwg_execute(m_fftw3_plan);
        m_fftw3_planner_access.unlock();
#elif FFT_FFTREAL
        m_fftreal_fft->do_fft(m_fftreal_out, &(in[0]));
#endif
    }
}

void FFTwrapper::execute(const vector<FFTTYPE>& in, vector<std::complex<FFTTYPE> >& out) {
    int neededsize = (m_size%2==1)?(m_size-1)/2+1:m_size/2+1;
    if(int(out.size())!=neededsize){
        out.resize(neededsize);
    }

    #ifdef FFT_FFTW3
        for(int i=0; i<m_size; i++)
            m_fftw3_in[i] = in[i];

        m_fftw3_planner_access.lock();
        fftwg_execute(m_fftw3_plan);
        m_fftw3_planner_access.unlock();

        for(size_t i=0; i<out.size(); i++)
            out[i] = make_complex(m_fftw3_out[i]);

    #elif FFT_FFTREAL
        if (m_forward) // DFT
            m_fftreal_fft->do_fft(m_fftreal_out, &(in[0]));
        else           // IDFT
            assert(false); // TODO

        out[0] = m_fftreal_out[0]; // DC
        // TODO manage odd size ?
        for(int f=1; f<m_size/2; f++)
            out[f] = make_complex(m_fftreal_out[f], -m_fftreal_out[m_size/2+f]);
        out[m_size/2] = m_fftreal_out[m_size/2]; // Nyquist

    #endif
}

FFTwrapper::~FFTwrapper()
{
    #ifdef FFT_FFTW3
        m_fftw3_planner_access.lock();
        if(m_fftw3_plan){
            fftwg_destroy_plan(m_fftw3_plan);
        }
        if(m_fftw3_in){
//            fftwg_free(m_fftw3_in);
            delete[] m_fftw3_in;
        }
        if(m_fftw3_out){
            fftwg_free(m_fftw3_out);
//            delete[] m_fftw3_out;
        }
        m_fftw3_planner_access.unlock();
    #elif FFT_FFTREAL
        if(m_fftreal_fft)	delete m_fftreal_fft;
        if(m_fftreal_out)	delete[] m_fftreal_out;
    #endif
}


void sigproc::hspec2rcc(const std::vector<FFTTYPE>& loghA, FFTwrapper* fft, std::vector<FFTTYPE>& cc){
    int dftlen = (loghA.size()-1)*2;

    fft->setInput(0, loghA[0]);
//    fft->in[0] = loghA[0];
    for(int n=1; n<int(loghA.size())-1; ++n){
        fft->setInput(n, loghA[n]);
        fft->setInput(dftlen/2+n, loghA[int(loghA.size())-1-n]);
//        fft->in[n] = loghA[n];
//        fft->in[dftlen/2+n] = loghA[int(loghA.size())-1-n];
    }
    fft->setInput(dftlen/2, loghA[dftlen/2]);
//    fft->in[dftlen/2] = loghA[dftlen/2];

    fft->execute(false);

    cc.resize(dftlen/2+1);
    cc[0] = fft->getDCOutput().real()/dftlen;
//    cc[0] = fft->out[0].real()/dftlen;
    for(size_t n=1; n<cc.size(); ++n)
        cc[n] = 2*fft->getMidOutput(n).real()/dftlen; // Compensate for the energy loss
//    cc[n] = 2*fft->out[n].real()/dftlen; // Compensate for the energy loss
    cc[dftlen/2] = fft->getNyquistOutput().real()/dftlen;
//    cc[dftlen/2] = fft->out[dftlen/2].real()/dftlen;

}
void sigproc::rcc2hspec(const std::vector<FFTTYPE>& cc, FFTwrapper* fft, std::vector<FFTTYPE>& loghA){
    int dftlen = (loghA.size()-1)*2;

    fft->resize(dftlen);

    size_t n=0;
    for(; n<cc.size(); ++n)
        fft->setInput(n, cc[n]);
//        fft->in[n] = cc[n];
    for(; n<size_t(dftlen); ++n)
        fft->setInput(n, 0.0);
//        fft->in[n] = 0.0;

    fft->execute(false);
//    fft->execute();

    loghA.resize(dftlen/2+1);

    loghA[0] = fft->getDCOutput().real();
    for(size_t n=1; n<loghA.size()-1; ++n)
        loghA[n] = fft->getOutput(n).real();
    loghA[dftlen/2] = fft->getNyquistOutput().real();
//        loghA[n] = fft->out[n].real();
}


namespace sigproc{
    double elc_f[29] = {20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500};

    double elc_af[29] = {0.532, 0.506, 0.480, 0.455, 0.432, 0.409, 0.387, 0.367, 0.349, 0.330, 0.315, 0.301, 0.288, 0.276, 0.267, 0.259, 0.253, 0.250, 0.246, 0.244, 0.243, 0.243, 0.243, 0.242, 0.242, 0.245, 0.254, 0.271, 0.301};

    double elc_Lu[29] = {-31.6, -27.2, -23.0, -19.1, -15.9, -13.0, -10.3, -8.1, -6.2, -4.5, -3.1, -2.0, -1.1, -0.4, 0.0, 0.3, 0.5, 0.0, -2.7, -4.1, -1.0, 1.7, 2.5, 1.2, -2.1, -7.1, -11.2, -10.7, -3.1};

    double elc_Tf[29] = {78.5, 68.7, 59.5, 51.1, 44.0, 37.5, 31.5, 26.5, 22.1, 17.9, 14.4, 11.4, 8.6, 6.2, 4.4, 3.0, 2.2, 2.4, 3.5, 1.7, -1.3, -4.2, -6.0, -5.4, -1.5, 6.0, 12.6, 13.9, 12.3};
}

// freq: [Hz]
// Ln: phone value [Phon]
double sigproc::equalloudnesscurvesISO226(double freq, double Ln){

    if(freq<20 || freq>20000)
        return +100000;
//        return +std::numeric_limits<double>::infinity();

//    double fmin = std::numeric_limits<double>::infinity();
    size_t ind = 0;
    while(ind<29 && freq>elc_f[ind])
        ind++;

    double Tf, Lu, af;
    if(ind<=0){
        Tf = elc_Tf[0];
        Lu = elc_Lu[0];
        af = elc_af[0];
    }
    else if(ind>=29){
        Tf = elc_Tf[28];
        Lu = elc_Lu[28];
        af = elc_af[28];
    }
    else if(freq==elc_f[ind]){
        Tf = elc_Tf[ind];
        Lu = elc_Lu[ind];
        af = elc_af[ind];
    }
    else{
        double g = (freq-elc_f[ind-1])/(elc_f[ind]-elc_f[ind-1]);
        Tf = (1-g)*elc_Tf[ind-1] + g*elc_Tf[ind];
        Lu = (1-g)*elc_Lu[ind-1] + g*elc_Lu[ind];
        af = (1-g)*elc_af[ind-1] + g*elc_af[ind];
    }

    double Af = 4.47e-3 * (std::pow(10, 0.025*Ln) - 1.14) + std::pow(0.4*std::pow(10, ((Tf+Lu)/10)-9), af);

    double Lp = ((10.0/af)*std::log10(Af)) - Lu + 94;

    // Add a slope of 3dB/1kHz above the max freq
    // This is completely arbitrary because there ISO does not define anything
    // above 12500Hz ...
    if(freq>elc_f[28])
        Lp += (3.0/1000)*(freq-elc_f[28]);

    return Lp;
}
