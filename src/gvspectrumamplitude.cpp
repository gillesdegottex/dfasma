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

#include "gvspectrumamplitude.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "gvspectrumamplitudewdialogsettings.h"
#include "ui_gvspectrumamplitudewdialogsettings.h"

#include "gvwaveform.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "ftsound.h"
#include "ftfzero.h"

#include <iostream>
#include <algorithm>
using namespace std;

#include <qnumeric.h>
#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>

#include "qaesigproc.h"
#include "qaehelpers.h"

GVSpectrumAmplitude::GVSpectrumAmplitude(WMainWindow* parent)
    : QGraphicsView(parent)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVAmplitudeSpectrumWDialogSettings(this);
    gMW->m_settings.add(gMW->ui->sldAmplitudeSpectrumMin);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the spectrum view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_aAmplitudeSpectrumShowGrid = new QAction(tr("Show &grid"), this);
    m_aAmplitudeSpectrumShowGrid->setObjectName("m_aAmplitudeSpectrumShowGrid");
    m_aAmplitudeSpectrumShowGrid->setStatusTip(tr("Show or hide the grid"));
    m_aAmplitudeSpectrumShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aAmplitudeSpectrumShowGrid->setCheckable(true);
    m_aAmplitudeSpectrumShowGrid->setChecked(true);
    gMW->m_settings.add(m_aAmplitudeSpectrumShowGrid);
    m_giGrid = new QAEGIGrid(this, "Hz", "dB");
    m_giGrid->setMathYAxis(true);
    m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giGrid->setVisible(m_aAmplitudeSpectrumShowGrid->isChecked());
    m_scene->addItem(m_giGrid);
    connect(m_aAmplitudeSpectrumShowGrid, SIGNAL(toggled(bool)), this, SLOT(gridSetVisible(bool)));
//    connect(gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop, SIGNAL(triggered()), m_scene, SLOT(update()));

    m_aAmplitudeSpectrumShowWindow = new QAction(tr("Show &window"), this);
    m_aAmplitudeSpectrumShowWindow->setObjectName("m_aAmplitudeSpectrumShowWindow");
    m_aAmplitudeSpectrumShowWindow->setStatusTip(tr("Show &window"));
    m_aAmplitudeSpectrumShowWindow->setCheckable(true);
    m_aAmplitudeSpectrumShowWindow->setIcon(QIcon(":/icons/window.svg"));
    gMW->m_settings.add(m_aAmplitudeSpectrumShowWindow);
    m_giWindow = new QAEGIUniformlySampledSignal(&m_windft, 1.0, this);
    QPen windowpen(QColor(192, 192, 192));
    windowpen.setWidth(0);
    m_giWindow->setPen(windowpen);
    m_giWindow->setVisible(m_aAmplitudeSpectrumShowWindow->isChecked());
    m_scene->addItem(m_giWindow);
    connect(m_aAmplitudeSpectrumShowWindow, SIGNAL(toggled(bool)), this, SLOT(windowSetVisible(bool)));
    connect(m_aAmplitudeSpectrumShowWindow, SIGNAL(toggled(bool)), this, SLOT(updateDFTs()));

    m_aAmplitudeSpectrumShowLoudnessCurve = new QAction(tr("Show &loudness curve"), this);
    m_aAmplitudeSpectrumShowLoudnessCurve->setObjectName("m_aAmplitudeSpectrumShowLoudnessCurve");
    m_aAmplitudeSpectrumShowLoudnessCurve->setStatusTip(tr("Show the loudness curve which is use for the spectrogram weighting."));
    m_aAmplitudeSpectrumShowLoudnessCurve->setCheckable(true);
    m_aAmplitudeSpectrumShowLoudnessCurve->setChecked(false);
    m_aAmplitudeSpectrumShowLoudnessCurve->setIcon(QIcon(":/icons/noun_29196_cc.svg"));
    gMW->m_settings.add(m_aAmplitudeSpectrumShowLoudnessCurve);
    m_giLoudnessCurve = new QAEGIUniformlySampledSignal(&m_elc, 1.0, this);
    QPen elcpen(QColor(192, 192, 255));
    elcpen.setWidth(0);
    m_giLoudnessCurve->setPen(elcpen);
    m_giLoudnessCurve->setVisible(m_aAmplitudeSpectrumShowLoudnessCurve->isChecked());
    m_scene->addItem(m_giLoudnessCurve);
    connect(m_aAmplitudeSpectrumShowLoudnessCurve, SIGNAL(toggled(bool)), this, SLOT(elcSetVisible(bool)));
    connect(m_aAmplitudeSpectrumShowLoudnessCurve, SIGNAL(toggled(bool)), m_scene, SLOT(update()));

    m_aFollowPlayCursor = new QAction(tr("Follow the play cursor"), this);;
    m_aFollowPlayCursor->setObjectName("m_aFollowPlayCursor");
    m_aFollowPlayCursor->setStatusTip(tr("Update the DFT view according to the play cursor position"));
    m_aFollowPlayCursor->setCheckable(true);
    m_aFollowPlayCursor->setChecked(false);
    gMW->m_settings.add(m_aFollowPlayCursor);

    m_fft = new qae::FFTwrapper();
    qae::FFTwrapper::setTimeLimitForPlanPreparation(m_dlgSettings->ui->sbAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->value());
    m_fftresizethread = new FFTResizeThread(m_fft, this);

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -1000, 0, 1000);
    m_giCursorHoriz->hide();
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursorHoriz->setPen(cursorPen);
    m_giCursorHoriz->setZValue(100);
    m_scene->addItem(m_giCursorHoriz);
    m_giCursorVert = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giCursorVert->hide();
    m_giCursorVert->setPen(cursorPen);
    m_giCursorVert->setZValue(100);
    m_scene->addItem(m_giCursorVert);
    QFont font("Helvetica", 10);
    m_giCursorPositionXTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionXTxt->hide();
    m_giCursorPositionXTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionXTxt->setFont(font);
    m_giCursorPositionXTxt->setZValue(100);
    m_scene->addItem(m_giCursorPositionXTxt);
    m_giCursorPositionYTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionYTxt->hide();
    m_giCursorPositionYTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionYTxt->setFont(font);
    m_giCursorPositionYTxt->setZValue(100);
    m_scene->addItem(m_giCursorPositionYTxt);

    // Selection
    m_currentAction = CANothing;
    m_giShownSelection = new QGraphicsRectItem();
    m_giShownSelection->hide();
    m_scene->addItem(m_giShownSelection);
    m_giSelectionTxt = new QGraphicsSimpleTextItem();
    m_giSelectionTxt->hide();
    m_giSelectionTxt->setBrush(QColor(64, 64, 64));
    m_giSelectionTxt->setFont(font);
    m_scene->addItem(m_giSelectionTxt);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giShownSelection->setPen(selectionPen);
    m_giShownSelection->setBrush(selectionBrush);
    m_giShownSelection->setOpacity(0.5);
    gMW->ui->lblSpectrumSelectionTxt->setText("No selection");

    // Min and max limits of the color range
    cursorPen = QPen(QColor(255, 0, 0));
    cursorPen.setWidth(0);
    m_giSpectrogramMax = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giSpectrogramMax->setPen(cursorPen);
    m_giSpectrogramMax->hide();
    m_scene->addItem(m_giSpectrogramMax);
    m_giSpectrogramMin = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giSpectrogramMin->setPen(cursorPen);
    m_giSpectrogramMin->hide();
    m_scene->addItem(m_giSpectrogramMin);

    // Build actions
    m_aZoomIn = new QAction(tr("Zoom In"), this);;
    m_aZoomIn->setStatusTip(tr("Zoom In"));
    m_aZoomIn->setShortcut(Qt::Key_Plus);
    m_aZoomIn->setIcon(QIcon(":/icons/zoomin.svg"));
    connect(m_aZoomIn, SIGNAL(triggered()), this, SLOT(azoomin()));
    m_aZoomOut = new QAction(tr("Zoom Out"), this);;
    m_aZoomOut->setStatusTip(tr("Zoom Out"));
    m_aZoomOut->setShortcut(Qt::Key_Minus);
    m_aZoomOut->setIcon(QIcon(":/icons/zoomout.svg"));
    connect(m_aZoomOut, SIGNAL(triggered()), this, SLOT(azoomout()));
    m_aUnZoom = new QAction(tr("Un-Zoom"), this);
    m_aUnZoom->setStatusTip(tr("Un-Zoom"));
    m_aUnZoom->setIcon(QIcon(":/icons/unzoomxy.svg"));
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(false);
    //m_aZoomOnSelection->setShortcut(Qt::Key_S); // This one creates "ambiguous" shortcuts
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));

    m_aAutoUpdateDFT = new QAction(tr("Auto-Update DFT"), this);;
    m_aAutoUpdateDFT->setStatusTip(tr("Auto-Update the DFT view when the time selection is modified"));
    m_aAutoUpdateDFT->setCheckable(true);
    m_aAutoUpdateDFT->setChecked(true);
    m_aAutoUpdateDFT->setIcon(QIcon(":/icons/autoupdate.svg"));
    connect(m_aAutoUpdateDFT, SIGNAL(toggled(bool)), this, SLOT(settingsModified()));

    gMW->ui->lblSpectrumSelectionTxt->setText("No selection");

    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    gMW->ui->pgbFFTResize->hide();
    gMW->ui->lblSpectrumInfoTxt->setText("");

    connect(m_fftresizethread, SIGNAL(fftResized(int,int)), this, SLOT(updateDFTs()));
    connect(m_fftresizethread, SIGNAL(fftResizing(int,int)), this, SLOT(fftResizing(int,int)));

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aAutoUpdateDFT);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(), gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lSpectraToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowGrid);
//    m_contextmenu.addAction(gMW->m_gvWaveform->m_aShowSelectedWaveformOnTop);
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowWindow);
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowLoudnessCurve);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aAutoUpdateDFT);
    m_contextmenu.addAction(m_aFollowPlayCursor);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(show()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(settingsModified()));

    connect(gMW->ui->sldAmplitudeSpectrumMin, SIGNAL(valueChanged(int)), this, SLOT(amplitudeMinChanged()));
}

void GVSpectrumAmplitude::gridSetVisible(bool visible){m_giGrid->setVisible(visible);}

void GVSpectrumAmplitude::windowSetVisible(bool visible){m_giWindow->setVisible(visible);}

void GVSpectrumAmplitude::elcSetVisible(bool visible){m_giLoudnessCurve->setVisible(visible);}

void GVSpectrumAmplitude::updateScrollBars(){
    if(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked()){
        gMW->m_gvSpectrumAmplitude->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        gMW->m_gvSpectrumPhase->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        gMW->m_gvSpectrumGroupDelay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        if(gMW->ui->actionShowPhaseSpectrum->isChecked()
           || gMW->ui->actionShowGroupDelaySpectrum->isChecked())
            gMW->m_gvSpectrumAmplitude->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        if(gMW->ui->actionShowGroupDelaySpectrum->isChecked()){
            gMW->m_gvSpectrumAmplitude->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            gMW->m_gvSpectrumPhase->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
        gMW->m_gvSpectrumAmplitude->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        gMW->m_gvSpectrumPhase->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        gMW->m_gvSpectrumGroupDelay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        gMW->m_gvSpectrumAmplitude->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gMW->m_gvSpectrumPhase->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gMW->m_gvSpectrumGroupDelay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gMW->m_gvSpectrumAmplitude->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gMW->m_gvSpectrumPhase->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gMW->m_gvSpectrumGroupDelay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void GVSpectrumAmplitude::settingsModified(){
//    COUTD << "QGVAmplitudeSpectrum::settingsModified " << gMW->m_gvWaveform->m_mouseSelection << endl;
    if(gMW->m_gvWaveform)
        gMW->m_gvWaveform->selectionSet(gMW->m_gvWaveform->m_mouseSelection, true);
}

void GVSpectrumAmplitude::updateAmplitudeExtent(){
//    COUTD << "QGVAmplitudeSpectrum::updateAmplitudeExtent" << endl;

    if(gFL->ftsnds.size()>0){
        gMW->ui->sldAmplitudeSpectrumMin->setMaximum(0);
        gMW->ui->sldAmplitudeSpectrumMin->setMinimum(-3*gFL->getMaxSQNR()); // to give a margin

        updateSceneRect();
    }

//    COUTD << "QGVAmplitudeSpectrum::~updateAmplitudeExtent" << endl;
}

void GVSpectrumAmplitude::amplitudeMinChanged() {
//    COUTD << "QGVSpectrumAmplitude::amplitudeMinChanged " << gMW->ui->sldAmplitudeSpectrumMin->value() << endl;

    if(!gMW->isLoading())
        QToolTip::showText(QCursor::pos(), QString("%1dB").arg(gMW->ui->sldAmplitudeSpectrumMin->value()), this);

    updateSceneRect();
    viewSet(m_scene->sceneRect(), false);

//    cout << "QGVAmplitudeSpectrum::~amplitudeMinChanged" << endl;
}

void GVSpectrumAmplitude::updateSceneRect() {
//    COUTD << "QGVSpectrumAmplitude::updateSceneRect " << gFL->getFs() << " " << (10-gMW->ui->sldAmplitudeSpectrumMin->value()) << endl;
    m_scene->setSceneRect(0.0, -10, gFL->getFs()/2, (10-gMW->ui->sldAmplitudeSpectrumMin->value()));
    m_giSpectrogramMax->setLine(QLineF(0, 0, gFL->getFs()/2.0, 0));
    m_giSpectrogramMin->setLine(QLineF(0, 0, gFL->getFs()/2.0, 0));
}

void GVSpectrumAmplitude::fftResizing(int prevSize, int newSize){
    Q_UNUSED(prevSize);

    gMW->ui->pgbFFTResize->show();
    gMW->ui->lblSpectrumInfoTxt->setText(QString("Optimizing DFT for %1").arg(newSize));
}

void GVSpectrumAmplitude::setSamplingRate(double fs)
{
    m_giWindow->setSamplingRate(fs);

    int dftlen = 2*4096; // TODO
    m_giLoudnessCurve->setSamplingRate(1.0/(fs/dftlen));
    m_elc.resize(dftlen/2, 0.0);
    for(size_t u=0; u<m_elc.size(); ++u)
        m_elc[u] = -qae::equalloudnesscurvesISO226(fs*double(u)/dftlen, 0);
    m_giLoudnessCurve->updateMinMaxValues();
}

void GVSpectrumAmplitude::setWindowRange(qreal tstart, qreal tend){
//    DCOUT << "GVSpectrumAmplitude::setWindowRange " << tstart << "," << tend << endl;

    if(tstart==tend)
        return;

    if(m_dlgSettings->ui->cbAmplitudeSpectrumLimitWindowDuration->isChecked() && (tend-tstart)>m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value())
        tend = tstart+m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value();

    unsigned int nl = std::max(0, int(0.5+tstart*gFL->getFs()));
    unsigned int nr = int(0.5+std::min(gFL->getMaxLastSampleTime(),tend)*gFL->getFs());

    if((nr-nl+1)%2==0
       && m_dlgSettings->ui->cbAmplitudeSpectrumWindowSizeForcedOdd->isChecked())
        nr++;

    if(nl==nr)
        return;

    int winlen = nr-nl+1;
    if(winlen<2)
        return;

    if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==0
       && winlen>m_dlgSettings->ui->sbAmplitudeSpectrumDFTSize->value())
        winlen = m_dlgSettings->ui->sbAmplitudeSpectrumDFTSize->value();

    // The window's shape
    int wintype = m_dlgSettings->ui->cbAmplitudeSpectrumWindowType->currentIndex();

    int normtype = m_dlgSettings->ui->cbAmplitudeSpectrumWindowsNormalisation->currentIndex();

    FTSound::DFTParameters newDFTParams(nl, nr, winlen, wintype, normtype);

    if(m_trgDFTParameters.isEmpty()
       || m_trgDFTParameters.winlen!=newDFTParams.winlen
       || m_trgDFTParameters.wintype!=newDFTParams.wintype
       || m_trgDFTParameters.normtype!=newDFTParams.normtype
       || wintype>7){

        if(wintype==0)
            m_win = qae::rectangular(newDFTParams.winlen);
        else if(wintype==1)
            m_win = qae::hamming(newDFTParams.winlen);
        else if(wintype==2)
            m_win = qae::hann(newDFTParams.winlen);
        else if(wintype==3)
            m_win = qae::blackman(newDFTParams.winlen);
        else if(wintype==4)
            m_win = qae::blackmannutall(newDFTParams.winlen);
        else if(wintype==5)
            m_win = qae::blackmanharris(newDFTParams.winlen);
        else if(wintype==6)
            m_win = qae::nutall(newDFTParams.winlen);
        else if(wintype==7)
            m_win = qae::flattop(newDFTParams.winlen);
        else if(wintype==8)
            m_win = qae::normwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowNormSigma->value());
        else if(wintype==9)
            m_win = qae::expwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowExpDecay->value());
        else if(wintype==10)
            m_win = qae::gennormwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowNormSigma->value(), m_dlgSettings->ui->spAmplitudeSpectrumWindowNormPower->value());
        else
            throw QString("No window selected");

        double winsum = 0.0;
        if(normtype==0) {
            // Normalize the window's sum to 1
            for(int n=0; n<newDFTParams.winlen; n++)
                winsum += m_win[n];
        }
        else if(normtype==1) {
            // Normalize the window's energy to 1
            for(int n=0; n<newDFTParams.winlen; n++)
                winsum += m_win[n]*m_win[n];
        }
        for(int n=0; n<newDFTParams.winlen; n++)
            m_win[n] /= winsum;

        newDFTParams.win = m_win;
    }

    // Set the DFT length
    if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==0)
        newDFTParams.dftlen = m_dlgSettings->ui->sbAmplitudeSpectrumDFTSize->value();
    else if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==1)
        newDFTParams.dftlen = std::pow(2.0, std::ceil(log2(float(newDFTParams.winlen)))+m_dlgSettings->ui->sbAmplitudeSpectrumOversamplingFactor->value());
    else if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==2){
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        int dftlen = viewport()->rect().width()/((viewrect.right()-viewrect.left())/gFL->getFs());
        dftlen = std::max(dftlen, newDFTParams.winlen);
        newDFTParams.dftlen = std::pow(2.0, std::ceil(log2(float(dftlen))));
    }

    if(newDFTParams==m_trgDFTParameters)
        return;

    // From now on we want the new parameters ...
    m_trgDFTParameters = newDFTParams;

    if(gMW->m_gvSpectrumGroupDelay
        && gMW->ui->actionShowGroupDelaySpectrum->isChecked())
        gMW->m_gvSpectrumGroupDelay->updateSceneRect();

    // Update the visible window in the waveform
    if(m_trgDFTParameters.win.size()>0) {
        double fs = gFL->getFs();

        FFTTYPE winmax = 0.0;
        for(size_t n=0; n<m_trgDFTParameters.win.size(); n++)
            winmax = std::max(winmax, m_trgDFTParameters.win[n]);
        winmax = 1.0/winmax;

        QPainterPath path;

        qreal prevx = 0;
        qreal prevy = m_trgDFTParameters.win[0]*winmax;
        path.moveTo(QPointF(prevx, -prevy));
        qreal y;
        for(size_t n=1; n<m_trgDFTParameters.win.size(); n++) {
            qreal x = n/fs;
            y = m_trgDFTParameters.win[n];
            y *= winmax;
            path.lineTo(QPointF(x, -y));
            prevx = x;
            prevy = y;
        }

        // Add the vertical line
        qreal winrelcenter = ((m_trgDFTParameters.win.size()-1)/2.0)/fs;
        path.moveTo(QPointF(winrelcenter, 2.0));
        path.lineTo(QPointF(winrelcenter, -1.0));

        gMW->m_gvWaveform->m_giWindow->setPath(path);
        gMW->m_gvWaveform->m_giWindow->setPos(tstart, 0.0);
    }

    // ... so let's see which DFTs we have to update.
    if(m_aAutoUpdateDFT->isChecked())
        updateDFTs();
}

void GVSpectrumAmplitude::updateDFTs(){
//    COUTD << "GVSpectrumAmplitude::updateDFTs " << endl;
    if(m_trgDFTParameters.win.size()<2) // Avoid the DFT of one sample ...
        return;

    m_fftresizethread->resize(m_trgDFTParameters.dftlen);

    if(m_fftresizethread->m_mutex_resizing.tryLock()){
        int dftlen = m_fft->size(); // Local copy of the actual dftlen

        gMW->ui->pgbFFTResize->hide();
        gMW->ui->lblSpectrumInfoTxt->setText(QString("DFT size=%1").arg(dftlen));

        std::vector<FFTTYPE>& win = m_trgDFTParameters.win; // For speeding up access

        bool didany = false;
        for(unsigned int fi=0; fi<gFL->ftsnds.size(); fi++){
            FTSound* snd = gFL->ftsnds[fi];
            if(!snd->isVisible())
                continue;

            if(!snd->m_dftparams.isEmpty()
               && snd->m_dftparams==m_trgDFTParameters
               && snd->m_dftparams.wav==snd->wavtoplay
               && snd->m_dftparams.ampscale==snd->m_giWavForWaveform->gain()
               && snd->m_dftparams.delay==snd->m_giWavForWaveform->delay())
                continue;

            WAVTYPE gain = snd->m_giWavForWaveform->gain();

            int n = 0;
            int wn = 0;
            for(; n<m_trgDFTParameters.winlen; n++){
                wn = m_trgDFTParameters.nl+n - snd->m_giWavForWaveform->delay();

                if(wn>=0 && wn<int(snd->wavtoplay->size())) {
                    WAVTYPE value = gain*(*(snd->wavtoplay))[wn];

                    if(value>1.0)       value = 1.0;
                    else if(value<-1.0) value = -1.0;

                    m_fft->in[n] = value*win[n];
                }
                else
                    m_fft->in[n] = 0.0;
            }
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute(); // Compute the DFT

            // Store first the complex values of the DFT
            // (so that it can be used to compute the group delay)
            std::vector<std::complex<WAVTYPE> > dft;
            dft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                dft[n] = m_fft->out[n];

            snd->m_dftamp.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                snd->m_dftamp[n] = 20*std::log10(std::abs(m_fft->out[n]));
            snd->m_giWavForSpectrumAmplitude->updateMinMaxValues();
            snd->m_giWavForSpectrumAmplitude->setSamplingRate(1.0/double(gFL->getFs()/dftlen));
            snd->m_giWavForSpectrumAmplitude->clearCache();

            snd->m_dftphase.resize(dftlen/2+1);
            double delay = (2.0*M_PI*(win.size()-1)/2.0)/dftlen;
            for(n=0; n<dftlen/2+1; n++){
                if(qIsInf(snd->m_dftamp[n]))
                    snd->m_dftphase[n] = std::numeric_limits<WAVTYPE>::infinity();
                else
                    snd->m_dftphase[n] = qae::wrap(std::arg(m_fft->out[n])+delay*n);
            }
            snd->m_giWavForSpectrumPhase->updateMinMaxValues();
            snd->m_giWavForSpectrumPhase->setSamplingRate(1.0/double(gFL->getFs()/dftlen));
            snd->m_giWavForSpectrumPhase->clearCache();

            // If the group delay is requested, update its data
            if(gMW->ui->actionShowGroupDelaySpectrum->isChecked()){
                // y = nx[n]
                for(int n=0; n<m_trgDFTParameters.winlen; n++)
                    m_fft->in[n] *= n;

                m_fft->execute(); // Compute the DFT of y

                // (Xr*Yr+Xi*Yi) / |X|^2
                snd->m_dftgd.resize(dftlen/2+1);
                WAVTYPE fs = gFL->getFs();
                WAVTYPE delay = ((m_trgDFTParameters.winlen-1)/2)/fs;
                for(int n=0; n<dftlen/2+1; n++) {
                    if(qIsInf(snd->m_dftamp[n]))
                        snd->m_dftgd[n] = std::numeric_limits<WAVTYPE>::infinity();
                    else {
                        WAVTYPE xp2 = std::real(dft[n])*std::real(dft[n]) + std::imag(dft[n])*std::imag(dft[n]);
                        snd->m_dftgd[n] = (std::real(dft[n])*std::real(m_fft->out[n]) + std::imag(dft[n])*std::imag(m_fft->out[n]))/xp2;

                        snd->m_dftgd[n] /= fs; // measure it in [second]

                        snd->m_dftgd[n] -= delay; // Remove the window's delay
                    }
                }
                snd->m_giWavForSpectrumGroupDelay->updateMinMaxValues();
                snd->m_giWavForSpectrumGroupDelay->setSamplingRate(1.0/double(gFL->getFs()/dftlen));
                snd->m_giWavForSpectrumGroupDelay->clearCache();
            }

            // Convert the spectrum values to log values
            snd->m_dftparams = m_trgDFTParameters;
            snd->m_dftparams.wav = snd->wavtoplay;
            snd->m_dftparams.ampscale = snd->m_giWavForWaveform->gain();
            snd->m_dftparams.delay = snd->m_giWavForWaveform->delay();

            didany = true;
        }

        // Compute the window's DFT
        if (m_aAmplitudeSpectrumShowWindow->isChecked()) {

            int n = 0;
            for(; n<m_trgDFTParameters.winlen; n++)
                m_fft->in[n] = m_trgDFTParameters.win[n];
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute();

            m_windft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                m_windft[n] = qae::mag2db(m_fft->out[n]);
            m_giWindow->updateMinMaxValues();

            m_giWindow->setSamplingRate(1.0/double(gFL->getFs()/dftlen));
            m_giWindow->updateGeometry();
            m_giWindow->clearCache();

            didany = true;
        }

        m_fftresizethread->m_mutex_resizing.unlock();

        if(didany){
            m_scene->update();
            if(gMW->m_gvSpectrumPhase)
                gMW->m_gvSpectrumPhase->m_scene->update();
            if(gMW->m_gvSpectrumGroupDelay)
                gMW->m_gvSpectrumGroupDelay->m_scene->update();
        }
    }

//    COUTD << "~QGVAmplitudeSpectrum::updateDFTs" << endl;
}

void GVSpectrumAmplitude::viewSet(QRectF viewrect, bool sync) {
//    cout << "QGVAmplitudeSpectrum::viewSet" << endl;

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

        QPointF center = viewrect.center();
        double hzeps = 1e-10;
        if(viewrect.width()<hzeps){
            viewrect.setLeft(center.x()-0.5*hzeps);
            viewrect.setRight(center.x()+0.5*hzeps);
        }
        double dbeps = 1e-10;
        if(viewrect.height()<dbeps){
            viewrect.setTop(center.x()-0.5*dbeps);
            viewrect.setBottom(center.x()+0.5*dbeps);
        }

        if(viewrect.top()<=m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top());
        if(viewrect.bottom()>=m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom());
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        // This is not perfect and might never be because:
        // 1) The workaround removeHiddenMargin is apparently simplistic
        // 2) This position in real coordinates involves also the position of the
        //    scrollbars which fit integers only. Thus the final position is always
        //    aproximative.
        // A solution would be to subclass QSplitter, catch the view when the
        // splitter's handle is clicked, and repeat this view until button released.
        fitInView(removeHiddenMargin(this, viewrect));

        for(size_t fi=0; fi<gFL->ftfzeros.size(); fi++)
            gFL->ftfzeros[fi]->updateTextsGeometry();

        m_giGrid->updateLines();

        if(sync){
            if(gMW->m_gvSpectrumPhase && gMW->ui->actionShowPhaseSpectrum->isChecked()) {
                QRectF phaserect = gMW->m_gvSpectrumPhase->mapToScene(gMW->m_gvSpectrumPhase->viewport()->rect()).boundingRect();
                phaserect.setLeft(viewrect.left());
                phaserect.setRight(viewrect.right());
                gMW->m_gvSpectrumPhase->viewSet(phaserect, false);
            }

            if(gMW->m_gvSpectrumGroupDelay && gMW->ui->actionShowGroupDelaySpectrum->isChecked()) {
                QRectF gdrect = gMW->m_gvSpectrumGroupDelay->mapToScene(gMW->m_gvSpectrumGroupDelay->viewport()->rect()).boundingRect();
                gdrect.setLeft(viewrect.left());
                gdrect.setRight(viewrect.right());
                gMW->m_gvSpectrumGroupDelay->viewSet(gdrect, false);
            }
        }
    }

    if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==2)
        setWindowRange(gMW->m_gvWaveform->m_selection.left(), gMW->m_gvWaveform->m_selection.right());

//    cout << "QGVAmplitudeSpectrum::~viewSet" << endl;
}

void GVSpectrumAmplitude::contextMenuEvent(QContextMenuEvent *event){
    if (event->modifiers().testFlag(Qt::ShiftModifier)
        || event->modifiers().testFlag(Qt::ControlModifier))
        return;

    QPoint posglobal = mapToGlobal(event->pos()+QPoint(0,0));
    m_contextmenu.exec(posglobal);
}

void GVSpectrumAmplitude::resizeEvent(QResizeEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::resizeEvent" << endl;

    // Note: Resized is called for all views so better to not forward modifications
    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {

        updateSceneRect();

        if(gMW->m_gvSpectrumPhase->viewport()->rect().width()*gMW->m_gvSpectrumPhase->viewport()->rect().height()>0){
            QRectF phaserect = gMW->m_gvSpectrumPhase->mapToScene(gMW->m_gvSpectrumPhase->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(phaserect.left());
            viewrect.setRight(phaserect.right());
            viewrect.setTop(-10);
            viewrect.setBottom(-gMW->ui->sldAmplitudeSpectrumMin->value());
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);

        if(gMW->m_gvWaveform->m_selection.width()>0)
            setWindowRange(gMW->m_gvWaveform->m_selection.left(), gMW->m_gvWaveform->m_selection.right());
    }
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    viewUpdateTexts();
    setMouseCursorPosition(QPointF(-1,0), false);

    if((event->oldSize().width()==-1 || event->oldSize().height()==-1
            || event->oldSize().width()*event->oldSize().height()==0)
       && event->size().width()*event->size().height()>0)
        updateDFTs();

//    COUTD << "QGVAmplitudeSpectrum::~resizeEvent" << endl;
}

void GVSpectrumAmplitude::scrollContentsBy(int dx, int dy) {
//    cout << "QGVAmplitudeSpectrum::scrollContentsBy" << endl;

    setMouseCursorPosition(QPointF(-1,0), false);
    viewUpdateTexts();

    QGraphicsView::scrollContentsBy(dx, dy);

    m_giGrid->updateLines();
}

void GVSpectrumAmplitude::wheelEvent(QWheelEvent* event) {

    qreal numDegrees = (event->angleDelta() / 8).y();
    // Clip to avoid flipping (workaround of a Qt bug ?)
    if(numDegrees>90) numDegrees = 90;
    if(numDegrees<-90) numDegrees = -90;

    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 += 0.01*h11*numDegrees;
    h22 += 0.01*h22*numDegrees;
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    m_aZoomOnSelection->setEnabled(!m_selection.isEmpty());
    m_aZoomOut->setEnabled(true);
    m_aZoomIn->setEnabled(true);
//    m_aUnZoom->setEnabled(true);
}

void GVSpectrumAmplitude::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setMouseCursorPosition(QPointF(-1,0), false);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.left()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()) {
                // Resize left boundary of the selection
                m_currentAction = CAModifSelectionLeft;
                m_selection_pressedp = QPointF(p.x()-m_selection.left(), 0);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.right()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()){
                // Resize right boundary of the selection
                m_currentAction = CAModifSelectionRight;
                m_selection_pressedp = QPointF(p.x()-m_selection.right(), 0);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.top()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
                // Resize top boundary of the selection
                m_currentAction = CAModifSelectionTop;
                m_selection_pressedp = QPointF(0, p.y()-m_selection.top());
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.bottom()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
                // Resize bottom boundary of the selection
                m_currentAction = CAModifSelectionBottom;
                m_selection_pressedp = QPointF(0, p.y()-m_selection.bottom());
            }
            else if((m_selection.width()>0 && m_selection.height()>0) && (event->modifiers().testFlag(Qt::ControlModifier) || (p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom()))){
                // When scroling the selection
                m_currentAction = CAMovingSelection;
                m_selection_pressedp = p;
                m_mouseSelection = m_selection;
                setCursor(Qt::ClosedHandCursor);
        //            gMW->ui->lblSpectrumSelectionTxt->setText(QString("Selection [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));
            }
            else {
                // When selecting
                m_currentAction = CASelecting;
                m_selection_pressedp = p;
                m_mouseSelection.setTopLeft(m_selection_pressedp);
                m_mouseSelection.setBottomRight(m_selection_pressedp);
                selectionSet(m_mouseSelection, true);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
//                 TODO
            }
            else{
                // When scaling the waveform
                m_currentAction = CAWaveformScale;
                m_selection_pressedp = p;
                gMW->setEditing(gFL->getCurrentFTSound(true));
                setCursor(Qt::SizeVerCursor);
            }
        }
    }
    else if(event->buttons()&Qt::RightButton) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            setCursor(Qt::CrossCursor);
            m_currentAction = CAZooming;
            m_selection_pressedp = p;
            m_pressed_mouseinviewport = mapFromScene(p);
            m_pressed_viewrect = mapToScene(viewport()->rect()).boundingRect();
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void GVSpectrumAmplitude::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVAmplitudeSpectrum::mouseMoveEvent" << endl;

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p, true);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
        setMouseCursorPosition(QPointF(-1,0), false);
        m_giGrid->updateLines();
    }
    else if(m_currentAction==CAZooming) {
        double dx = -(event->pos()-m_pressed_mouseinviewport).x()/100.0;
        double dy = (event->pos()-m_pressed_mouseinviewport).y()/100.0;

        QRectF newrect = m_pressed_viewrect;

        newrect.setLeft(m_selection_pressedp.x()-(m_selection_pressedp.x()-m_pressed_viewrect.left())*exp(dx));
        newrect.setRight(m_selection_pressedp.x()+(m_pressed_viewrect.right()-m_selection_pressedp.x())*exp(dx));

        newrect.setTop(m_selection_pressedp.y()-(m_selection_pressedp.y()-m_pressed_viewrect.top())*exp(dy));
        newrect.setBottom(m_selection_pressedp.y()+(m_pressed_viewrect.bottom()-m_selection_pressedp.y())*exp(dy));
        viewSet(newrect);

        viewUpdateTexts();
        setMouseCursorPosition(m_selection_pressedp, true);

        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionTop){
        m_mouseSelection.setTop(p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionBottom){
        m_mouseSelection.setBottom(p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y(), p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAWaveformScale){
        // When scaling the waveform
        FTSound* currentftsound = gFL->getCurrentFTSound(true);
        if(currentftsound){
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                gMW->setEditing(NULL);
                m_currentAction = CANothing;
            }
            else {
                currentftsound->m_giWavForWaveform->setGain(currentftsound->m_giWavForWaveform->gain()*std::pow(10, -(p.y()-m_selection_pressedp.y())/20.0));

                m_selection_pressedp = p;

                if(currentftsound->m_giWavForWaveform->gain()>1e10)
                    currentftsound->m_giWavForWaveform->setGain(1e10);
                else if(currentftsound->m_giWavForWaveform->gain()<1e-10)
                    currentftsound->m_giWavForWaveform->setGain(1e-10);

                currentftsound->needDFTUpdate();

                currentftsound->setStatus();
                updateDFTs();
                gMW->m_gvWaveform->m_scene->update();
                gFL->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
                if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
                    gMW->m_gvSpectrogram->updateSTFTSettings();
            }
        }
    }
    else{
        QRect selview = mapFromScene(m_selection).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.left()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.right()-event->x())<5 && selview.top()<=event->y() && selview.bottom()>=event->y())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.top()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.bottom()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                    setCursor(Qt::OpenHandCursor);
                else
                    setCursor(Qt::CrossCursor);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
            }
        }
    }

//    std::cout << "~GVSpectrumAmplitude::mouseMoveEvent" << endl;
    QGraphicsView::mouseMoveEvent(event);
}

void GVSpectrumAmplitude::mouseReleaseEvent(QMouseEvent* event) {
//    std::cout << "QGVAmplitudeSpectrum::mouseReleaseEvent " << endl;

    QPointF p = mapToScene(event->pos());

    m_currentAction = CANothing;

    if(gMW->ui->actionSelectionMode->isChecked()) {
        if(event->modifiers().testFlag(Qt::ShiftModifier)) {
            setCursor(Qt::OpenHandCursor);
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)) {
            setCursor(Qt::OpenHandCursor);
        }
        else{
            if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                setCursor(Qt::OpenHandCursor);
            else
                setCursor(Qt::CrossCursor);
        }
    }
    else if(gMW->ui->actionEditMode->isChecked()) {
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)) {
        }
        else{
            setCursor(Qt::SizeVerCursor);
        }
    }

    if(abs(m_selection.width())<=0 || abs(m_selection.height())<=0)
        selectionClear();
    else {
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void GVSpectrumAmplitude::keyPressEvent(QKeyEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape) {
        if(!hasSelection()) {
            if(!gMW->m_gvSpectrogram->hasSelection()
                && !gMW->m_gvWaveform->hasSelection())
                gMW->m_gvWaveform->playCursorSet(0.0, true);

            gMW->m_gvWaveform->selectionClear();
        }
        selectionClear();
    }
    if(event->key()==Qt::Key_S)
        selectionZoomOn();

    QGraphicsView::keyPressEvent(event);
}

void GVSpectrumAmplitude::selectionClear(bool forwardsync) {
    Q_UNUSED(forwardsync)

    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    if(gMW->m_gvSpectrumPhase)
        gMW->m_gvSpectrumPhase->selectionClear();
    if(gMW->m_gvSpectrumGroupDelay)
        gMW->m_gvSpectrumGroupDelay->selectionClear();

    if(forwardsync){
        if(gMW->m_gvSpectrogram){
            if(gMW->m_gvSpectrogram->m_giShownSelection->isVisible()) {
                QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
                if(std::abs(rect.left()-gMW->m_gvSpectrogram->m_scene->sceneRect().left())<std::numeric_limits<double>::epsilon()
                    && std::abs(rect.right()-gMW->m_gvSpectrogram->m_scene->sceneRect().right())<std::numeric_limits<double>::epsilon()){
                    gMW->m_gvSpectrogram->selectionClear();
                }
                else {
                    rect.setTop(-gFL->getFs()/2);
                    rect.setBottom(0.0);
                    gMW->m_gvSpectrogram->selectionSet(rect, false);
                }
            }
        }
    }

    selectionSetTextInForm();

    m_scene->update();
}

void GVSpectrumAmplitude::selectionSetTextInForm() {

    QString str;

//    cout << "QGVAmplitudeSpectrum::selectionSetText: " << m_selection.height() << " " << gMW->m_gvPhaseSpectrum->m_selection.height() << endl;

    if (m_selection.height()==0 && gMW->m_gvSpectrumPhase->m_selection.height()==0) {
        str = "No selection";
    }
    else {
        str += QString("");

        double left, right;
        if(m_selection.height()>0) {
            left = m_selection.left();
            right = m_selection.right();
        }
        else {
            left = gMW->m_gvSpectrumPhase->m_selection.left();
            right = gMW->m_gvSpectrumPhase->m_selection.right();
        }
        // TODO The line below cannot be avoided exept by reversing the y coordinate of the
        //      whole seen of the spectrogram, and I don't know how to do this :(
        if(std::abs(left)<1e-10) left=0.0;

        str += QString("[%1,%2]%3Hz").arg(left).arg(right).arg(right-left);

        if (gMW->m_gvSpectrumAmplitude->isVisible() && m_selection.height()>0) {
            str += QString(" x [%4,%5]%6dB").arg(-m_selection.bottom()).arg(-m_selection.top()).arg(m_selection.height());
        }
//        if (gMW->m_gvPhaseSpectrum->isVisible() && gMW->m_gvPhaseSpectrum->m_selection.height()>0) {
//            str += QString(" x [%7,%8]%9rad").arg(-gMW->m_gvPhaseSpectrum->m_selection.bottom()).arg(-gMW->m_gvPhaseSpectrum->m_selection.top()).arg(gMW->m_gvPhaseSpectrum->m_selection.height());
//        }
    }

    gMW->ui->lblSpectrumSelectionTxt->setText(str);
}

void GVSpectrumAmplitude::selectionSet(QRectF selection, bool forwardsync) {

    // Order the selection to avoid negative width and negative height
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }
//    COUTD << selection << " " << selection.width() << "x" << selection.height() << std::endl;
    if(selection.top()>selection.bottom()){
        float tmp = selection.top();
        selection.setTop(selection.bottom());
        selection.setBottom(tmp);
    }
//    COUTD << selection << " " << selection.width() << "x" << selection.height() << std::endl;

    m_selection = m_mouseSelection = selection;

    if(m_selection.left()<0) m_selection.setLeft(0);
    if(m_selection.right()>gFL->getFs()/2.0) m_selection.setRight(gFL->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    m_giShownSelection->setRect(m_selection);
    m_giShownSelection->show();

    m_giSelectionTxt->setText(QString("%1Hz,%2dB").arg(m_selection.width()).arg(m_selection.height()));
//    m_giSelectionTxt->show();
    viewUpdateTexts();

    selectionSetTextInForm();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());

    if(forwardsync) {
        if(gMW->m_gvSpectrumPhase){
            QRectF rect = gMW->m_gvSpectrumPhase->m_mouseSelection;
            rect.setLeft(m_mouseSelection.left());
            rect.setRight(m_mouseSelection.right());
            if(rect.height()==0) {
                rect.setTop(gMW->m_gvSpectrumPhase->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrumPhase->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrumPhase->selectionSet(rect, false);
        }

        if(gMW->m_gvSpectrumGroupDelay){
            QRectF rect = gMW->m_gvSpectrumGroupDelay->m_mouseSelection;
            rect.setLeft(m_mouseSelection.left());
            rect.setRight(m_mouseSelection.right());
            if(rect.height()==0) {
                rect.setTop(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrumGroupDelay->selectionSet(rect, false);
        }

        if(gMW->m_gvSpectrogram){
            QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
            rect.setTop(-m_mouseSelection.right());
            rect.setBottom(-m_mouseSelection.left());
            if(!gMW->m_gvSpectrogram->m_giShownSelection->isVisible()) {
                rect.setLeft(gMW->m_gvSpectrogram->m_scene->sceneRect().left());
                rect.setRight(gMW->m_gvSpectrogram->m_scene->sceneRect().right());
            }
            gMW->m_gvSpectrogram->selectionSet(rect, false);
        }
    }

    selectionSetTextInForm();
}

void GVSpectrumAmplitude::viewUpdateTexts() {
    QTransform trans = transform();
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(), 1.0/trans.m22());

    // Cursor
    m_giCursorPositionXTxt->setTransform(txttrans);
    m_giCursorPositionYTxt->setTransform(txttrans);

    // Selection
    QRectF br = m_giSelectionTxt->boundingRect();
    m_giSelectionTxt->setTransform(txttrans);
    m_giSelectionTxt->setPos(m_selection.center()-QPointF(0.5*br.width()/trans.m11(), 0.5*br.height()/trans.m22()));
}

void GVSpectrumAmplitude::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
            zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
            zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
            zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        }
        viewSet(zoomonrect);

        viewUpdateTexts();

        if(gMW->m_gvSpectrumPhase)
            gMW->m_gvSpectrumPhase->viewUpdateTexts();
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->viewUpdateTexts();

        setMouseCursorPosition(QPointF(-1,0), false);
//        m_aZoomOnSelection->setEnabled(false);
    }
}

void GVSpectrumAmplitude::azoomin(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void GVSpectrumAmplitude::azoomout(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void GVSpectrumAmplitude::aunzoom(){

    // Compute max and min among all visible files
    FFTTYPE ymin = std::numeric_limits<FFTTYPE>::infinity();
    FFTTYPE ymax = -std::numeric_limits<FFTTYPE>::infinity();
    for(unsigned int fi=0; fi<gFL->ftsnds.size(); fi++){
        if(!gFL->ftsnds[fi]->isVisible())
            continue;

        ymin = std::min(ymin, gFL->ftsnds[fi]->m_giWavForSpectrumAmplitude->getMinValue());
        ymax = std::max(ymax, gFL->ftsnds[fi]->m_giWavForSpectrumAmplitude->getMaxValue());
    }
    ymin = ymin-3;
    ymax = ymax+3;

    QRectF rect = QRectF(0.0, -ymax, gFL->getFs()/2, (ymax-ymin));

    if(rect.bottom()>(-gMW->ui->sldAmplitudeSpectrumMin->value()))
        rect.setBottom(-gMW->ui->sldAmplitudeSpectrumMin->value());
    if(rect.top()<-ymax)
        rect.setTop(m_scene->sceneRect().top());

    viewSet(rect, false);

//    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    cout << "QGVAmplitudeSpectrum::aunzoom viewrect: " << viewrect.left() << "," << viewrect.right() << " X " << viewrect.top() << "," << viewrect.bottom() << endl;

    if(gMW->m_gvSpectrumPhase)
        gMW->m_gvSpectrumPhase->viewSet(QRectF(0.0, -M_PI, gFL->getFs()/2, 2*M_PI), false);
    if(gMW->m_gvSpectrumGroupDelay)
        gMW->m_gvSpectrumGroupDelay->viewSet(QRectF(0.0, sceneRect().top(), gFL->getFs()/2, sceneRect().height()), false);

//    cursorUpdate(QPointF(-1,0));
//    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void GVSpectrumAmplitude::setMouseCursorPosition(QPointF p, bool forwardsync) {
    QFontMetrics qfm(gMW->m_dlgSettings->ui->lblGridFontSample->font());

    QLineF line;
    line.setP1(QPointF(p.x(), m_giCursorVert->line().y1()));
    line.setP2(QPointF(p.x(), m_giCursorVert->line().y2()));
    m_giCursorVert->setLine(line);
    line.setP1(QPointF(m_giCursorHoriz->line().x1(), p.y()));
    line.setP2(QPointF(m_giCursorHoriz->line().x2(), p.y()));
    m_giCursorHoriz->setLine(line);

    if(m_giCursorVert->line().x1()==-1){
        m_giCursorHoriz->hide();
        m_giCursorVert->hide();
        m_giCursorPositionXTxt->hide();
        m_giCursorPositionYTxt->hide();
    }
    else {
        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());

        m_giCursorHoriz->show();
        m_giCursorHoriz->setLine(viewrect.right()-50/trans.m11(), m_giCursorHoriz->line().y1(), gFL->getFs()/2.0, m_giCursorHoriz->line().y1());
        m_giCursorVert->show();
        m_giCursorVert->setLine(m_giCursorVert->line().x1(), viewrect.top(), m_giCursorVert->line().x1(), viewrect.top()+(qfm.height())/trans.m22());
        m_giCursorPositionXTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionYTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionXTxt->show();
        m_giCursorPositionYTxt->show();

        m_giCursorPositionXTxt->setTransform(txttrans);
        m_giCursorPositionYTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionXTxt->boundingRect();
        qreal x = m_giCursorVert->line().x1()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());

        QString freqstr = QString("%1Hz").arg(m_giCursorVert->line().x1());
        if(m_giCursorVert->line().x1()>0 && gMW->m_dlgSettings->ui->cbViewsShowMusicNoteNames->isChecked())
            freqstr += "("+qae::h2n(qae::f2h(m_giCursorVert->line().x1()))+")";
        m_giCursorPositionXTxt->setText(freqstr);
        m_giCursorPositionXTxt->setPos(x, viewrect.top()-2/trans.m22());

        m_giCursorPositionYTxt->setText(QString("%1dB").arg(-m_giCursorHoriz->line().y1()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), m_giCursorHoriz->line().y1()-br.height()/trans.m22());
    }

    if(forwardsync){
        if(gMW->m_gvSpectrogram)
            gMW->m_gvSpectrogram->setMouseCursorPosition(QPointF(gMW->m_gvSpectrogram->m_giMouseCursorLineTime->x(), -p.x()), false);
        if(gMW->m_gvSpectrumPhase)
            gMW->m_gvSpectrumPhase->setMouseCursorPosition(QPointF(p.x(), 0.0), false);
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->setMouseCursorPosition(QPointF(p.x(), 0.0), false);
    }
}

void GVSpectrumAmplitude::drawBackground(QPainter* painter, const QRectF& rect){
//    COUTD << "QGVAmplitudeSpectrum::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    double fs = gFL->getFs();

    // Draw the f0 and its harmonics
    FTFZero* curfzero = gFL->getCurrentFTFZero(true);
    for(size_t fi=0; fi<gFL->ftfzeros.size(); fi++)
        if(gFL->ftfzeros[fi]!=curfzero)
            gFL->ftfzeros[fi]->draw_freq_amp(painter, rect);
    if(curfzero)
        curfzero->draw_freq_amp(painter, rect);

    // Draw the filter response TODO Move to QAEGIUniformlySampledSignal
    if(m_filterresponse.size()>0) {
        QPen outlinePen(QColor(255, 192, 192));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        int dftlen = (int(m_filterresponse.size())-1)*2; // The dftlen of the filter response is a fixed one ! It is not the same as the other spectra
        int kmin = std::max(0, int(dftlen*rect.left()/fs));
        int kmax = std::min(dftlen/2, int(1+dftlen*rect.right()/fs));

        double prevx = fs*kmin/dftlen;
        double prevy = m_filterresponse[kmin];
        for(int k=kmin+1; k<=kmax; ++k){
            double x = fs*k/dftlen;
            double y = m_filterresponse[k];
            if(y<-1000000) y=-1000000;
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }

//    COUTD << "QGVAmplitudeSpectrum::~drawBackground" << endl;
}

GVSpectrumAmplitude::~GVSpectrumAmplitude(){
    m_fftresizethread->m_mutex_resizing.lock();
    m_fftresizethread->m_mutex_resizing.unlock();
    m_fftresizethread->m_mutex_changingsizes.lock();
    m_fftresizethread->m_mutex_changingsizes.unlock();
    m_fftresizethread->wait();
    delete m_fftresizethread;
    delete m_fft;
    delete m_dlgSettings;
}
