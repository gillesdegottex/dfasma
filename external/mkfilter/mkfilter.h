/* mkfilter -- given n, compute recurrence relation
   to implement Butterworth, Bessel or Chebyshev filter of order n
   A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
   September 1992 */

#include <vector>
#include <deque>
#include <QString>

namespace mkfilter {

static QString version = "4.6";

void make_butterworth_filter(int _order, double _alpha, bool isLowPass, std::vector<double>& num, std::vector<double>& den, std::vector<double>* response=NULL, int dftlen=0);

void make_chebyshev_filter(int _order, double _alpha, double chebrip, bool isLowPass, std::vector<double>& num, std::vector<double>& den);

// filtfilt using Direct Form II
template<typename DataType, typename ContainerIn, typename ContainerOut> inline void filtfilt(const ContainerIn& wav, const std::vector<double>& num, const std::vector<double>& den, ContainerOut& filteredwav)
{
    filteredwav.resize(wav.size());

    size_t order = den.size()-1; // Filter order

    std::deque<DataType> delays(order+1, 0.0);

    // Forward pass
    for(size_t n=0; n<wav.size(); ++n) {

        delays.pop_back();
        delays.push_front(wav[n]);

        for(size_t k=1; k<=order; ++k)
            delays[0] += den[k] * delays[k];

        double f = 0.0;
        for(size_t k=0; k<=order; ++k)
            f += num[k] * delays[k];

        filteredwav[n] = f;
    }

    delays = std::deque<DataType>(order+1, 0.0);

    // Backward pass
    for(int n=filteredwav.size()-1; n>=0; --n) {

        delays.pop_back();
        delays.push_front(filteredwav[n]);

        for(size_t k=1; k<=order; ++k)
            delays[0] += den[k] * delays[k];

        double f = 0.0;
        for(size_t k=0; k<=order; k++)
            f += num[k] * delays[k];

        filteredwav[n] = f;

        if(f!=f) throw QString("filter is numerically unstable");
    }
}



}

