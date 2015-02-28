/* mkfilter -- given n, compute recurrence relation
   to implement Butterworth, Bessel or Chebyshev filter of order n
   A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
   September 1992 */

#include "mkfilter.h"

//using namespace mkfilter;

namespace mkfilter {

#include <math.h>
#include <iostream>
#include <QString>

typedef unsigned int uint;
#undef	PI
//#define PI	    3.14159265358979323846  /* Microsoft C++ does not define M_PI ! */
#define PI	    M_PI
#define TWOPI	(2.0 * M_PI)
#define EPS	    1e-10
#define MAXORDER    128
#define MAXPZ	    512	    /* .ge. 2*MAXORDER, to allow for doubling of poles in BP filter;
                   high values needed for FIR filters */

inline bool seq(char *s1, char *s2) {
    return strcmp(s1,s2) == 0;
}

inline bool onebit(uint m) {
    return (m != 0) && ((m & (m-1)) == 0);
}

// Microsoft C++ does not define
inline double asinh(double x) {
    return log(x + sqrt(1.0 + x*x));
}

// TODO Use STD for the below ? ------------------------------------------------

struct c_complex
  { double re, im;
  };

struct complex
  { double re, im;
    complex(double r, double i = 0.0) { re = r; im = i; }
    complex() { }					/* uninitialized complex */
    complex(c_complex z) { re = z.re; im = z.im; }	/* init from denotation */
  };

extern complex csqrt(complex), cexp(complex), expj(double);	    /* from complex.C */
//extern complex evaluate(complex[], int, complex[], int, complex);   /* from complex.C */

inline double hypot(complex z) { return ::hypot(z.im, z.re); }
inline double atan2(complex z) { return ::atan2(z.im, z.re); }

inline complex cconj(complex z)
  { z.im = -z.im;
    return z;
  }

inline complex operator * (double a, complex z)
  { z.re *= a; z.im *= a;
    return z;
  }

inline complex operator / (complex z, double a)
  { z.re /= a; z.im /= a;
    return z;
  }

inline void operator /= (complex &z, double a)
  { z = z / a;
  }

extern complex operator * (complex, complex);
extern complex operator / (complex, complex);

inline complex operator + (complex z1, complex z2)
  { z1.re += z2.re;
    z1.im += z2.im;
    return z1;
  }

inline complex operator - (complex z1, complex z2)
  { z1.re -= z2.re;
    z1.im -= z2.im;
    return z1;
  }

inline complex operator - (complex z)
  { return 0.0 - z;
  }

inline bool operator == (complex z1, complex z2)
  { return (z1.re == z2.re) && (z1.im == z2.im);
  }

inline complex sqr(complex z)
  { return z*z;
  }

static double Xsqrt(double x)
  { /* because of deficiencies in hypot on Sparc, it's possible for arg of Xsqrt to be small and -ve,
       which logically it can't be (since r >= |x.re|).	 Take it as 0. */
    return (x >= 0.0) ? sqrt(x) : 0.0;
  }

static complex eval(complex coeffs[], int npz, complex z)
  { /* evaluate polynomial in z, substituting for z */
    complex sum = complex(0.0);
    for (int i = npz; i >= 0; i--) sum = (sum * z) + coeffs[i];
    return sum;
  }

complex evaluate(complex topco[], int nz, complex botco[], int np, complex z)
  { /* evaluate response, substituting for z */
    return eval(topco, nz, z) / eval(botco, np, z);
  }

complex csqrt(complex x)
  { double r = hypot(x);
    complex z = complex(Xsqrt(0.5 * (r + x.re)),
            Xsqrt(0.5 * (r - x.re)));
    if (x.im < 0.0) z.im = -z.im;
    return z;
  }

complex cexp(complex z)
  { return exp(z.re) * expj(z.im);
  }

complex expj(double theta)
  { return complex(cos(theta), sin(theta));
  }

complex operator * (complex z1, complex z2)
  { return complex(z1.re*z2.re - z1.im*z2.im,
           z1.re*z2.im + z1.im*z2.re);
  }

complex operator / (complex z1, complex z2)
  { double mag = (z2.re * z2.re) + (z2.im * z2.im);
    return complex (((z1.re * z2.re) + (z1.im * z2.im)) / mag,
            ((z1.im * z2.re) - (z1.re * z2.im)) / mag);
  }

// TODO Use the STD for the above ?

#define opt_be 0x00001	/* -Be		Bessel characteristic	       */
#define opt_bu 0x00002	/* -Bu		Butterworth characteristic     */
#define opt_ch 0x00004	/* -Ch		Chebyshev characteristic       */
#define opt_re 0x00008	/* -Re		Resonator		       */
#define opt_pi 0x00010	/* -Pi		proportional-integral	       */

#define opt_lp 0x00020	/* -Lp		lowpass			       */
#define opt_hp 0x00040	/* -Hp		highpass		       */
#define opt_bp 0x00080	/* -Bp		bandpass		       */
#define opt_bs 0x00100	/* -Bs		bandstop		       */
#define opt_ap 0x00200	/* -Ap		allpass			       */

#define opt_a  0x00400	/* -a		alpha value		       */
#define opt_l  0x00800	/* -l		just list filter parameters    */
#define opt_o  0x01000	/* -o		order of filter		       */
#define opt_p  0x02000	/* -p		specified poles only	       */
#define opt_w  0x04000	/* -w		don't pre-warp		       */
#define opt_z  0x08000	/* -z		use matched z-transform	       */
#define opt_Z  0x10000	/* -Z		additional zero		       */

// multiply factor (z-w) into coeffs
static void multin(const complex w, int npz, complex coeffs[]) {
    complex nw = -w;
    for (int i = npz; i >= 1; i--)
        coeffs[i] = (nw * coeffs[i]) + coeffs[i-1];
    coeffs[0] = nw * coeffs[0];
}

// compute product of poles or zeros as a polynomial of z
static void expand(const complex pz[], int npz, complex coeffs[]) {
    int i;
    coeffs[0] = 1.0;
    for (i=0; i < npz; i++)
        coeffs[i+1] = 0.0;
    for (i=0; i < npz; i++)
        multin(pz[i], npz, coeffs);
    // check computed coeffs of z^k are all real
    for (i=0; i < npz+1; i++) {
        if (fabs(coeffs[i].im) > EPS)
            throw QString("MKFILTER: coeff of z^")+QString::number(i)+QString(" is not real (")+QString::number(coeffs[i].im)+QString("; poles/zeros are not complex conjugates");
    }
}

inline complex blt(complex pz) {
    return (2.0 + pz) / (2.0 - pz);
}

inline complex reflect(complex z)
  { double r = hypot(z);
    return z / sqr(r);
  }

// Below is not Scope-safe -----------------------------------------------------

struct pzrep
  { complex poles[MAXPZ], zeros[MAXPZ];
    int numpoles, numzeros;
  };

static uint options;
static int order;
static double raw_alpha1, raw_alpha2;
static uint polemask;

static pzrep splane;
static double warped_alpha1, warped_alpha2, chebrip, qfactor;
static bool infq;

static c_complex bessel_poles[] =
  { // table produced by /usr/fisher/bessel --	N.B. only one member of each C.Conj. pair is listed
    { -1.00000000000e+00, 0.00000000000e+00}, { -1.10160133059e+00, 6.36009824757e-01},
    { -1.32267579991e+00, 0.00000000000e+00}, { -1.04740916101e+00, 9.99264436281e-01},
    { -1.37006783055e+00, 4.10249717494e-01}, { -9.95208764350e-01, 1.25710573945e+00},
    { -1.50231627145e+00, 0.00000000000e+00}, { -1.38087732586e+00, 7.17909587627e-01},
    { -9.57676548563e-01, 1.47112432073e+00}, { -1.57149040362e+00, 3.20896374221e-01},
    { -1.38185809760e+00, 9.71471890712e-01}, { -9.30656522947e-01, 1.66186326894e+00},
    { -1.68436817927e+00, 0.00000000000e+00}, { -1.61203876622e+00, 5.89244506931e-01},
    { -1.37890321680e+00, 1.19156677780e+00}, { -9.09867780623e-01, 1.83645135304e+00},
    { -1.75740840040e+00, 2.72867575103e-01}, { -1.63693941813e+00, 8.22795625139e-01},
    { -1.37384121764e+00, 1.38835657588e+00}, { -8.92869718847e-01, 1.99832584364e+00},
    { -1.85660050123e+00, 0.00000000000e+00}, { -1.80717053496e+00, 5.12383730575e-01},
    { -1.65239648458e+00, 1.03138956698e+00}, { -1.36758830979e+00, 1.56773371224e+00},
    { -8.78399276161e-01, 2.14980052431e+00}, { -1.92761969145e+00, 2.41623471082e-01},
    { -1.84219624443e+00, 7.27257597722e-01}, { -1.66181024140e+00, 1.22110021857e+00},
    { -1.36069227838e+00, 1.73350574267e+00}, { -8.65756901707e-01, 2.29260483098e+00},
  };


void checkoptions(uint& options, int& order, uint& polemask, double& raw_alpha1, double& raw_alpha2) {
    bool optsok = true;
    if(!(onebit(options & (opt_be | opt_bu | opt_ch | opt_re | opt_pi))))
      throw QString("MKFILTER: must specify exactly one of -Be, -Bu, -Ch, -Re, -Pi");
    if (options & opt_re)
      { if(!((onebit(options & (opt_bp | opt_bs | opt_ap)))))
        throw QString("must specify exactly one of -Bp, -Bs, -Ap with -Re");
    if (options & (opt_lp | opt_hp | opt_o | opt_p | opt_w | opt_z))
      throw QString("MKFILTER: can't use -Lp, -Hp, -o, -p, -w, -z with -Re");
      }
    else if (options & opt_pi)
      { if (options & (opt_lp | opt_hp | opt_bp | opt_bs | opt_ap))
      throw QString("MKFILTER: -Lp, -Hp, -Bp, -Bs, -Ap illegal in conjunction with -Pi");
    if(!((options & opt_o) && (order == 1)))
        throw QString("MKFILTER: -Pi implies -o 1");
      }
    else
      { if(!(onebit(options & (opt_lp | opt_hp | opt_bp | opt_bs))))
      throw QString("MKFILTER: must specify exactly one of -Lp, -Hp, -Bp, -Bs");
    if (options & opt_ap)
        throw QString("MKFILTER: -Ap implies -Re");
    if (options & opt_o)
      { if(!(order >= 1 && order <= MAXORDER))
            throw QString("MKFILTER: order must be in range 1 .. ")+QString(MAXORDER);
        if (options & opt_p)
          { uint m = (1 << order) - 1; // "order" bits set
        if ((polemask & ~m) != 0)
          throw QString("MKFILTER: order=")+QString(order)+QString(", so args to -p must be in range 0 .. ")+QString(order-1);
          }
      }
    else
        throw QString("MKFILTER: must specify -o");
      }
    if(!(options & opt_a))
        throw QString("MKFILTER: must specify -a");
    if(!(optsok))
        throw QString("MKFILTER: Filter design options not consistent");

    // setdefaults()
    if(!(options & opt_p)) polemask = ~0; // use all poles
    if(!(options & (opt_bp | opt_bs))) raw_alpha2 = raw_alpha1;
}

inline void choosepole(complex z) {
    if (z.re < 0.0) {
        if (polemask & 1)
            splane.poles[splane.numpoles++] = z;
        polemask >>= 1;
    }
}

void compute_s() // compute S-plane poles for prototype LP filter
  { splane.numpoles = 0;
    if (options & opt_be)
      { // Bessel filter
    int p = (order*order)/4; // ptr into table
    if (order & 1) choosepole(bessel_poles[p++]);
    for (int i = 0; i < order/2; i++)
      { choosepole(bessel_poles[p]);
        choosepole(cconj(bessel_poles[p]));
        p++;
      }
      }
    if (options & (opt_bu | opt_ch))
      { // Butterworth filter
    for (int i = 0; i < 2*order; i++)
      { double theta = (order & 1) ? (i*PI) / order : ((i+0.5)*PI) / order;
        choosepole(expj(theta));
      }
      }
    if (options & opt_ch)
      { // modify for Chebyshev (p. 136 DeFatta et al.) (Type I)
    if (chebrip >= 0.0)
        throw QString("mkfilter: Chebyshev ripple is ")+QString::number(chebrip)+QString(" dB; must be < 0.0");
    double rip = pow(10.0, -chebrip / 10.0);
    double eps = sqrt(rip - 1.0);
    double y = asinh(1.0 / eps) / (double) order;
    if (y <= 0.0)
        throw QString("MKFILTER: bug: Chebyshev y=")+QString::number(y)+QString("; must be < 0.0");
    for (int i = 0; i < splane.numpoles; i++)
      { splane.poles[i].re *= sinh(y);
        splane.poles[i].im *= cosh(y);
      }
      }
  }

void prewarp()
  { // for bilinear transform, perform pre-warp on alpha values
    if (options & (opt_w | opt_z))
      { warped_alpha1 = raw_alpha1;
    warped_alpha2 = raw_alpha2;
      }
    else
      { warped_alpha1 = tan(PI * raw_alpha1) / PI;
    warped_alpha2 = tan(PI * raw_alpha2) / PI;
      }
  }

// called for trad, not for -Re or -Pi
void normalize() {

    double w1 = TWOPI * warped_alpha1;
    double w2 = TWOPI * warped_alpha2;
    // transform prototype into appropriate filter type (lp/hp/bp/bs)
    switch (options & (opt_lp | opt_hp | opt_bp| opt_bs)) {
    case opt_lp:
      { for (int i = 0; i < splane.numpoles; i++) splane.poles[i] = splane.poles[i] * w1;
        splane.numzeros = 0;
        break;
      }

    case opt_hp:
      { int i;
        for (i=0; i < splane.numpoles; i++) splane.poles[i] = w1 / splane.poles[i];
        for (i=0; i < splane.numpoles; i++) splane.zeros[i] = 0.0;	 // also N zeros at (0,0)
        splane.numzeros = splane.numpoles;
        break;
      }

    case opt_bp:
      { double w0 = sqrt(w1*w2), bw = w2-w1; int i;
        for (i=0; i < splane.numpoles; i++)
          { complex hba = 0.5 * (splane.poles[i] * bw);
        complex temp = csqrt(1.0 - sqr(w0 / hba));
        splane.poles[i] = hba * (1.0 + temp);
        splane.poles[splane.numpoles+i] = hba * (1.0 - temp);
          }
        for (i=0; i < splane.numpoles; i++) splane.zeros[i] = 0.0;	 // also N zeros at (0,0)
        splane.numzeros = splane.numpoles;
        splane.numpoles *= 2;
        break;
      }

    case opt_bs:
      { double w0 = sqrt(w1*w2), bw = w2-w1; int i;
        for (i=0; i < splane.numpoles; i++)
          { complex hba = 0.5 * (bw / splane.poles[i]);
        complex temp = csqrt(1.0 - sqr(w0 / hba));
        splane.poles[i] = hba * (1.0 + temp);
        splane.poles[splane.numpoles+i] = hba * (1.0 - temp);
          }
        for (i=0; i < splane.numpoles; i++)	   // also 2N zeros at (0, +-w0)
          { splane.zeros[i] = complex(0.0, +w0);
        splane.zeros[splane.numpoles+i] = complex(0.0, -w0);
          }
        splane.numpoles *= 2;
        splane.numzeros = splane.numpoles;
        break;
      }
    }
}

// compute Z-plane pole & zero positions for bandpass resonator
void compute_bpres(pzrep& zplane) {
    zplane.numpoles = zplane.numzeros = 2;
    zplane.zeros[0] = 1.0;
    zplane.zeros[1] = -1.0;
    double theta = TWOPI * raw_alpha1; // where we want the peak to be
    if (infq)
      { // oscillator
    complex zp = expj(theta);
    zplane.poles[0] = zp;
    zplane.poles[1] = cconj(zp);
      }
    else
      { // must iterate to find exact pole positions
    complex topcoeffs[MAXPZ+1];
    expand(zplane.zeros, zplane.numzeros, topcoeffs);
    double r = exp(-theta / (2.0 * qfactor));
    double thm = theta, th1 = 0.0, th2 = PI;
    bool cvg = false;
    for (int i=0; i < 50 && !cvg; i++)
      { complex zp = r * expj(thm);
        zplane.poles[0] = zp;
        zplane.poles[1] = cconj(zp);
        complex botcoeffs[MAXPZ+1];
        expand(zplane.poles, zplane.numpoles, botcoeffs);
        complex g = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta));
        double phi = g.im / g.re; // approx to atan2
        if (phi > 0.0) th2 = thm; else th1 = thm;
        if (fabs(phi) < EPS) cvg = true;
        thm = 0.5 * (th1+th2);
      }
    if(!(cvg))
        throw QString("MKFILTER: warning: failed to converge");
      }
  }

// compute Z-plane pole & zero positions for bandstop resonator (notch filter)
void compute_notch(pzrep& zplane) {
    compute_bpres(zplane);		// iterate to place poles
    double theta = TWOPI * raw_alpha1;
    complex zz = expj(theta);	// place zeros exactly
    zplane.zeros[0] = zz; zplane.zeros[1] = cconj(zz);
}

// compute Z-plane pole & zero positions for allpass resonator
void compute_apres(pzrep& zplane) {
    compute_bpres(zplane);		// iterate to place poles
    zplane.zeros[0] = reflect(zplane.poles[0]);
    zplane.zeros[1] = reflect(zplane.poles[1]);
}

// Scope-safe below ------------------------------------------------------------

// given S-plane poles & zeros, compute Z-plane poles & zeros, by matched z-transform
void compute_z_mzt(const pzrep& splane2, pzrep& zplane2) {
    zplane2.numpoles = splane2.numpoles;
    zplane2.numzeros = splane2.numzeros;
    for (int i=0; i < zplane2.numpoles; i++) zplane2.poles[i] = cexp(splane2.poles[i]);
    for (int i=0; i < zplane2.numzeros; i++) zplane2.zeros[i] = cexp(splane2.zeros[i]);
}

// given S-plane poles & zeros, compute Z-plane poles & zeros, by bilinear transform
void compute_z_blt(const pzrep& splane2, pzrep& zplane2){
    zplane2.numpoles = splane2.numpoles;
    zplane2.numzeros = splane2.numzeros;
    for (int i=0; i < zplane2.numpoles; i++)
        zplane2.poles[i] = blt(splane2.poles[i]);
    for (int i=0; i < zplane2.numzeros; i++)
        zplane2.zeros[i] = blt(splane2.zeros[i]);
    while (zplane2.numzeros < zplane2.numpoles)
        zplane2.zeros[zplane2.numzeros++] = -1.0;
}

void expandpoly3(const pzrep& zplane2, uint options2, double raw_alpha1, double raw_alpha2, std::vector<double>& num, std::vector<double>& den, double& G) // given Z-plane poles & zeros, compute top & bot polynomials in Z, and then recurrence relation
{
    complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1];

    expand(zplane2.zeros, zplane2.numzeros, topcoeffs);
    expand(zplane2.poles, zplane2.numpoles, botcoeffs);

    // Get gain
    // TODO Simplify below
    complex dc_gain = evaluate(topcoeffs, zplane2.numzeros, botcoeffs, zplane2.numpoles, 1.0);
    double theta = TWOPI * 0.5 * (raw_alpha1 + raw_alpha2); // "jwT" for centre freq.
    complex fc_gain = evaluate(topcoeffs, zplane2.numzeros, botcoeffs, zplane2.numpoles, expj(theta));
    complex hf_gain = evaluate(topcoeffs, zplane2.numzeros, botcoeffs, zplane2.numpoles, -1.0);

    complex gain = (options2 & opt_pi) ? hf_gain :
               (options2 & opt_lp) ? dc_gain :
               (options2 & opt_hp) ? hf_gain :
               (options2 & (opt_bp | opt_ap)) ? fc_gain :
               (options2 & opt_bs) ? csqrt(dc_gain * hf_gain) : complex(1.0);
    G  = hypot(gain);

    // Get numerator(with gain) and denominator
    std::vector<double> xcoeffs2(zplane2.numzeros+1);
    std::vector<double> ycoeffs2(zplane2.numpoles+1);

    for (int i = 0; i <= zplane2.numzeros; i++)
        xcoeffs2[i] = +(topcoeffs[i].re / botcoeffs[zplane2.numpoles].re);
    for (int i = 0; i <= zplane2.numpoles; i++)
        ycoeffs2[i] = -(botcoeffs[i].re / botcoeffs[zplane2.numpoles].re);

    num.resize(zplane2.numzeros+1);
    for (int i = 0; i <= zplane2.numzeros; i++)
        num[i] = xcoeffs2[i]/G;

    den.resize(zplane2.numpoles+1);
    for (int i = 0; i <= zplane2.numpoles; i++)
        den[i] = ycoeffs2[zplane2.numpoles-i];
}

}

// C++ interface to the above functions ----------------------------------------

void mkfilter::make_butterworth_filter(int _order, double _alpha, bool isLowPass, std::vector<double>& num, std::vector<double>& den, std::vector<double>* response, int dftlen) {

    order = _order;
    raw_alpha1 = _alpha;

    options = opt_bu|opt_a|opt_o|opt_l;

    if (isLowPass)  options |= opt_lp;
    else            options |= opt_hp;

    checkoptions(options, order, polemask, raw_alpha1, raw_alpha2);

    compute_s();
    prewarp();
    normalize();

//    // Plot s-roots
//    for (int i=0; i < splane.numpoles; i++)
//        cout << "SP" << i << ": " << splane.poles[i].re << "+j*" << splane.poles[i].im << endl;
//    for (int i=0; i < splane.numzeros; i++)
//        cout << "SZ" << i << ": " << splane.zeros[i].re << "+j*" << splane.zeros[i].im << endl;

    pzrep zplane;
    compute_z_blt(splane, zplane);

    double gain;
    expandpoly3(zplane, options, raw_alpha1, raw_alpha2, num, den, gain);

    // Compute the frequency response if asked
    if(response!=NULL) {

        complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1];
        expand(zplane.zeros, zplane.numzeros, topcoeffs);
        expand(zplane.poles, zplane.numpoles, botcoeffs);

        response->resize(dftlen/2+1);

        double theta;
        for(int k=0; k<=dftlen/2; k++) {
            theta = (TWOPI*k)/dftlen;
            (*response)[k] = (1.0/gain)*hypot(evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta)));
        }
    }

//    // Plot coefs
//    std::cout << "gain=" << gain << std::endl;
//    for (size_t i=0; i < num.size(); i++)
//        std::cout << "num[" << i << "]=" << num[i] << std::endl;
//    for (size_t i=0; i < den.size(); i++)
//        std::cout << "den[" << i << "]=" << den[i] << std::endl;
}

void printpoleszeros(mkfilter::pzrep zplane) {
    for (int i=0; i < zplane.numpoles; i++)
        std::cout << "ZP" << i << ": " << zplane.poles[i].re << "+j*" << zplane.poles[i].im << std::endl;
    for (int i=0; i < zplane.numzeros; i++)
        std::cout << "ZZ" << i << ": " << zplane.zeros[i].re << "+j*" << zplane.zeros[i].im << std::endl;
}

void mkfilter::make_butterworth_filter_biquad(int _order, double _alpha, bool isLowPass, std::vector< std::vector<double> >& num, std::vector<std::vector<double> >& den, std::vector<double>* response, int dftlen) {

    order = _order;
    raw_alpha1 = _alpha;

    options = opt_bu|opt_a|opt_o|opt_l;

    if (isLowPass)  options |= opt_lp;
    else            options |= opt_hp;

    checkoptions(options, order, polemask, raw_alpha1, raw_alpha2);

    compute_s();
    prewarp();
    normalize();

//   // Plot s-roots
//   for (int i=0; i < splane.numpoles; i++)
//       std::cout << "SP" << i << ": " << splane.poles[i].re << "+j*" << splane.poles[i].im << std::endl;
//   for (int i=0; i < splane.numzeros; i++)
//       std::cout << "SZ" << i << ": " << splane.zeros[i].re << "+j*" << splane.zeros[i].im << std::endl;

    pzrep zplane;
    compute_z_blt(splane, zplane);

    if(response)
        (*response) = std::vector<double>(dftlen/2+1, 1.0);

    // Plot z-roots
//    std::cout << "Original" << std::endl;
//    printpoleszeros(zplane);

    int ii=0;
    while(ii<zplane.numpoles) {
        std::vector<complex> biqp;
        std::vector<complex> biqz;

        pzrep biq;
        biq.numpoles = 0;
        biq.numzeros = 0;

        if(zplane.numpoles-ii==3) {
            throw QString("TODO triquad")+QString(__FILE__)+":"+QString(__LINE__);
        }
        else {
            // Take 1 pole and 1 zero
            biq.poles[biq.numpoles++] = zplane.poles[ii];
            biq.poles[biq.numpoles++] = cconj(zplane.poles[ii]);

            biq.zeros[biq.numzeros++] = zplane.zeros[ii];
            biq.zeros[biq.numzeros++] = cconj(zplane.zeros[ii]);
        }

//        std::cout << "Biquad: " << ii << std::endl;
//        printpoleszeros(biq);

        double gain;
        std::vector<double> subnum;
        std::vector<double> subden;
        expandpoly3(biq, options, raw_alpha1, raw_alpha2, subnum, subden, gain);
        num.push_back(subnum);
        den.push_back(subden);

        // Compute the frequency response if asked
        if(response) {
            complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1];
            expand(biq.zeros, biq.numzeros, topcoeffs);
            expand(biq.poles, biq.numpoles, botcoeffs);

            double theta;
            for(int k=0; k<=dftlen/2; k++) {
                theta = (TWOPI*k)/dftlen;
                (*response)[k] *= (1.0/gain)*hypot(evaluate(topcoeffs, biq.numzeros, botcoeffs, biq.numpoles, expj(theta)));
            }
        }


        ii += biq.numpoles;
    }
    
//    // Plot coefs
////     std::cout << "gain=" << gain << std::endl;
//    for (size_t i=0; i < num.size(); i++)
//        for (size_t j=0; j < num[i].size(); j++)
//            std::cout << "num[" << i << "," << j << "]=" << num[i][j] << std::endl;
//   for (size_t i=0; i < den.size(); i++)
//        for (size_t j=0; j < num[i].size(); j++)
//            std::cout << "den[" << i << "," << j << "]=" << den[i][j] << std::endl;
}


void mkfilter::make_chebyshev_filter(int _order, double _alpha, double _chebrip, bool isLowPass, std::vector<double>& num, std::vector<double>& den) {

    order = _order;
    raw_alpha1 = _alpha;

    options = opt_ch|opt_a|opt_o|opt_l;
    chebrip = _chebrip;

    if (isLowPass)  options |= opt_lp;
    else            options |= opt_hp;

    checkoptions(options, order, polemask, raw_alpha1, raw_alpha2);

    compute_s();
    prewarp();
    normalize();

    pzrep zplane;
    compute_z_blt(splane, zplane);

    double gain;
    expandpoly3(zplane, options, raw_alpha1, raw_alpha2, num, den, gain);
}
