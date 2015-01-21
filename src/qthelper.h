#ifndef QTHELPER_H
#define QTHELPER_H

#include <iostream>

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
QRectF removeHiddenMargin(QGraphicsView* gv, const QRectF& sceneRect){
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

#endif // QTHELPER_H
