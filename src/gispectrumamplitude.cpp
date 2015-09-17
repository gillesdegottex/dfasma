#include "gispectrumamplitude.h"

#include <QPainter>

#include "ftsound.h"
#include "wmainwindow.h"
#include "gvspectrumamplitude.h"

GISpectrumAmplitude::GISpectrumAmplitude(std::vector<std::complex<WAVTYPE> > *ldft, double fs, QGraphicsView *view)
    : m_ldft(ldft)
    , m_fs(fs)
    , m_view(view)
{
    // This cache mode is slower than using m_wavp{x|y}_min
    // And, exposedRect is not inconsistent with this cache mode enabled.
//     setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    //    setOpacity(0.5); // TODO ?
}

QRectF GISpectrumAmplitude::boundingRect() const
{
//    double ymin = qae::log2db*m_snd->m_dft_min;
//    double ymax = qae::log2db*m_snd->m_dft_max;
    double ymin = 100;    // TODO
    double ymax = -10000; // TODO

    double height = (ymax-ymin);
    if(height==0)
        height = 100; // Default for height, otherwise flickering issues

    return QRectF(0.0, -ymax, m_fs/2.0, height);
}

void GISpectrumAmplitude::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    int dftlen = (int(m_ldft->size())-1)*2;

    if (dftlen<2)
            return;
//        || m_snd->wavtoplay->empty())

    painter->setClipRect(option->exposedRect);

    QRectF viewrect = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
//    COUTD << "viewrect=" << viewrect << endl;
    QRectF rect = option->exposedRect;
//    COUTD << "rect to update=" << rect << endl;

    int kmin = std::max(0, int(dftlen*rect.left()/m_fs));
    int kmax = std::min(dftlen/2, int(1+dftlen*rect.right()/m_fs));

    // Draw the sound's spectra
    double samppixdensity = (dftlen*(viewrect.right()-viewrect.left())/m_fs)/m_view->viewport()->rect().width();

    painter->setPen(m_pen);

//    samppixdensity = 0.1;
    if(samppixdensity<=1.0) {
//        COUTD << "Spec: Draw lines between each bin" << endl;

        double prevx = m_fs*kmin/dftlen;
        std::complex<WAVTYPE>* yp = m_ldft->data();
        double prevy = qae::log2db*(yp[kmin].real());
        for(int k=kmin+1; k<=kmax; ++k){
            double x = m_fs*k/dftlen;
            double y = qae::log2db*(yp[k].real());
            if(y<-10*viewrect.bottom()) y=-10*viewrect.bottom();
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }
    else {
        // TODO Add buffer like for the waveform
//        COUTD << "Spec: Plot only one line per pixel, in order to reduce computation time" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        QRect pixrect = m_view->mapFromScene(rect).boundingRect();
        QRect fullpixrect = m_view->mapFromScene(viewrect).boundingRect();

        double s2p = -(fullpixrect.height()-1)/viewrect.height(); // Scene to pixel
        double p2s = viewrect.width()/fullpixrect.width(); // Pixel to scene
        double yzero = m_view->mapFromScene(QPointF(0,0)).y();
        double yinfmin = -viewrect.bottom()/qae::log2db; // Nothing seen below this

        std::complex<WAVTYPE>* yp = m_ldft->data();

        int ns = int(dftlen*(viewrect.left()+pixrect.left()*p2s)/m_fs);
        for(int i=pixrect.left(); i<=pixrect.right(); i++) {
            int ne = int(dftlen*(viewrect.left()+(i+1)*p2s)/m_fs);

            if(ns>=0 && ne<int(m_ldft->size())) {
                WAVTYPE ymin = std::numeric_limits<WAVTYPE>::infinity();
                WAVTYPE ymax = -std::numeric_limits<WAVTYPE>::infinity();
                std::complex<WAVTYPE>* ypp = yp+ns;
                WAVTYPE y;
                for(int n=ns; n<=ne; n++) {
                    y = ypp->real();
                    if(y<yinfmin) y = yinfmin-10;
                    ymin = std::min(ymin, y);
                    ymax = std::max(ymax, y);
                    ypp++;
                }
                ymin = qae::log2db*(ymin);
                ymax = qae::log2db*(ymax);
                ymin *= s2p;
                ymax *= s2p;
                ymin = int(ymin-1);
                ymax = int(ymax);
                if(ymin>fullpixrect.height()+1-yzero) ymin=fullpixrect.height()+1-yzero;
                if(ymax>fullpixrect.height()+1-yzero) ymax=fullpixrect.height()+1-yzero;
                painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
            }

            ns = ne;
        }

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }
}

