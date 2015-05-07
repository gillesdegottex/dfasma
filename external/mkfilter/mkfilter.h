/* mkfilter -- given n, compute recurrence relation
   to implement Butterworth, Bessel or Chebyshev filter of order n
   A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
   September 1992 */

// Note that the code below should NOT be turned into float or any smaller precision

#include <math.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <iostream>
#include <QString>

namespace mkfilter {

    static QString version = "4.6";

    // IIR Butterworth-based filters
    void make_butterworth_filter(int _order, double _alpha, bool isLowPass, std::vector<double>& num, std::vector<double>& den, std::vector<double>* response=NULL, int dftlen=0);
    void make_butterworth_filter_biquad(int _order, double _alpha, bool isLowPass, std::vector< std::vector<double> >& num, std::vector<std::vector<double> >& den, std::vector<double>* response=NULL, int dftlen=0);

    // IIR Chebyshev filter
    void make_chebyshev_filter(int _order, double _alpha, double chebrip, bool isLowPass, std::vector<double>& num, std::vector<double>& den);

}
