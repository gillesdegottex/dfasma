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

#ifndef QGVGENERICTIMEVALUE_H
#define QGVGENERICTIMEVALUE_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMenu>
//#include <QTime>

#include "qaesigproc.h"
#include "qaegigrid.h"

#include "wmainwindow.h"

//class GVAmplitudeSpectrumWDialogSettings;
class MainWindow;
class QSpinBox;
class WidgetGenericTimeValue;

class GVGenericTimeValue : public QGraphicsView
{
    Q_OBJECT

    WidgetGenericTimeValue* m_fgtv;

protected:
    void contextMenuEvent(QContextMenuEvent * event);

public:
    explicit GVGenericTimeValue(WidgetGenericTimeValue* parent);

//    GVAmplitudeSpectrumWDialogSettings* m_dlgSettings;

    QList<FTGenericTimeValue*> m_ftgenerictimevalues;

    WidgetGenericTimeValue* widget() const {return m_fgtv;}

    QGraphicsScene* m_scene;

    QToolBar* m_toolBar;
    QMenu m_contextmenu;

    QAEGIGrid* m_giGrid;

    // Cursor
    QGraphicsLineItem* m_giCursorHoriz;
    QGraphicsLineItem* m_giCursorVert;
    QGraphicsSimpleTextItem* m_giCursorPositionXTxt;
    QGraphicsSimpleTextItem* m_giCursorPositionYTxt;
    void setMouseCursorPosition(QPointF p, bool forwardsync);

    // Selection
    QPointF m_selection_pressedp;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_viewrect;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    QGraphicsSimpleTextItem* m_giSelectionTxt;
    void selectionSet(QRectF selection, bool forwardsync);
    void selectionSetTextInForm();
    bool hasSelection(){return m_selection.width()>0.0;}

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool sync=true);
    void viewUpdateTexts();

    ~GVGenericTimeValue();

    QAction* m_aShowGrid;
    QAction* m_aUpdateSceneRect;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;
    QAction* m_aShowProperties;

protected slots:
    void gridSetVisible(bool visible);

public slots:
    void showScrollBars(bool show);
    void updateSceneRect(); // To call when fs has changed and limits in dB
//    void updateAmplitudeExtent();
//    void settingsModified();

    void selectionZoomOn();
    void selectionClear(bool forwardsync=true);
    void azoomin();
    void azoomout();
    void aunzoom();
};

#endif // QGVGENERICTIMEVALUE_H
