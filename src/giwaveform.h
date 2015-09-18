#ifndef GIWAVEFORM_H
#define GIWAVEFORM_H

#include <QGraphicsItem>

#include "ftsound.h"

class GIWaveform : public QGraphicsItem
{
    FTSound* m_snd;

    std::deque<WAVTYPE> m_wavpx_min;    // Cache for min values
    std::deque<WAVTYPE> m_wavpx_max;    // Cache for max values

public:
    FTSound::WavParameters m_wavparams;

    GIWaveform(FTSound* snd);
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
};

#endif // GIWAVEFORM_H
