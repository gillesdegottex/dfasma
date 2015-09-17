#ifndef GIWAVEFORM_H
#define GIWAVEFORM_H

#include <QGraphicsItem>

#include "ftsound.h"

class GIWaveform : public QGraphicsItem
{
    FTSound* m_snd;
public:
    FTSound::WavParameters m_wavparams;
    std::deque<WAVTYPE> m_wavpx_min;
    std::deque<WAVTYPE> m_wavpx_max;

    GIWaveform(FTSound* snd);
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
};

#endif // GIWAVEFORM_H
