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

#include "wmainwindow.h"

#include "sigproc.h"
#include "ftsound.h"

class GVSpectrogramWDialogSettings;
class MainWindow;
class QSpinBox;
class FFTResizeThread;

class QGVSpectrogram : public QGraphicsView
{
    Q_OBJECT

    qreal m_minsy;
    qreal m_maxsy;

    QRectF removeHiddenMargin(const QRectF& sceneRect);

    QPen m_gridFontPen;
    QFont m_gridFont;

public:
    explicit QGVSpectrogram(WMainWindow* parent);

    GVSpectrogramWDialogSettings* m_dlgSettings;

    sigproc::FFTwrapper* m_fft;
    FFTResizeThread* m_fftresizethread;

    QGraphicsScene* m_scene;

    QToolBar* m_toolBar;
    QMenu m_contextmenu;

    int m_winlen;
    int m_dftlen; // The dftlen set through the settings
//    unsigned int m_nl;
//    unsigned int m_nr;
    std::vector<FFTTYPE> m_win;
    QImage m_imgSTFT;

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
    void selectionSetTextInForm();

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool sync=true);
    void viewSync();
    void viewUpdateTexts();
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);

    ~QGVSpectrogram();

    QAction* m_aShowGrid;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;
    QAction* m_aShowProperties;

signals:
    
public slots:
    void settingsSave();
    void soundsChanged();

    void updateAmplitudeExtent();
    void updateSceneRect(); // To call when fs has changed and limits in dB
    void updateDFTSettings();
    void settingsModified();
    void computeSTFT();
    void updateSTFTPlot();
    void fftResizing(int prevSize, int newSize);

    void selectionZoomOn();
    void selectionClear();
    void azoomin();
    void azoomout();
    void aunzoom();
};

#endif // QGVSPECTROGRAM_H
