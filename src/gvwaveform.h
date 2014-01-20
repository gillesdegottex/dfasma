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

#ifndef QGVWAVEFORM_H
#define QGVWAVEFORM_H

#include <QGraphicsView>

class WMainWindow;

class QGVWaveform : public QGraphicsView
{
    Q_OBJECT

public:

    bool first_start;

    WMainWindow* main;

    float selection_pressedx;
    enum CurrentAction {CANothing, CAMoving, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight};
    int currentAction;
    QRectF selection, mouseSelection;
    QGraphicsRectItem* giSelection;
    QGraphicsLineItem* giCursor;
    QGraphicsSimpleTextItem* giCursorPositionTxt;
//    QGraphicsItemGroup* m_yTicksLabels; // TODO Use this instead of print them individually ?

    QGraphicsLineItem* giPlayCursor;

    qreal h11;
    qreal h12;
    qreal h21;
    qreal h22;

    float amplitudezoom;

    QGraphicsScene* m_scene;

    QAction* m_aZoomOnSelection;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;

    explicit QGVWaveform(WMainWindow* _main);

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void drawBackground(QPainter* painter, const QRectF& rect);

    void update_cursor(float x);
    QPen m_gridPen;
    QPen m_gridFontPen;
    QFont m_gridFont;
    void draw_grid(QPainter* painter, const QRectF& rect);
    void draw_waveform(QPainter* painter, const QRectF& rect);

signals:

public slots:
    void soundsChanged();
    void azoomonselection();
    void azoomin();
    void azoomout();
    void aunzoom();
    void sldAmplitudeChanged(int value);
    void clipandsetselection();

    void setPlayCursor(double t);

};

#endif // QGVWAVEFORM_H
