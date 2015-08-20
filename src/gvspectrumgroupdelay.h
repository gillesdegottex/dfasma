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

#ifndef QGVSPECTRUMGROUPDELAY_H
#define QGVSPECTRUMGROUPDELAY_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMenu>
#include <QToolBar>

#include "sigproc.h"
#include "ftsound.h"

//class GVPhaseSpectrumWDialogSettings;
class WMainWindow;
class QSpinBox;

class QGVSpectrumGroupDelay : public QGraphicsView
{
    Q_OBJECT

public:
    explicit QGVSpectrumGroupDelay(WMainWindow* parent);

//    GVPhaseSpectrumWDialogSettings* m_dlgSettings;

    QGraphicsScene* m_scene;

    QMenu m_contextmenu;

    QGraphicsLineItem* m_giCursorHoriz;
    QGraphicsLineItem* m_giCursorVert;
    QGraphicsSimpleTextItem* m_giCursorPositionXTxt;
    QGraphicsSimpleTextItem* m_giCursorPositionYTxt;
    void setMouseCursorPosition(QPointF p, bool forwardsync);

    QPointF m_selection_pressedp;
    QRectF m_wavselection_pressed;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_viewrect;
    qint64 m_pressed_delay;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAMovingWaveformSelection, CAWaveformDelay};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    QGraphicsSimpleTextItem* m_giSelectionTxt;
    void selectionSet(QRectF selection, bool forwardsync);
    void selectionClear(bool forwardsync=true);

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool forwardsync=true);
    void viewUpdateTexts();
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_spectrum(QPainter* painter, std::vector<WAVTYPE> &gd, double fs, double delay, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);

//    QAction* m_aShowProperties;
    QAction* m_aPhaseSpectrumGridUsePiFraction;

    ~QGVSpectrumGroupDelay();

signals:
    
public slots:
    void showScrollBars(bool show);

    void updateSceneRect(double maxdelay=-1);

    void azoomin();
    void azoomout();
};

#endif // QGVSPECTRUMGROUPDELAY_H
