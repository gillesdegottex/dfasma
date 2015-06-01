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

#ifndef QGVSPECTROGRAM_H
#define QGVSPECTROGRAM_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMutex>
#include <QThread>
#include <QMenu>
class QTime;

#include "wmainwindow.h"

#include "sigproc.h"
#include "ftsound.h"

class GVSpectrogramWDialogSettings;
class MainWindow;
class QSpinBox;
class STFTComputeThread;

class QGVSpectrogram : public QGraphicsView
{
    Q_OBJECT

    QPen m_gridFontPen;
    QFont m_gridFont;

    QTime m_progresswidgets_lastup;

    QGraphicsPathItem* m_giPlayCursor;

public:
    explicit QGVSpectrogram(WMainWindow* parent);

    GVSpectrogramWDialogSettings* m_dlgSettings;

    QGraphicsScene* m_scene;

    QToolBar* m_toolBar;
    QMenu m_contextmenu;

    STFTComputeThread* m_stftcomputethread;

    std::vector<FFTTYPE> m_win;

    QGraphicsLineItem* m_giMouseCursorLineTime;
    QGraphicsLineItem* m_giMouseCursorLineFreq;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtTime;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtFreq;
    void setMouseCursorPosition(QPointF p, bool forwardsync);

    QImage m_imgSTFT;
    STFTComputeThread::ImageParameters m_imgSTFTParams; // This is the target parameters for the image
                                                        // During STFT update, it doesn't correspond to m_imgSTFT
    QPointF m_selection_pressedp;
    bool m_topismax;
    bool m_bottomismin;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_viewrect;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAWaveformScale};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    void selectionSet(QRectF selection, bool forwardsync=true);
    void selectionSetTextInForm();

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool forwardsync=true);
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);

    ~QGVSpectrogram();

    QAction* m_aSpectrogramShowGrid;
    QAction* m_aSpectrogramShowHarmonics;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aShowProperties;

signals:

public slots:
    void showScrollBars(bool show);

    void allSoundsChanged();
    void playCursorSet(double t, bool forwardsync);

    void amplitudeExtentSlidersChanged();
    void updateSceneRect(); // To call when fs has changed and limits in dB
    void updateTextsGeometry();
    void updateSTFTSettings();
    void updateSTFTPlot(bool force=false);
    void clearSTFTPlot();
    void stftComputingStateChanged(int state);
    void showProgressWidgets();

    void selectionZoomOn();
    void selectionClear(bool forwardsync=true);
    void azoomin();
    void azoomout();
};

#endif // QGVSPECTROGRAM_H
