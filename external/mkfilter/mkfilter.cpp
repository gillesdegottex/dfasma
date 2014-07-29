/* mkfilter -- given n, compute recurrence relation
   to implement Butterworth, Bessel or Chebyshev filter of order n
   A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
   September 1992 */

#include "mkfilter.h"

//using namespace mkfilter;

#include <math.h>

typedef unsigned int uint;
#undef	PI
//#define PI	    3.14159265358979323846  /* Microsoft C++ does not define M_PI ! */
#define PI	    M_PI
#define TWOPI	(2.0 * M_PI)
#define EPS	    1e-10
#define MAXORDER    128
#define MAXPZ	    512	    /* .ge. 2*MAXORDER, to allow for doubling of poles in BP filter;
                   high values needed for FIR filters */

#include <iostream>
using namespace std;
#include <QString>



inline double sqr(double x)	    { return x*x;			       }
inline bool seq(char *s1, char *s2) { return strcmp(s1,s2) == 0;	       }
inline bool onebit(uint m)	    { return (m != 0) && ((m & (m-1)) == 0);     }

inline double asinh(double x)
  { /* Microsoft C++ does not define */
    return log(x + sqrt(1.0 + sqr(x)));
  }

inline double fix(double x)
  { /* nearest integer */
    return (x >= 0.0) ? floor(0.5+x) : -floor(0.5-x);
  }


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
extern complex evaluate(complex[], int, complex[], int, complex);   /* from complex.C */

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


static complex eval(complex[], int, complex);
static double Xsqrt(double);


complex evaluate(complex topco[], int nz, complex botco[], int np, complex z)
  { /* evaluate response, substituting for z */
    return eval(topco, nz, z) / eval(botco, np, z);
  }

static complex eval(complex coeffs[], int npz, complex z)
  { /* evaluate polynomial in z, substituting for z */
    complex sum = complex(0.0);
    for (int i = npz; i >= 0; i--) sum = (sum * z) + coeffs[i];
    return sum;
  }

complex csqrt(complex x)
  { double r = hypot(x);
    complex z = complex(Xsqrt(0.5 * (r + x.re)),
            Xsqrt(0.5 * (r - x.re)));
    if (x.im < 0.0) z.im = -z.im;
    return z;
  }

static double Xsqrt(double x)
  { /* because of deficiencies in hypot on Sparc, it's possible for arg of Xsqrt to be small and -ve,
       which logically it can't be (since r >= |x.re|).	 Take it as 0. */
    return (x >= 0.0) ? sqrt(x) : 0.0;
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

struct pzrep
  { complex poles[MAXPZ], zeros[MAXPZ];
    int numpoles, numzeros;
  };

static pzrep splane, zplane;
static int order;
static double raw_alpha1, raw_alpha2, raw_alphaz;
static complex dc_gain, fc_gain, hf_gain;
static uint options;
static double warped_alpha1, warped_alpha2, chebrip, qfactor;
static bool infq;
static uint polemask;
static double xcoeffs[MAXPZ+1], ycoeffs[MAXPZ+1];


static void choosepole(complex);
static void compute_z_mzt();
static void compute_notch(), compute_apres();
static complex reflect(complex);
static void compute_bpres(), add_extra_zero();
static void expand(complex[], int, complex[]);
static void multin(complex, int, complex[]);
static void printcoeffs(const char*, int, double[]);


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


void checkoptions()
  { bool optsok = true;
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
  }

void setdefaults() {
    if(!(options & opt_p)) polemask = ~0; // use all poles
    if(!(options & (opt_bp | opt_bs))) raw_alpha2 = raw_alpha1;
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

static void choosepole(complex z)
  { if (z.re < 0.0)
      { if (polemask & 1) splane.poles[splane.numpoles++] = z;
    polemask >>= 1;
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

void normalize()		// called for trad, not for -Re or -Pi
  {
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

static complex blt(complex pz)
  { return (2.0 + pz) / (2.0 - pz);
  }

void compute_z_blt() // given S-plane poles & zeros, compute Z-plane poles & zeros, by bilinear transform
  { int i;
    zplane.numpoles = splane.numpoles;
    zplane.numzeros = splane.numzeros;
    for (i=0; i < zplane.numpoles; i++) zplane.poles[i] = blt(splane.poles[i]);
    for (i=0; i < zplane.numzeros; i++) zplane.zeros[i] = blt(splane.zeros[i]);
    while (zplane.numzeros < zplane.numpoles) zplane.zeros[zplane.numzeros++] = -1.0;
  }

static void compute_z_mzt() // given S-plane poles & zeros, compute Z-plane poles & zeros, by matched z-transform
  { int i;
    zplane.numpoles = splane.numpoles;
    zplane.numzeros = splane.numzeros;
    for (i=0; i < zplane.numpoles; i++) zplane.poles[i] = cexp(splane.poles[i]);
    for (i=0; i < zplane.numzeros; i++) zplane.zeros[i] = cexp(splane.zeros[i]);
  }

static void compute_notch()
  { // compute Z-plane pole & zero positions for bandstop resonator (notch filter)
    compute_bpres();		// iterate to place poles
    double theta = TWOPI * raw_alpha1;
    complex zz = expj(theta);	// place zeros exactly
    zplane.zeros[0] = zz; zplane.zeros[1] = cconj(zz);
  }

static void compute_apres()
  { // compute Z-plane pole & zero positions for allpass resonator
    compute_bpres();		// iterate to place poles
    zplane.zeros[0] = reflect(zplane.poles[0]);
    zplane.zeros[1] = reflect(zplane.poles[1]);
  }

static complex reflect(complex z)
  { double r = hypot(z);
    return z / sqr(r);
  }

static void compute_bpres()
  { // compute Z-plane pole & zero positions for bandpass resonator
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

static void add_extra_zero()
  { if (zplane.numzeros+2 > MAXPZ)
      throw QString("MKFILTER: too many zeros; can't do -Z");
    double theta = TWOPI * raw_alphaz;
    complex zz = expj(theta);
    zplane.zeros[zplane.numzeros++] = zz;
    zplane.zeros[zplane.numzeros++] = cconj(zz);
    while (zplane.numpoles < zplane.numzeros) zplane.poles[zplane.numpoles++] = 0.0;	 // ensure causality
  }

void expandpoly() // given Z-plane poles & zeros, compute top & bot polynomials in Z, and then recurrence relation
  { complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1]; int i;
    expand(zplane.zeros, zplane.numzeros, topcoeffs);
    expand(zplane.poles, zplane.numpoles, botcoeffs);
    dc_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, 1.0);
    double theta = TWOPI * 0.5 * (raw_alpha1 + raw_alpha2); // "jwT" for centre freq.
    fc_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta));
    hf_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, -1.0);
    for (i = 0; i <= zplane.numzeros; i++) xcoeffs[i] = +(topcoeffs[i].re / botcoeffs[zplane.numpoles].re);
    for (i = 0; i <= zplane.numpoles; i++) ycoeffs[i] = -(botcoeffs[i].re / botcoeffs[zplane.numpoles].re);
  }

// compute product of poles or zeros as a polynomial of z
static void expand(complex pz[], int npz, complex coeffs[]) {
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

// multiply factor (z-w) into coeffs
static void multin(complex w, int npz, complex coeffs[]) {
    complex nw = -w;
    for (int i = npz; i >= 1; i--)
        coeffs[i] = (nw * coeffs[i]) + coeffs[i-1];
    coeffs[0] = nw * coeffs[0];
}

void printfiltercoefs() {
    complex gain = (options & opt_pi) ? hf_gain :
               (options & opt_lp) ? dc_gain :
               (options & opt_hp) ? hf_gain :
               (options & (opt_bp | opt_ap)) ? fc_gain :
               (options & opt_bs) ? csqrt(dc_gain * hf_gain) : complex(1.0);
    cout << "gain=" << gain.re << "," << gain.im << endl;
    cout << "G  = " << hypot(gain) << endl;
    printcoeffs("NZ", zplane.numzeros, xcoeffs);
    printcoeffs("NP", zplane.numpoles, ycoeffs);
}

static void printcoeffs(const char *pz, int npz, double coeffs[]) {
    cout << pz << "=" << npz << endl;
    for (int i = 0; i <= npz; i++)
        cout << coeffs[i] << endl;
}

void copycoefs(std::vector<double>& num, std::vector<double>& den, double& G) {
    complex gain = (options & opt_pi) ? hf_gain :
               (options & opt_lp) ? dc_gain :
               (options & opt_hp) ? hf_gain :
               (options & (opt_bp | opt_ap)) ? fc_gain :
               (options & opt_bs) ? csqrt(dc_gain * hf_gain) : complex(1.0);
    G  = hypot(gain);
//    cout << "G=" << gain.re << "+j*" << gain.im << endl;

    num.resize(zplane.numzeros+1);
    for (int i = 0; i <= zplane.numzeros; i++)
        num[i] = xcoeffs[i]/G;

    den.resize(zplane.numpoles+1);
    for (int i = 0; i <= zplane.numpoles; i++)
        den[i] = ycoeffs[zplane.numpoles-i];
}


// C++ interface to the above functions ----------------------------------------

void mkfilter::make_butterworth_filter(int _order, double _alpha, bool isLowPass, std::vector<double>& num, std::vector<double>& den, std::vector<double>* response, int dftlen) {

    order = _order;
    raw_alpha1 = _alpha;

    options = opt_bu|opt_a|opt_o|opt_l;

    if (isLowPass)  options |= opt_lp;
    else            options |= opt_hp;

    checkoptions();
    setdefaults();
    compute_s();
    prewarp();
    normalize();

//    // Plot s-roots
//    for (int i=0; i < splane.numpoles; i++)
//        cout << "SP" << i << ": " << splane.poles[i].re << "+j*" << splane.poles[i].im << endl;
//    for (int i=0; i < splane.numzeros; i++)
//        cout << "SZ" << i << ": " << splane.zeros[i].re << "+j*" << splane.zeros[i].im << endl;

    compute_z_blt();

//    // Plot z-roots
//    for (int i=0; i < zplane.numpoles; i++)
//        cout << "ZP" << i << ": " << zplane.poles[i].re << "+j*" << zplane.poles[i].im << endl;
//    for (int i=0; i < zplane.numzeros; i++)
//        cout << "ZZ" << i << ": " << zplane.zeros[i].re << "+j*" << zplane.zeros[i].im << endl;

    // TODO Split here in biquads in order to manage orders higher than 19

    expandpoly();

    double gain;
    copycoefs(num, den, gain);

    // Compute the frequency response if asked
    if(response!=NULL){

        complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1];
        expand(zplane.zeros, zplane.numzeros, topcoeffs);
        expand(zplane.poles, zplane.numpoles, botcoeffs);

        response->resize(dftlen/2+1);

        double theta;
        for(unsigned int k=0; k<=dftlen/2; k++) {
            theta = (TWOPI*k)/dftlen;
            (*response)[k] = (1.0/gain)*hypot(evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta)));
        }
    }

//    // Plot coefs
//    cout << "gain=" << gain << endl;
//    for (int i=0; i < zplane.numpoles+1; i++)
//        cout << "xcoeffs[" << i << "]=" << xcoeffs[i] << endl;
//    for (int i=0; i < zplane.numzeros; i++)
//        cout << "ycoeffs[" << i << "]=" << ycoeffs[i] << endl;
}

void mkfilter::make_chebyshev_filter(int _order, double _alpha, double _chebrip, bool isLowPass, std::vector<double>& num, std::vector<double>& den) {

    order = _order;
    raw_alpha1 = _alpha;

    options = opt_ch|opt_a|opt_o|opt_l;
    chebrip = _chebrip;

    if (isLowPass)  options |= opt_lp;
    else            options |= opt_hp;

    checkoptions();
    setdefaults();
    compute_s();
    prewarp();
    normalize();
    compute_z_blt();
    expandpoly();
    double gain;
    copycoefs(num, den, gain);
}
