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

#ifndef QGVPHASESPECTRUM_H
#define QGVPHASESPECTRUM_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMenu>

//#include "wmainwindow.h"

#include "external/FFTwrapper.h"

//class GVPhaseSpectrumWDialogSettings;
class WMainWindow;
class QSpinBox;

class QGVPhaseSpectrum : public QGraphicsView
{
    Q_OBJECT

    QRectF removeHiddenMargin(const QRectF& sceneRect);

public:
    explicit QGVPhaseSpectrum(WMainWindow* parent);

//    GVPhaseSpectrumWDialogSettings* m_dlgSettings;

    QGraphicsScene* m_scene;

    QMenu m_contextmenu;

    QGraphicsLineItem* m_giCursorHoriz;
    QGraphicsLineItem* m_giCursorVert;
    QGraphicsSimpleTextItem* m_giCursorPositionXTxt;
    QGraphicsSimpleTextItem* m_giCursorPositionYTxt;
    void cursorUpdate(QPointF p);
    void cursorFixAndRefresh();

    QPointF m_selection_pressedp;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_viewrect;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAWaveformScale};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    QGraphicsSimpleTextItem* m_giSelectionTxt;
    void selectionChangesRequested();
    void selectionFixAndRefresh();
    void selectionClear();

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewFixAndRefresh();
    void viewSet(QRectF viewrect);
    void viewChangesRequested();
    void viewUpdateTexts();
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);

    QAction* m_aShowGrid;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
//    QAction* m_aShowProperties;

    ~QGVPhaseSpectrum();

signals:
    
public slots:
    void settingsSave();
    void soundsChanged();
    void updateSceneRect();

    void azoomin();
    void azoomout();
};

#endif // QGVPHASESPECTRUM_H
