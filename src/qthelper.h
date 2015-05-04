#ifndef QTHELPER_H
#define QTHELPER_H

#include <QRectF>

#include <iostream>

#include <QThread>
#include <QDateTime>
#include <QColor>
#include <QGraphicsView>

#include "ftsound.h"

#ifdef __MINGW32__
    #define COMPILER "MinGW"
#elif (defined(__GNUC__) || defined(__GNUG__))
    #define COMPILER "GCC"
#elif defined(_MSC_VER)
    #define COMPILER "MSVC"
#endif


// Check if compiling using MSVC (Microsoft compiler)
#ifndef MSVC_VERSION
    // The following is necessary for MSVC 2012
    inline bool qIsInf(float f){return std::abs(f)>std::numeric_limits<float>::max()/2;}
    template<class Type> inline Type log2(Type v) {return std::log(v)/std::log(2.); }
#endif

#define QUOTE(name) #name
#ifdef _MSC_VER
    #define STR(val) QUOTE(val,"")
#else
    #define STR(val) QUOTE(val)
#endif


#define COUTD std::cout << QThread::currentThreadId() << " " << QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("hh:mm:ss.zzz             ").toLocal8Bit().constData() << " " << __FILE__ << ":" << __LINE__ << " "

#define FLAG COUTD << std::endl;

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
inline QRectF removeHiddenMargin(QGraphicsView* gv, const QRectF& sceneRect){
    const int bugMargin = 2;
    const double mx = sceneRect.width()/gv->viewport()->size().width()*bugMargin;
    const double my = sceneRect.height()/gv->viewport()->size().height()*bugMargin;
    return sceneRect.adjusted(mx, my, -mx, -my);
}

inline std::ostream& operator<<(std::ostream& stream, const QRectF& rectf) {
    stream << "[" << rectf.left() << "," << rectf.right() << "]x[" << rectf.top() << "," << rectf.bottom() << "]";

    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const QSize& size) {
    stream << size.width() << "x" << size.height();

    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const QPoint& p) {
    stream << "(" << p.x() << "," << p.y() << ")";

    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const QColor& c) {
    stream << "(" << c.red() << "," << c.green() << "," << c.blue() << ")";

    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const QTime& t) {
    stream << t.toString("hh:mm:ss.zzz").toLocal8Bit().constData();

    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const FTSound::DFTParameters& params) {
    stream << "samples[" << params.nl << "," << params.nr << "] winlen=" << params.winlen << " wintype=" << params.wintype << " win.size()=" << params.win.size() << " dftlen=" << params.dftlen << " ampscale=" << params.ampscale << " delay=" << params.delay;

    return stream;
}

//            COUTD << snd->m_dftparams.nl << " " << snd->m_dftparams.nr << " " << snd->m_dftparams.winlen << " " << snd->m_dftparams.dftlen << " " << snd->m_dftparams.ampscale << " " << snd->m_dftparams.delay << endl;

#endif // QTHELPER_H
