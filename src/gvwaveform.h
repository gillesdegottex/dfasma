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

#include "qaegigrid.h"

class QToolBar;
class WMainWindow;
class FTLabels;

class GVWaveform : public QGraphicsView
{
    Q_OBJECT

    qreal m_tmpdelay;

    int m_ftlabel_current_index;

    QGraphicsSimpleTextItem* m_giMouseCursorTxt;

//    int m_scrolledx; // For #419 ?

protected:
    void contextMenuEvent(QContextMenuEvent * event);

public:
    QGraphicsLineItem* m_giMouseCursorLine;

    QToolBar* m_toolBar;

    bool m_first_start;

    QPointF m_selection_pressedp;
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
    QGraphicsLineItem* m_giPlayCursor;
    QGraphicsRectItem* m_giFilteredSelection;

    // Graphic items
    QGraphicsScene* m_scene;
    QAEGIGrid* m_giGrid;
    QGraphicsPathItem* m_giWindow;
    qreal m_ampzoom;

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

    explicit GVWaveform(WMainWindow* parent);

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

    void selectSegmentFindStartEnd(double x, FTLabels* ftl, double& start, double& end);
    void selectSegment(double x, bool add);
    void selectRemoveSegment(double x);
    bool hasSelection(){return m_selection.width()>0.0;}
    double getPlayCursorPosition() const;

    void viewSet(QRectF viewrect, bool sync=true);

    void fixTimeLimitsToSamples(QRectF& selection, const QRectF& mouseSelection, int action);

public slots:
    void showScrollBars(bool show);
    void gridSetVisible(bool visible){m_giGrid->setVisible(visible);}

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
