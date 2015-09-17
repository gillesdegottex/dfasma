#ifndef GISPECTRUMAMPLITUDE_H
#define GISPECTRUMAMPLITUDE_H

#include <vector>
#include <complex>

#include <QGraphicsItem>
#include <QPen>

#ifdef SIGPROC_FLOAT
#define WAVTYPE float
#else
#define WAVTYPE double
#endif

class FTSound;
class QGraphicsView;

class GISpectrumAmplitude : public QGraphicsItem
{
    std::vector<std::complex<WAVTYPE> >* m_ldft;
    double m_fs;
    QPen m_pen;
    QGraphicsView* m_view;

public:
    GISpectrumAmplitude(std::vector<std::complex<WAVTYPE> >* ldft, double fs, QGraphicsView* view);

    void setPen(const QPen& pen)    {m_pen=pen;}
    void setSamplingRate(double fs) {m_fs=fs;}

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
};

#endif // GISPECTRUMAMPLITUDE_H
