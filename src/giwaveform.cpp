#include "giwaveform.h"

#include <QPainter>
#include <QScrollBar>

#include "wmainwindow.h"
#include "gvwaveform.h"

GIWaveform::GIWaveform(FTSound* snd)
    : m_snd(snd)
{
    // This cache mode is slower than using m_wavp{x|y}_min
    // And, exposedRect is not inconsistent with this cache mode enabled.
//     setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
//    setOpacity(0.5); // TODO ?
}

QRectF GIWaveform::boundingRect() const {
    return QRectF(m_snd->m_delay/m_snd->fs, -1.0, m_snd->getDuration(), 2.0);
}

void GIWaveform::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget) {
    Q_UNUSED(widget)
//    COUTD << "FTSound::GraphicItemWaveform::paint update_rect=" << option->exposedRect << endl;

//    void QGVWaveform::draw_waveform(QPainter* painter, const QRectF& rect, FTSound* snd){
    if(!m_snd->m_actionShow->isChecked() // TODO Need this ?
        || m_snd->wavtoplay->empty())
        return;

    painter->setClipRect(option->exposedRect);

    double fs = m_snd->fs;

    QRectF viewrect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
//    COUTD << "viewrect=" << viewrect << endl;
    QRectF rect = option->exposedRect;
//    COUTD << "rect to update=" << rect << endl;

    double samppixdensity = (viewrect.right()-viewrect.left())*fs/gMW->m_gvWaveform->viewport()->rect().width();
    // COUTD << samppixdensity << endl;

//    samppixdensity=3;
    if(samppixdensity<4) {
//        COUTD << "draw_waveform: Draw lines between each sample" << endl;

        WAVTYPE a = m_snd->m_ampscale;
        if(m_snd->m_actionInvPolarity->isChecked())
            a *=-1.0;

        QPen wavpen(m_snd->getColor());
        wavpen.setWidth(0);
        painter->setPen(wavpen);

        int delay = m_snd->m_delay;
        int nleft = int((rect.left())*fs)-delay;
        int nright = int((rect.right())*fs)+1-delay;
        nleft = std::max(nleft, -delay);
        nleft = std::max(nleft, 0);
        nleft = std::min(nleft, int(m_snd->wav.size()-1)-delay);
        nleft = std::min(nleft, int(m_snd->wav.size()-1));
        nright = std::max(nright, -delay);
        nright = std::max(nright, 0);
        nright = std::min(nright, int(m_snd->wav.size()-1)-delay);
        nright = std::min(nright, int(m_snd->wav.size()-1));

        // Draw a line between each sample value
        WAVTYPE dt = 1.0/fs;
        WAVTYPE prevx = (nleft+delay)*dt;
        a *= -1;
        WAVTYPE* data = m_snd->wavtoplay->data();
        WAVTYPE prevy = a*(*(data+nleft));

        WAVTYPE x, y;
        for(int n=nleft; n<=nright; ++n){
            x = (n+delay)*dt;
            y = a*(*(data+n));

            if(y>1.0)       y = 1.0;
            else if(y<-1.0) y = -1.0;

            painter->drawLine(QLineF(prevx, prevy, x, y));

            prevx = x;
            prevy = y;
        }

        // When resolution is big enough, draw tick marks at each sample
        double samppixdensity_dotsthr = 0.125;
        if(samppixdensity<samppixdensity_dotsthr){
            qreal markhalfheight = gMW->m_gvWaveform->m_ampzoom*(1.0/10)*((samppixdensity_dotsthr-samppixdensity)/samppixdensity_dotsthr);

            for(int n=nleft; n<=nright; n++){
                x = (n+delay)*dt;
                y = a*(*(data+n));
                painter->drawLine(QLineF(x, y-markhalfheight, x, y+markhalfheight));
            }
        }

        painter->drawLine(QLineF(double(m_snd->wav.size()-1)/fs, -1.0, double(m_snd->wav.size()-1)/fs, 1.0));
    }
    else {
//        COUTD << "draw_waveform: Plot only one line per pixel" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        double gain = m_snd->m_ampscale;
        if(m_snd->m_actionInvPolarity->isChecked())
            gain *= -1.0;

        QPen outlinePen(m_snd->getColor());
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        QRect fullpixrect = gMW->m_gvWaveform->mapFromScene(viewrect).boundingRect();
        QRect pixrect = gMW->m_gvWaveform->mapFromScene(rect).boundingRect();
        pixrect = pixrect.intersected(fullpixrect);

        double s2p = -(fullpixrect.height()-1)/viewrect.height(); // Scene to pixel
        double p2n = fs*double(viewrect.width())/double(fullpixrect.width()-1); // Pixel to scene
        double yzero = fullpixrect.height()/2;

        WAVTYPE* yp = m_snd->wavtoplay->data();

        int winpixdelay = gMW->m_gvWaveform->horizontalScrollBar()->value(); // - 1.0/p2n; // The magic value to make everything plot at the same place whatever the scroll

        int snddelay = m_snd->m_delay;

        FTSound::WavParameters reqparams(fullpixrect, viewrect, winpixdelay, m_snd);
        if(m_snd->wavtoplay==&(m_snd->wav) && reqparams==m_wavparams){
//             COUTD << "Using existing buffer " << pixrect << endl;
            for(int i=pixrect.left(); i<=pixrect.right(); i++){
//            for(int i=0; i<=snd->m_wavpx_min.size(); i++)
                if(m_wavpx_min[i]<1e10) // TODO Clean this dirty fix
                    painter->drawLine(QLineF(i, yzero+m_wavpx_min[i], i, yzero+m_wavpx_max[i]));
            }
        }
        else {
//            COUTD << "Re compute buffer and draw " << pixrect << endl;
            if(int(m_wavpx_min.size())!=reqparams.fullpixrect.width()){
//                COUTD << "Resize" << endl;
                m_wavpx_min.resize(reqparams.fullpixrect.width());
                m_wavpx_max.resize(reqparams.fullpixrect.width());
                pixrect = reqparams.fullpixrect;
            }
            int ns = int((pixrect.left()+winpixdelay)*p2n)-snddelay;
            for(int i=pixrect.left(); i<=pixrect.right(); i++) {

                int ne = int((i+1+winpixdelay)*p2n)-snddelay;

                if(ns>=0 && ne<int(m_snd->wav.size())) {
                    WAVTYPE ymin = 1.0;
                    WAVTYPE ymax = -1.0;
                    WAVTYPE* ypp = yp+ns;
                    WAVTYPE y;
                    for(int n=ns; n<=ne; n++) {
                        y = *ypp;
                        ymin = std::min(ymin, y);
                        ymax = std::max(ymax, y);
                        ypp++;
                    }
                    ymin = gain*ymin;
                    if(ymin>1.0)       ymin = 1.0;
                    else if(ymin<-1.0) ymin = -1.0;
                    ymax = gain*ymax;
                    if(ymax>1.0)       ymax = 1.0;
                    else if(ymax<-1.0) ymax = -1.0;
                    ymin *= s2p;
                    ymax *= s2p;
                    ymin = int(ymin-2);
                    ymax = int(ymax-2);
                    m_wavpx_min[i] = ymin;
                    m_wavpx_max[i] = ymax;
                    painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
                }
                else {
                    m_wavpx_min[i] = 1e20; // TODO Clean this dirty fix
                    m_wavpx_max[i] = 1e20; // TODO Clean this dirty fix
                }

                ns = ne;
            }
        }
        m_wavparams = reqparams;

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }
}

// Might be usefull for #419
//void QGVWaveform::draw_allwaveforms(QPainter* painter, const QRectF& rect){
////    COUTD << "QGVWaveform::draw_allwaveforms " << rect << endl;
////    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVWaveform::draw_waveform [" << rect.width() << "]" << endl;

//    // First shift the visible waveforms according to the previous scrolling
//    // (This cannot be done in scrollContentsBy because this &^$%#@ function is called when resizing the
//    //  spectrogram, because they are synchronized horizontally (Using: connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int))); onnect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));)).
////    COUTD << m_scrolledx << endl;
//    if(m_currentAction!=CAZooming && gMW->m_gvSpectrogram->m_currentAction!=QGVSpectrogram::CAZooming){
//        for(size_t fi=0; fi<gFL->ftsnds.size(); fi++){
//            if(!gFL->ftsnds[fi]->m_actionShow->isChecked())
//                continue;

//            if(gFL->ftsnds[fi]->m_wavpx_min.size()>0){
//                int ddx = m_scrolledx;
//                if(ddx>0){
//                    while(ddx>0){
//                        gFL->ftsnds[fi]->m_wavpx_min.pop_back();
//                        gFL->ftsnds[fi]->m_wavpx_max.pop_back();
//                        gFL->ftsnds[fi]->m_wavpx_min.push_front(0.0);
//                        gFL->ftsnds[fi]->m_wavpx_max.push_front(0.0);
//                        ddx--;
//                    }
//                }
//                else if(ddx<0){
//                    while(ddx<0){
//                        gFL->ftsnds[fi]->m_wavpx_min.pop_front();
//                        gFL->ftsnds[fi]->m_wavpx_max.pop_front();
//                        gFL->ftsnds[fi]->m_wavpx_min.push_back(0.0);
//                        gFL->ftsnds[fi]->m_wavpx_max.push_back(0.0);
//                        ddx++;
//                    }
//                }
//            }
//        }
//    }
//    m_scrolledx = 0;
//}
