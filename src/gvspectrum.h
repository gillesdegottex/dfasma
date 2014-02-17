/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#ifndef QGVSPECTRUM_H
#define QGVSPECTRUM_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMutex>
#include <QThread>

#include "wmainwindow.h"
#include "../external/FFTwrapper.h"

class MainWindow;

class FFTResizeThread : public QThread
{
    Q_OBJECT

    FFTwrapper* m_fft;   // The FFT transformer

    int m_size_resizing;// The size which is in preparation by FFTResizeThread
    int m_size_todo;    // The next size which has to be done by FFTResizeThread asap

    void run(); //Q_DECL_OVERRIDE

signals:
    void fftResizing(int prevSize, int newSize);
    void fftResized(int prevSize, int newSize);

public:
    FFTResizeThread(FFTwrapper* fft, QObject* parent);

    void resize(int newsize);

    int size();

    QMutex m_mutex_resizing;      // To protect the access to the FFT transformer
    QMutex m_mutex_changingsizes; // To protect the access to the size variables above
};


class QGVSpectrum : public QGraphicsView
{
    Q_OBJECT

    bool m_first_start;

public:
    explicit QGVSpectrum(WMainWindow* main);

    FFTwrapper* m_fft;
    FFTResizeThread* m_fftresizethread;

    WMainWindow* m_main;
    QGraphicsScene* m_scene;

    int m_winlen;
    unsigned int m_nl;
    unsigned int m_nr;
    std::vector<double> m_win;


    QGraphicsLineItem* m_giCursorHoriz;
    QGraphicsLineItem* m_giCursorVert;
    QGraphicsSimpleTextItem* m_giCursorPositionXTxt;
    QGraphicsSimpleTextItem* m_giCursorPositionYTxt;

    QPointF m_selection_pressedp;
    enum CurrentAction {CANothing, CAMoving, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAWaveformScale};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giSelection;
    QGraphicsSimpleTextItem* m_giSelectionTxt;
    void selectionClipAndSet();
    void clipzoom(float& h11, float& h22);

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);


    void update_cursor(QPointF p);
    void update_texts_dimensions();
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);

    ~QGVSpectrum();

    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;

    float m_minsy;
    float m_maxsy;

signals:
    
public slots:
    void soundsChanged();

    void setWindowRange(double tstart, double tend);
    void computeDFTs();
    void fftResizing(int prevSize, int newSize);

    void selectionZoomOn();
    void selectionClear();
    void azoomin();
    void azoomout();
    void aunzoom();
};

#endif // QGVSPECTRUM_H
