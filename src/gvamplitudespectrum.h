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

#ifndef QGVAMPLITUDESPECTRUM_H
#define QGVAMPLITUDESPECTRUM_H

#include <vector>
#include <deque>

#include <QGraphicsView>
#include <QMenu>
#include <QTime>

#include "wmainwindow.h"
#include "fftresizethread.h"

#include "sigproc.h"
#include "ftsound.h"

class GVAmplitudeSpectrumWDialogSettings;
class MainWindow;
class QSpinBox;

class QGVAmplitudeSpectrum : public QGraphicsView
{
    Q_OBJECT

    QPen m_gridFontPen;

    QTime m_last_parameters_change;

    std::vector<FFTTYPE> m_win; // Keep one here to limit allocations

public:
    explicit QGVAmplitudeSpectrum(WMainWindow* parent);

    GVAmplitudeSpectrumWDialogSettings* m_dlgSettings;

    sigproc::FFTwrapper* m_fft;
    FFTResizeThread* m_fftresizethread;

    QGraphicsScene* m_scene;

    QToolBar* m_toolBar;
    QMenu m_contextmenu;

    FTSound::DFTParameters m_trgDFTParameters;
    std::vector<std::complex<FFTTYPE> > m_windft; // Window spectrum
    std::vector<FFTTYPE> m_filterresponse;

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
    enum CurrentAction {CANothing, CAMoving, CAZooming, CASelecting, CAMovingSelection, CAModifSelectionLeft, CAModifSelectionRight, CAModifSelectionTop, CAModifSelectionBottom, CAWaveformScale};
    int m_currentAction;

    QRectF m_selection, m_mouseSelection;
    QGraphicsRectItem* m_giShownSelection;
    QGraphicsSimpleTextItem* m_giSelectionTxt;
    void selectionSet(QRectF selection, bool forwardsync);
    void selectionSetTextInForm();
    bool hasSelection(){return m_selection.width()>0.0;}

    // Min and max limits of the color range
    QGraphicsLineItem* m_giSpectrogramMax;
    QGraphicsLineItem* m_giSpectrogramMin;

    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    void viewSet(QRectF viewrect=QRectF(), bool sync=true);
    void viewUpdateTexts();
    void drawBackground(QPainter* painter, const QRectF& rect);
    void draw_grid(QPainter* painter, const QRectF& rect);
    void draw_spectrum(QPainter* painter, std::vector<std::complex<WAVTYPE> >& ldft, double fs, double ascale, const QRectF& rect);

    ~QGVAmplitudeSpectrum();

    QAction* m_aAmplitudeSpectrumShowGrid;
    QAction* m_aAmplitudeSpectrumShowWindow;
    QAction* m_aZoomOnSelection;
    QAction* m_aSelectionClear;
    QAction* m_aZoomIn;
    QAction* m_aZoomOut;
    QAction* m_aUnZoom;
    QAction* m_aShowProperties;
    QAction* m_aAutoUpdateDFT;

signals:
    
public slots:
    void showScrollBars(bool show);

    void setWindowRange(double tstart, double tend);
    void updateSceneRect(); // To call when fs has changed and limits in dB
    void updateAmplitudeExtent();
    void amplitudeMinChanged();
    void settingsModified();
    void updateDFTs();
    void fftResizing(int prevSize, int newSize);

    void selectionZoomOn();
    void selectionClear(bool forwardsync=true);
    void azoomin();
    void azoomout();
    void aunzoom();
};

#endif // QGVAMPLITUDESPECTRUM_H
