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

#ifndef GVSPECTROGRAM_H
#define GVSPECTROGRAM_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMutex>
#include <QThread>
#include <QMenu>
class QTime;

#include "qaesigproc.h"
#include "qaegigrid.h"

#include "wmainwindow.h"
#include "ftsound.h"

class GVSpectrogramWDialogSettings;
class MainWindow;
class QSpinBox;
class STFTComputeThread;

class GVSpectrogram : public QGraphicsView
{
    Q_OBJECT

    QTime m_progresswidgets_lastup;

    FTFZero* m_editing_fzero;

    QGraphicsSimpleTextItem* m_giInfoTxtInCenter;

protected:
    void contextMenuEvent(QContextMenuEvent * event);

public:
    explicit GVSpectrogram(WMainWindow* parent);

    GVSpectrogramWDialogSettings* m_dlgSettings;

    QGraphicsScene* m_scene;

    QToolBar* m_toolBar;
    QMenu m_contextmenu;

    STFTComputeThread* m_stftcomputethread;

    std::vector<FFTTYPE> m_win;

    // Cursor
    QGraphicsLineItem* m_giMouseCursorLineTimeBack;
    QGraphicsLineItem* m_giMouseCursorLineFreqBack;
    QGraphicsLineItem* m_giMouseCursorLineTime;
    QGraphicsLineItem* m_giMouseCursorLineFreq;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtTimeBack;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtFreqBack;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtTime;
    QGraphicsSimpleTextItem* m_giMouseCursorTxtFreq;
    void setMouseCursorPosition(QPointF p, bool forwardsync);

    QGraphicsLineItem* m_giPlayCursor;

    QAEGIGrid* m_giGrid;

    QPointF m_selection_pressedp;
    bool m_topismax;
    bool m_bottomismin;
    QPointF m_pressed_mouseinviewport;
    QRectF m_pressed_viewrect;
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAWaveformScale, CAEditFZero};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    void selectionSet(QRectF selection, bool forwardsync=true);
    void selectionSetTextInForm();
    bool hasSelection(){return m_selection.width()>0.0 || m_selection.height()>0.0;}

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool forwardsync=true);
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_spectrogram(QPainter* painter, const QRectF& rect, const QRectF& viewrect, FTSound* snd);

    ~GVSpectrogram();

    QAction* m_aSpectrogramShowGrid;
    QAction* m_aSpectrogramShowHarmonics;
    QAction* m_aAutoUpdate;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;
    QAction* m_aShowProperties;
    QAction* m_aSavePicture;

signals:

public slots:
    void showScrollBars(bool show);
    void gridSetVisible(bool visible);
    void showHarmonics(bool show);
    void savePicture();

    void allSoundsChanged();
    void playCursorSet(double t, bool forwardsync);

    void amplitudeExtentSlidersChanged();
    void amplitudeExtentSlidersChangesEnded();
    void updateSceneRect(); // To call when fs has changed and limits in dB
    void updateTextsGeometry();
    void updateSTFTSettings();
    void updateSTFTPlot(bool force=false);
    void stftComputingStateChanged(int state);
    void showProgressWidgets();
    void autoUpdate(bool autoupdate);

    void selectionZoomOn();
    void selectionClear(bool forwardsync=true);
    void azoomin();
    void azoomout();
    void aunzoom();
};

#endif // GVSPECTROGRAM_H
