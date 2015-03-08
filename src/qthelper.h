#ifndef QTHELPER_H
#define QTHELPER_H

#include <iostream>

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

template<typename streamtype>
streamtype& operator<<(streamtype& stream, const QRectF& rectf) {
    stream << "[" << rectf.left() << "," << rectf.right() << "]x[" << rectf.top() << "," << rectf.bottom() << "]";

    return stream;
}

template<typename streamtype>
streamtype& operator<<(streamtype& stream, const QSize& size) {
    stream << size.width() << "x" << size.height();

    return stream;
}

template<typename streamtype>
streamtype& operator<<(streamtype& stream, const QPoint& p) {
    stream << "(" << p.x() << "," << p.y() << ")";

    return stream;
}

template<typename streamtype>
streamtype& operator<<(streamtype& stream, const QColor& c) {
    stream << "(" << c.red() << "," << c.green() << "," << c.blue() << ")";

    return stream;
}

#endif // QTHELPER_H
