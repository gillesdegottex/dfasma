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

#include <deque>

#include <QGraphicsView>
#include <QMenu>

#include "ftsound.h"

class WMainWindow;
class QToolBar;
class FTLabels;

class QGVWaveform : public QGraphicsView
{
    Q_OBJECT

    qreal m_tmpdelay;
    FTSound::DFTParameters m_windowParams;

    int m_ftlabel_current_index;

    QGraphicsLineItem* m_giMouseCursorLine;
    QGraphicsSimpleTextItem* m_giMouseCursorTxt;

    int m_scrolledx;

public:

    QToolBar* m_toolBar;

    bool m_first_start;

    float m_selection_pressedx;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_scenerect;
    int m_ca_pressed_index;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAStretchSelection, CAWaveformScale, CAWaveformDelay, CALabelWritting, CALabelModifPosition, CALabelAllModifPosition};
    int m_currentAction;
    QRectF m_mouseSelection; // The mouse selection. This one ignores the samples
    QRectF m_selection; // The actual selection, always at exact samples time
    QGraphicsRectItem* m_giSelection; // The shown selection, which contains the actual selection (start and end - and + 0.5 sample before and after the actual selection)
    // QGraphicsRectItem* m_giMouseSelection; // For debug purpose
//    QGraphicsItemGroup* m_yTicksLabels; // TODO Use this instead of print them individually ?
    qreal m_initialPlayPosition;
    QGraphicsPathItem* m_giPlayCursor;
    QGraphicsRectItem* m_giFilteredSelection;

    QGraphicsPathItem* m_giWindow;

    qreal m_ampzoom;

    QGraphicsScene* m_scene;

    QAction* m_aWaveformShowGrid;
    QAction* m_aWaveformShowWindow;
    QAction* m_aWaveformShowSTFTWindowCenters;
    QAction* m_aWaveformStickToSTFTWindows;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;
    QAction* m_aFitViewToSoundsAmplitude;
    QAction* m_aWaveformShowSelectedWaveformOnTop;
    QMenu m_contextmenu;

    explicit QGVWaveform(WMainWindow* parent);

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void drawBackground(QPainter* painter, const QRectF& rect);

//    void cursorUpdate(float x);
    QPen m_gridPen;
    QPen m_gridFontPen;
    void draw_grid(QPainter* painter, const QRectF& rect);
    void draw_allwaveforms(QPainter* painter, const QRectF& rect);
    void draw_waveform(QPainter* painter, const QRectF& rect, FTSound *snd);

    void selectSegmentFindStartEnd(double x, FTLabels* ftl, double& start, double& end);
    void selectSegment(double x, bool add);
    void selectRemoveSegment(double x);
    bool hasSelection(){return m_selection.width()>0.0;}
    double getPlayCursorPosition();

    void viewSet(QRectF viewrect, bool sync=true);

    void fixTimeLimitsToSamples(QRectF& selection, const QRectF& mouseSelection, int action);

signals:

public slots:
    void showScrollBars(bool show);

    void updateSceneRect();
    void updateTextsGeometry();
    void updateSelectionText();
    void sldAmplitudeChanged(int value);
    void fitViewToSoundsAmplitude();
    void azoomin();
    void azoomout();
    void aunzoom();
    void selectionSet(QRectF selection, bool forwardsync=true);
    void selectionClear(bool forwardsync=true);
    void selectionZoomOn();
    void setMouseCursorPosition(double position, bool forwardsync);
    void playCursorSet(double t, bool forwardsync=true);

};

#endif // QGVWAVEFORM_H
