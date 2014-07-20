/* mkfilter -- given n, compute recurrence relation
   to implement Butterworth, Bessel or Chebyshev filter of order n
   A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
   September 1992 */

#include <vector>
#include <deque>
#include <QString>

namespace mkfilter {

static QString version = "4.6";

void make_butterworth_filter(int _order, double _alpha, bool isLowPass, std::vector<double>& num, std::vector<double>& den);

void make_chebyshev_filter(int _order, double _alpha, double chebrip, bool isLowPass, std::vector<double>& num, std::vector<double>& den);

void filtfilt(const std::deque<double>& wav, const std::vector<double>& num, const std::vector<double>& den, std::deque<double>& filteredwav);

}

