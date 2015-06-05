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

#include "gvspectrogram.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"
#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"

#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"
#include "gvwaveform.h"
#include "stftcomputethread.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "ftsound.h"
#include "ftlabels.h"
#include "ftfzero.h"
#include "sigproc.h"

#include <iostream>
#include <algorithm>
#include <cmath>
using namespace std;

#include <QMessageBox>
#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QToolTip>
#include <QScrollBar>
#include "../external/libqxt/qxtspanslider.h"

#include "qthelper.h"

QGVSpectrogram::QGVSpectrogram(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_imgSTFT(1, 1, QImage::Format_RGB32)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVSpectrogramWDialogSettings(this);
    gMW->m_qxtSpectrogramSpanSlider->setUpperValue(gMW->m_settings.value("m_qxtSpectrogramSpanSlider_upper", 90).toInt());
    gMW->m_qxtSpectrogramSpanSlider->setLowerValue(gMW->m_settings.value("m_qxtSpectrogramSpanSlider_lower", 10).toInt());

    m_imgSTFT.fill(Qt::white);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the Spectrogram view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_gridFontPen.setColor(QColor(0,0,0));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPointSize(8);
    m_gridFont.setFamily("Helvetica");

    m_aSpectrogramShowGrid = new QAction(tr("Show &grid"), this);
    m_aSpectrogramShowGrid->setObjectName("m_aSpectrogramShowGrid"); // For auto settings
    m_aSpectrogramShowGrid->setStatusTip(tr("Show &grid"));
    m_aSpectrogramShowGrid->setCheckable(true);
    m_aSpectrogramShowGrid->setChecked(false);
    m_aSpectrogramShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aSpectrogramShowGrid);
    connect(m_aSpectrogramShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(update()));

    m_aSpectrogramShowHarmonics = new QAction(tr("Show &Harmonics"), this);
    m_aSpectrogramShowHarmonics->setObjectName("m_aSpectrogramShowHarmonics"); // For auto settings
    m_aSpectrogramShowHarmonics->setStatusTip(tr("Show the harmonics of the fundamental frequency curves"));
    m_aSpectrogramShowHarmonics->setCheckable(true);
    m_aSpectrogramShowHarmonics->setChecked(false);
//    m_aSpectrogramShowHarmonics->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aSpectrogramShowHarmonics);
    connect(m_aSpectrogramShowHarmonics, SIGNAL(toggled(bool)), m_scene, SLOT(update()));


    m_stftcomputethread = new STFTComputeThread(this);

    // Cursor
    m_giMouseCursorLineTime = new QGraphicsLineItem(0, 0, 1, 1);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giMouseCursorLineTime->setPen(cursorPen);
    m_giMouseCursorLineTime->hide();
    m_scene->addItem(m_giMouseCursorLineTime);
    m_giMouseCursorLineFreq = new QGraphicsLineItem(0, 0, 1, 1);
    m_giMouseCursorLineFreq->setPen(cursorPen);
    m_giMouseCursorLineFreq->hide();
    m_scene->addItem(m_giMouseCursorLineFreq);
    QFont font("Helvetica", 10);
    m_giMouseCursorTxtTime = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtTime->setBrush(QColor(64, 64, 64));
    m_giMouseCursorTxtTime->setFont(font);
    m_giMouseCursorTxtTime->hide();
    m_scene->addItem(m_giMouseCursorTxtTime);
    m_giMouseCursorTxtFreq = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtFreq->setBrush(QColor(64, 64, 64));
    m_giMouseCursorTxtFreq->setFont(font);
    m_giMouseCursorTxtFreq->hide();
    m_scene->addItem(m_giMouseCursorTxtFreq);

    // Play Cursor
    m_giPlayCursor = new QGraphicsPathItem();
    QPen playCursorPen(QColor(255, 0, 0));
    playCursorPen.setWidth(0);
    m_giPlayCursor->setPen(playCursorPen);
    m_giPlayCursor->setBrush(QBrush(QColor(255, 0, 0)));
    QPainterPath path;
    path.moveTo(QPointF(0, 1000000.0));
    path.lineTo(QPointF(0, 0.0));
    path.lineTo(QPointF(1, 0.0));
    path.lineTo(QPointF(1, 1000000.0));
    m_giPlayCursor->setPath(path);
    playCursorSet(0.0, false);
    m_scene->addItem(m_giPlayCursor);

    // Selection
    m_currentAction = CANothing;
    m_giShownSelection = new QGraphicsRectItem();
    m_giShownSelection->hide();
    m_scene->addItem(m_giShownSelection);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giShownSelection->setPen(selectionPen);
    m_giShownSelection->setBrush(selectionBrush);
    m_giShownSelection->setOpacity(0.5);
    gMW->ui->lblSpectrogramSelectionTxt->setText("No selection");

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

    gMW->ui->lblSpectrogramSelectionTxt->setText("No selection");

    showScrollBars(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked());
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), this, SLOT(showScrollBars(bool)));
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    gMW->ui->pbSTFTComputingCancel->hide();
    gMW->ui->pgbSpectrogramSTFTCompute->hide();
    gMW->ui->pgbSpectrogramSTFTCompute->setMaximum(100);
    gMW->ui->lblSpectrogramInfoTxt->setText("");

    connect(m_stftcomputethread, SIGNAL(stftComputingStateChanged(int)), this, SLOT(stftComputingStateChanged(int)));
    connect(m_stftcomputethread, SIGNAL(stftProgressing(int)), gMW->ui->pgbSpectrogramSTFTCompute, SLOT(setValue(int)));

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aShowProperties);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(),gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lSpectrogramToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aSpectrogramShowGrid);
    m_contextmenu.addAction(m_aSpectrogramShowHarmonics);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(updateSTFTSettings()));
    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), m_dlgSettings, SLOT(close()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(updateSTFTSettings()));

    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(sliderPressed()), SLOT(amplitudeExtentSlidersChanged()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(spanChanged(int,int)), SLOT(amplitudeExtentSlidersChanged()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(lowerValueChanged(int)), this, SLOT(updateSTFTPlot()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(upperValueChanged(int)), this, SLOT(updateSTFTPlot()));

    updateSTFTSettings(); // Prepare a window from loaded settings
}

void QGVSpectrogram::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void QGVSpectrogram::amplitudeExtentSlidersChanged(){
    if(!gMW->isLoading())
        QToolTip::showText(QCursor::pos(), QString("[%1,%2]\%").arg(gMW->m_qxtSpectrogramSpanSlider->lowerValue()).arg(gMW->m_qxtSpectrogramSpanSlider->upperValue()), this);

    updateSTFTPlot();
}

void QGVSpectrogram::updateSTFTSettings(){
//    cout << "QGVSpectrogram::updateSTFTSettings fs=" << gMW->getFs() << endl;

    gMW->ui->pbSpectrogramSTFTUpdate->hide();
    m_dlgSettings->checkImageSize();

    int winlen = std::floor(0.5+gMW->getFs()*m_dlgSettings->ui->sbSpectrogramWindowSize->value());
    //    cout << "QGVSpectrogram::updateSTFTSettings winlen=" << winlen << endl;

    if(winlen%2==0 && m_dlgSettings->ui->cbSpectrogramWindowSizeForcedOdd->isChecked())
        winlen++;

    // Create the window
    int wintype = m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex();
    if(wintype==0)
        m_win = sigproc::rectangular(winlen);
    else if(wintype==1)
        m_win = sigproc::hamming(winlen);
    else if(wintype==2)
        m_win = sigproc::hann(winlen);
    else if(wintype==3)
        m_win = sigproc::blackman(winlen);
    else if(wintype==4)
        m_win = sigproc::blackmannutall(winlen);
    else if(wintype==5)
        m_win = sigproc::blackmanharris(winlen);
    else if(wintype==6)
        m_win = sigproc::nutall(winlen);
    else if(wintype==7)
        m_win = sigproc::flattop(winlen);
    else if(wintype==8)
        m_win = sigproc::normwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowNormSigma->value());
    else if(wintype==9)
        m_win = sigproc::expwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowExpDecay->value());
    else if(wintype==10)
        m_win = sigproc::gennormwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowNormSigma->value(), m_dlgSettings->ui->spSpectrogramWindowNormPower->value());
    else
        throw QString("No window selected");

//    assert(m_win.size()==winlen);

    // Normalize the window energy to sum=1
    double winsum = 0.0;
    for(size_t n=0; n<m_win.size(); ++n)
        winsum += m_win[n];
    for(size_t n=0; n<m_win.size(); ++n)
        m_win[n] /= winsum;

    updateSTFTPlot();

//    cout << "QGVSpectrogram::~updateSTFTSettings" << endl;
}


//template<typename streamtype>
//streamtype& operator<<(streamtype& stream, const QGVSpectrogram::ImageParameters& imgparams){
//    stream << imgparams.stftparams.snd << " win.size=" << imgparams.stftparams.win.size() << " stepsize=" << imgparams.stftparams.stepsize << " dftlen=" << imgparams.stftparams.dftlen << " amp=[" << imgparams.amplitudeMin << ", " << imgparams.amplitudeMax << "]";
//}


void QGVSpectrogram::stftComputingStateChanged(int state){
    if(state==STFTComputeThread::SCSDFT){
//        COUTD << "SCSDFT" << endl;
        gMW->ui->pgbSpectrogramSTFTCompute->setValue(0);
        gMW->ui->pgbSpectrogramSTFTCompute->show();
        gMW->ui->pbSTFTComputingCancel->show();
        gMW->ui->pbSpectrogramSTFTUpdate->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("Computing STFT"));
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_progresswidgets_lastup = QTime::currentTime();
        QTimer::singleShot(250, this, SLOT(showProgressWidgets()));
    }
    else if(state==STFTComputeThread::SCSIMG){
//        COUTD << "SCSIMG" << endl;
        gMW->ui->pgbSpectrogramSTFTCompute->setValue(0);
        gMW->ui->pgbSpectrogramSTFTCompute->show();
        gMW->ui->pbSTFTComputingCancel->show();
        gMW->ui->pbSpectrogramSTFTUpdate->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("Updating Image"));
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_progresswidgets_lastup = QTime::currentTime();
        QTimer::singleShot(250, this, SLOT(showProgressWidgets()));
    }
    else if(state==STFTComputeThread::SCSFinished){
//        COUTD << "SCSFinished" << endl;
        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->hide();
        gMW->ui->pgbSpectrogramSTFTCompute->hide();
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_imgSTFTParams = m_stftcomputethread->m_params_last;
//        COUTD << m_imgSTFTParams.stftparams.dftlen << endl;
//        gMW->ui->lblSpectrogramInfoTxt->setText(" ");
//        gMW->ui->lblSpectrogramInfoTxt->setText(QString("STFT: size %1, %2s step").arg(m_imgSTFTParams.stftparams.dftlen).arg(m_imgSTFTParams.stftparams.stepsize/gMW->getFs()));
        m_scene->update();
        if(gMW->m_gvWaveform->m_aWaveformShowSTFTWindowCenters->isChecked())
            gMW->m_gvWaveform->update();
    }
    else if(state==STFTComputeThread::SCSCanceled){
//        COUTD << "SCSCanceled" << endl;
        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->hide();
        gMW->ui->pgbSpectrogramSTFTCompute->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("STFT Canceled"));
        clearSTFTPlot();
        gMW->ui->pbSpectrogramSTFTUpdate->show();
        gMW->ui->wSpectrogramProgressWidgets->show();
    }
    else if(state==STFTComputeThread::SCSMemoryFull){
//        COUTD << "SCSMemoryFull" << endl;
        QMessageBox::critical(NULL, "Memory full!", "There is not enough free memory for computing this STFT");
    }
//    COUTD << "QGVSpectrogram::~stftComputingStateChanged" << endl;
}

void QGVSpectrogram::showProgressWidgets() {
//    COUTD << "QGVSpectrogram::showProgressWidgets " << QTime::currentTime().msecsSinceStartOfDay() << " " << m_progresswidgets_lastup.msecsSinceStartOfDay() << " " << QTime::currentTime().msecsSinceStartOfDay()-m_progresswidgets_lastup.msecsSinceStartOfDay() << endl;
    if(m_progresswidgets_lastup.msecsTo(QTime::currentTime())>=250 && m_stftcomputethread->isComputing())
        gMW->ui->wSpectrogramProgressWidgets->show();
}

void QGVSpectrogram::clearSTFTPlot(){
//    cout << "QGVSpectrogram::clearSTFTPlot" << endl;
    m_imgSTFTParams.clear();
    m_imgSTFT.fill(Qt::white);
    m_scene->update();
}

void QGVSpectrogram::updateSTFTPlot(bool force){
//    COUTD << "QGVSpectrogram::updateSTFTPlot" << endl;

    if(!gMW->ui->actionShowSpectrogram->isChecked())
        return;

    // Fix limits between min and max sliders
    FTSound* csnd = gMW->getCurrentFTSound(true);
    if(csnd){
        if(csnd->m_actionShow->isChecked()) {
    //        cout << "QGVSpectrogram::updateSTFTPlot " << csnd->fileFullPath.toLatin1().constData() << endl;

            if(force)
                m_imgSTFTParams.clear();

            int stepsize = std::floor(0.5+gMW->getFs()*m_dlgSettings->ui->sbSpectrogramStepSize->value());//[samples]
            int dftlen = pow(2, std::ceil(log2(float(m_win.size())))+m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());//[samples]
            int cepliftorder = -1;//[samples]
            if(gMW->m_gvSpectrogram->m_dlgSettings->ui->gbSpectrogramCepstralLiftering->isChecked())
                cepliftorder = gMW->m_gvSpectrogram->m_dlgSettings->ui->sbSpectrogramCepstralLifteringOrder->value();
            bool cepliftpresdc = gMW->m_gvSpectrogram->m_dlgSettings->ui->cbSpectrogramCepstralLifteringPreserveDC->isChecked();

            STFTComputeThread::STFTParameters reqSTFTParams(csnd, m_win, stepsize, dftlen, cepliftorder, cepliftpresdc);
            STFTComputeThread::ImageParameters reqImgSTFTParams(reqSTFTParams, &m_imgSTFT, m_dlgSettings->ui->cbSpectrogramColorMaps->currentIndex(), m_dlgSettings->ui->cbSpectrogramColorMapReversed->isChecked(), gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0, gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0);

            if(m_imgSTFTParams.isEmpty() || reqImgSTFTParams!=m_imgSTFTParams) {
                gMW->ui->pbSpectrogramSTFTUpdate->hide();
                m_stftcomputethread->compute(reqImgSTFTParams);
            }
        }
        m_scene->update();
        if(gMW->m_gvWaveform->m_aWaveformShowSTFTWindowCenters->isChecked())
            gMW->m_gvWaveform->update();
    }

//    COUTD << "QGVSpectrogram::~updateSTFTPlot" << endl;
}


void QGVSpectrogram::updateSceneRect() {
    m_scene->setSceneRect(-1.0/gMW->getFs(), 0.0, gMW->getMaxDuration()+1.0/gMW->getFs(), gMW->getFs()/2);
//    updateTextsGeometry();
}

void QGVSpectrogram::allSoundsChanged(){
    if(gMW->ftsnds.size()>0)
        updateSTFTPlot();
}

void QGVSpectrogram::viewSet(QRectF viewrect, bool forwardsync) {

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

        if(viewrect.top()<=m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top()-2);
        if(viewrect.bottom()>=m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom()+2);
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        fitInView(removeHiddenMargin(this, viewrect));

        if(forwardsync){
            if(gMW->m_gvWaveform && !gMW->m_gvWaveform->viewport()->size().isEmpty()) { // && gMW->ui->actionShowWaveform->isChecked()
                QRectF currect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
                currect.setLeft(viewrect.left());
                currect.setRight(viewrect.right());
                gMW->m_gvWaveform->viewSet(currect, false);
            }
        }
    }
}

void QGVSpectrogram::resizeEvent(QResizeEvent* event){
    // Note: Resized is called for all views so better to not forward modifications
//    cout << "QGVSpectrogram::resizeEvent" << endl;

    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {

        updateSceneRect();

        if(gMW->m_gvWaveform->viewport()->rect().width()*gMW->m_gvWaveform->viewport()->rect().height()>0){
            QRectF spectrorect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(spectrorect.left());
            viewrect.setRight(spectrorect.right());
            viewrect.setTop(0);
            viewrect.setBottom(gMW->getFs()/2);
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);
    }
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    updateTextsGeometry();
    setMouseCursorPosition(QPointF(-1,0), false);
//    cout << "QGVSpectrogram::~resizeEvent" << endl;
}

void QGVSpectrogram::scrollContentsBy(int dx, int dy) {
//    cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << " QGVSpectrogram::scrollContentsBy" << endl;

    // Invalidate the necessary parts
    // Ensure the y ticks will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r); // TODO Throw away after grid is moved to items

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r); // TODO Throw away after grid is moved to items

    updateTextsGeometry();
    setMouseCursorPosition(QPointF(-1,0), false);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVSpectrogram::wheelEvent(QWheelEvent* event) {

    qreal numDegrees = (event->angleDelta() / 8).y();
    // Clip to avoid flipping (workaround of a Qt bug ?)
    if(numDegrees>90) numDegrees = 90;
    if(numDegrees<-90) numDegrees = -90;


    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

//    cout << "QGVSpectrogram::wheelEvent: " << viewrect << endl;

    if(event->modifiers().testFlag(Qt::ShiftModifier)){
        QScrollBar* sb = horizontalScrollBar();
        sb->setValue(sb->value()-numDegrees);
    }
    else if((numDegrees>0 && viewrect.width()>10.0/gMW->getFs()) || numDegrees<0) {
        double gx = double(mapToScene(event->pos()).x()-viewrect.left())/viewrect.width();
        double gy = double(mapToScene(event->pos()).y()-viewrect.top())/viewrect.height();
        QRectF newrect = mapToScene(viewport()->rect()).boundingRect();
        newrect.setLeft(newrect.left()+gx*0.01*viewrect.width()*numDegrees);
        newrect.setRight(newrect.right()-(1-gx)*0.01*viewrect.width()*numDegrees);
        newrect.setTop(newrect.top()+gy*0.01*viewrect.height()*numDegrees);
        newrect.setBottom(newrect.bottom()-(1-gy)*0.01*viewrect.height()*numDegrees);
        if(newrect.width()<10.0/gMW->getFs()){
           newrect.setLeft(newrect.center().x()-5.0/gMW->getFs());
           newrect.setRight(newrect.center().x()+5.0/gMW->getFs());
        }

        viewSet(newrect);
    }

    QPointF p = mapToScene(event->pos());
    setMouseCursorPosition(p, true);
}

void QGVSpectrogram::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_giShownSelection->boundingRect()).boundingRect();

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
            else if((m_selection.width()>0 && m_selection.height()>0) && (event->modifiers().testFlag(Qt::ControlModifier) || (event->x()>=selview.left() && event->x()<=selview.right() && event->y()>=selview.top() && event->y()<=selview.bottom()))){
                // When scroling the selection
                m_currentAction = CAMovingSelection;
                m_topismax = m_mouseSelection.top()<=0.0;
                m_bottomismin = m_mouseSelection.bottom()>=gMW->getFs()/2.0;
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
                selectionSet(m_mouseSelection);
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
        else {
            QPoint posglobal = mapToGlobal(mapFromScene(p)+QPoint(0,0));
            m_contextmenu.exec(posglobal);
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVSpectrogram::mouseMoveEvent(QMouseEvent* event){
//    COUTD << "QGVSpectrogram::mouseMoveEvent" << endl;

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p, true);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
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

        updateTextsGeometry();

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
        if(m_topismax) m_mouseSelection.setTop(0.0);
        if(m_bottomismin) m_mouseSelection.setBottom(gMW->getFs()/2.0);
        selectionSet(m_mouseSelection, true);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionSet(m_mouseSelection, true);
    }
    else{
        QRect selview = mapFromScene(m_giShownSelection->boundingRect()).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.left()-event->x())<5
                        && event->y()>=selview.top() && event->y()<=selview.bottom())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.right()-event->x())<5
                        && selview.top()<=event->y() && selview.bottom()>=event->y())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.top()-event->y())<5
                        && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.bottom()-event->y())<5
                        && event->x()>=selview.left() && event->x()<=selview.right()) // If inside the time interv
                    setCursor(Qt::SplitVCursor);
                else if(event->x()>=selview.left() && event->x()<=selview.right() && event->y()>=selview.top() && event->y()<=selview.bottom())
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

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
    QGraphicsView::mouseMoveEvent(event);
}

void QGVSpectrogram::mouseReleaseEvent(QMouseEvent* event) {
//    std::cout << "QGVWaveform::mouseReleaseEvent " << selection.width() << endl;

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
            else{
                setCursor(Qt::CrossCursor);
            }
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

void QGVSpectrogram::keyPressEvent(QKeyEvent* event){

//    cout << "QGVSpectrogram::keyPressEvent " << endl;

    if(event->key()==Qt::Key_S){
        selectionZoomOn();
    }

    QGraphicsView::keyPressEvent(event);
}

void QGVSpectrogram::selectionClear(bool forwardsync) {
    m_giShownSelection->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    selectionSetTextInForm();

    if(forwardsync){
        if(gMW->m_gvWaveform)      gMW->m_gvWaveform->selectionClear(false);
        if(gMW->m_gvAmplitudeSpectrum)      gMW->m_gvAmplitudeSpectrum->selectionClear(false);
        if(gMW->m_gvPhaseSpectrum) gMW->m_gvPhaseSpectrum->selectionClear(false);
    }
}

void QGVSpectrogram::selectionSetTextInForm() {

    QString str;

//    cout << "QGVSpectrogram::selectionSetText: " << m_selection.height() << " " << gMW->m_gvPhaseSpectrum->m_selection.height() << endl;

//    if (m_selection.height()==0 && gMW->m_gvPhaseSpectrum->m_selection.height()==0) {
    if (m_selection.height()==0 && m_selection.width()==0) {
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
            left = gMW->m_gvPhaseSpectrum->m_selection.left();
            right = gMW->m_gvPhaseSpectrum->m_selection.right();
        }
        str += QString("[%1,%2]%3s").arg(left, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(right, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(right-left, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value());

        if (m_selection.height()>0) {
            // TODO The two lines below cannot be avoided exept by reversing the y coordinate of the
            //      whole seen, and I don't know how to do this :(
            double lower = gMW->getFs()/2-m_selection.bottom();
            if(std::abs(lower)<1e-10) lower=0.0;
            str += QString(" x [%4,%5]%6Hz").arg(lower).arg(gMW->getFs()/2-m_selection.top()).arg(m_selection.height());
        }
    }

    gMW->ui->lblSpectrogramSelectionTxt->setText(str);
}

void QGVSpectrogram::selectionSet(QRectF selection, bool forwardsync) {
    double fs = gMW->getFs();

    // Order the selection to avoid negative width and negative height
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }
    if(selection.top()>selection.bottom()){
        float tmp = selection.top();
        selection.setTop(selection.bottom());
        selection.setBottom(tmp);
    }

    m_selection = m_mouseSelection = selection;

    if(m_selection.left()<0) m_selection.setLeft(0);
    if(m_selection.right()>gMW->getFs()/2.0) m_selection.setRight(gMW->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    QGVWaveform::fixTimeLimitsToSamples(m_selection, m_mouseSelection, m_currentAction);

    m_giShownSelection->setRect(m_selection.left()-0.5/fs, m_selection.top(), m_selection.width()+1.0/fs, m_selection.height());
    m_giShownSelection->show();

    updateTextsGeometry();

    selectionSetTextInForm();

    playCursorSet(m_selection.left(), true); // Put the play cursor

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());

    if(forwardsync){
        if(gMW->m_gvWaveform) {
            if(gMW->m_gvWaveform->m_selection.left()!=m_selection.left()
               || gMW->m_gvWaveform->m_selection.right()!=m_selection.right()) {
                gMW->m_gvWaveform->selectionSet(m_selection, false);
            }
        }

        if(gMW->m_gvAmplitudeSpectrum){
            if(m_selection.height()>=gMW->getFs()/2){
                gMW->m_gvAmplitudeSpectrum->selectionClear();
            }
            else{
                QRectF rect = gMW->m_gvAmplitudeSpectrum->m_mouseSelection;
                rect.setLeft(gMW->getFs()/2-m_selection.bottom());
                rect.setRight(gMW->getFs()/2-m_selection.top());
                if(rect.height()==0){
                    rect.setTop(gMW->m_gvAmplitudeSpectrum->m_scene->sceneRect().top());
                    rect.setBottom(gMW->m_gvAmplitudeSpectrum->m_scene->sceneRect().bottom());
                }
                gMW->m_gvAmplitudeSpectrum->selectionSet(rect, false);
            }
        }
        if(gMW->m_gvPhaseSpectrum){
            if(m_selection.height()>=gMW->getFs()/2){
                gMW->m_gvPhaseSpectrum->selectionClear();
            }
            else{
                QRectF rect = gMW->m_gvPhaseSpectrum->m_mouseSelection;
                rect.setLeft(gMW->getFs()/2-m_selection.bottom());
                rect.setRight(gMW->getFs()/2-m_selection.top());
                if(rect.height()==0){
                    rect.setTop(gMW->m_gvPhaseSpectrum->m_scene->sceneRect().top());
                    rect.setBottom(gMW->m_gvPhaseSpectrum->m_scene->sceneRect().bottom());
                }
                gMW->m_gvPhaseSpectrum->selectionSet(rect, false);
            }
        }
    }

    selectionSetTextInForm();
}

void QGVSpectrogram::updateTextsGeometry() {
    QTransform trans = transform();
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(), 1.0/trans.m22());

    // Mouse Cursor
    m_giMouseCursorTxtTime->setTransform(txttrans);
    m_giMouseCursorTxtFreq->setTransform(txttrans);

    // Play cursor
    m_giPlayCursor->setTransform(QTransform::fromScale(1.0/trans.m11(), 1.0));

    // Labels
    for(size_t fi=0; fi<gMW->ftlabels.size(); fi++){
        if(!gMW->ftlabels[fi]->m_actionShow->isChecked())
            continue;

        gMW->ftlabels[fi]->updateTextsGeometry();
    }
}

void QGVSpectrogram::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
            zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
            zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
            zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        }
        viewSet(zoomonrect);

        updateTextsGeometry();
//        if(gMW->m_gvPhaseSpectrum)
//            gMW->m_gvPhaseSpectrum->viewUpdateTexts();
//        if(gMW->m_gvAmplitudeSpectrum)
//            gMW->m_gvAmplitudeSpectrum->viewUpdateTexts();

        setMouseCursorPosition(QPointF(-1,0), false);
//        m_aZoomOnSelection->setEnabled(false);
    }
}

void QGVSpectrogram::azoomin(){
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
void QGVSpectrogram::azoomout(){
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

void QGVSpectrogram::setMouseCursorPosition(QPointF p, bool forwardsync) {
    if(p.x()==-1){
        m_giMouseCursorLineFreq->hide();
        m_giMouseCursorLineTime->hide();
        m_giMouseCursorTxtTime->hide();
        m_giMouseCursorTxtFreq->hide();
    }
    else {

        m_giMouseCursorLineTime->setPos(p.x(), 0.0);
        m_giMouseCursorLineFreq->setPos(0.0, p.y());

        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());

        m_giMouseCursorLineTime->setLine(0.0, viewrect.top(), 0.0, viewrect.top()+18/trans.m22());
        m_giMouseCursorLineTime->show();
        m_giMouseCursorLineFreq->setLine(viewrect.right()-50/trans.m11(), 0.0, gMW->getFs()/2.0, 0.0);
        m_giMouseCursorLineFreq->show();

        QRectF br = m_giMouseCursorTxtTime->boundingRect();
        qreal x = p.x()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());
        m_giMouseCursorTxtTime->setText(QString("%1s").arg(p.x(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()));
        m_giMouseCursorTxtTime->setPos(x, viewrect.top()-3/trans.m22());
        m_giMouseCursorTxtTime->setTransform(txttrans);
        m_giMouseCursorTxtTime->show();

        m_giMouseCursorTxtFreq->setText(QString("%1Hz").arg(0.5*gMW->getFs()-p.y()));
        br = m_giMouseCursorTxtFreq->boundingRect();
        m_giMouseCursorTxtFreq->setPos(viewrect.right()-br.width()/trans.m11(), p.y()-br.height()/trans.m22());
        m_giMouseCursorTxtFreq->setTransform(txttrans);
        m_giMouseCursorTxtFreq->show();

        if(forwardsync){
            if(gMW->m_gvWaveform)
                gMW->m_gvWaveform->setMouseCursorPosition(p.x(), false);
            if(gMW->m_gvAmplitudeSpectrum)
                gMW->m_gvAmplitudeSpectrum->setMouseCursorPosition(QPointF(0.5*gMW->getFs()-p.y(), 0.0), false);
            if(gMW->m_gvPhaseSpectrum)
                gMW->m_gvPhaseSpectrum->setMouseCursorPosition(QPointF(0.5*gMW->getFs()-p.y(), 0.0), false);
        }
    }
}

void QGVSpectrogram::playCursorSet(double t, bool forwardsync){
    if(t==-1){
        if(m_selection.width()>0)
            playCursorSet(m_selection.left(), forwardsync);
        else
            m_giPlayCursor->setPos(QPointF(gMW->m_gvWaveform->m_initialPlayPosition, 0));
    }
    else{
        m_giPlayCursor->setPos(QPointF(t, 0));
    }

    if(gMW->m_gvWaveform && forwardsync)
        gMW->m_gvWaveform->playCursorSet(t, false);
}

void QGVSpectrogram::drawBackground(QPainter* painter, const QRectF& rect){

//    cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVSpectrogram::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    double fs = gMW->getFs();

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    // Draw the sound's spectrogram
    FTSound* csnd = gMW->getCurrentFTSound(true);
    if(csnd && csnd->m_actionShow->isChecked() && csnd->m_stftts.size()>0) {

//        double bin2hz = fs*1/csnd->m_stftparams.dftlen;

        // Build the piece of STFT which will be drawn in the view
        QRectF srcrect;
        double stftwidth = csnd->m_stftts.back()-csnd->m_stftts.front();
        srcrect.setLeft(0.5+(m_imgSTFT.rect().width()-1)*(viewrect.left()-csnd->m_stftts.front())/stftwidth);
        srcrect.setRight(0.5+(m_imgSTFT.rect().width()-1)*(viewrect.right()-csnd->m_stftts.front())/stftwidth);
//        COUTD << "viewrect=" << viewrect << endl;
//        COUTD << "IMG: " << m_imgSTFT.rect() << endl;
        // This one is vertically super sync,
        // but the cursor falls always on the top of the line, not in the middle of it.
        srcrect.setTop((m_imgSTFT.rect().height()-1)*viewrect.top()/m_scene->sceneRect().height());
        srcrect.setBottom((m_imgSTFT.rect().height()-1)*viewrect.bottom()/m_scene->sceneRect().height());
        QRectF trgrect = viewrect;
        srcrect.setTop(srcrect.top()+0.5);
        srcrect.setBottom(srcrect.bottom()+0.5);

        // This one is the basic synchronized version,
        // but it creates flickering when zooming
//        QRectF srcrect = m_imgSTFT.rect();
//        QRectF trgrect = m_scene->sceneRect();
//        trgrect.setLeft(csnd->m_stftts.front()-0.5*csnd->m_stftparams.stepsize/csnd->fs);
//        trgrect.setRight(csnd->m_stftts.back()+0.5*csnd->m_stftparams.stepsize/csnd->fs);
//        double bin2hz = fs*1/csnd->m_stftparams.dftlen;
//        trgrect.setTop(-bin2hz/2);// Hard to verify because of the flickering
//        trgrect.setBottom(fs/2+bin2hz/2);// Hard to verify because of the flickering

//        COUTD << "Scene: " << m_scene->sceneRect() << endl;
//        COUTD << "SRC: " << srcrect << endl;
//        COUTD << "TRG: " << trgrect << endl;

//        if(!m_stftcomputethread->isComputing())
        if(!m_imgSTFT.isNull() && m_stftcomputethread->m_mutex_imageallocation.tryLock()) {
            painter->drawImage(trgrect, m_imgSTFT, srcrect);
            m_stftcomputethread->m_mutex_imageallocation.unlock();
        }
    }

    // Draw the grid above the spectrogram
    if(m_aSpectrogramShowGrid->isChecked())
        draw_grid(painter, rect);

    // Draw the f0 grids
    if(!gMW->ftfzeros.empty()) {
        for(size_t fi=0; fi<gMW->ftfzeros.size(); fi++){
            if(!gMW->ftfzeros[fi]->m_actionShow->isChecked() && gMW->ftfzeros[fi]->ts.size()>1)
                continue;

            // Draw the f0
            QColor c = gMW->ftfzeros[fi]->color;
            c.setAlphaF(1.0);
            QPen outlinePen(c);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            double f0min = fs/2;
            for(int ti=0; ti<int(gMW->ftfzeros[fi]->ts.size())-1; ++ti){
                if(gMW->ftfzeros[fi]->f0s[ti]>0.0)
                    f0min = std::min(f0min, gMW->ftfzeros[fi]->f0s[ti]);
                painter->drawLine(QLineF(gMW->ftfzeros[fi]->ts[ti], fs/2-gMW->ftfzeros[fi]->f0s[ti], gMW->ftfzeros[fi]->ts[ti+1], fs/2-gMW->ftfzeros[fi]->f0s[ti+1]));
            }
            if(gMW->ftfzeros[fi]->f0s.back()>0.0)
                f0min = std::min(f0min, gMW->ftfzeros[fi]->f0s.back());

            if(m_aSpectrogramShowHarmonics->isChecked()){
                // Draw harmonics up to Nyquist
                c.setAlphaF(0.5);
                outlinePen.setColor(c);
                painter->setPen(outlinePen);
                for(int h=2; h<int(0.5*fs/f0min)+1; h++)
                    for(int ti=0; ti<int(gMW->ftfzeros[fi]->ts.size())-1; ++ti)
                        painter->drawLine(QLineF(gMW->ftfzeros[fi]->ts[ti], fs/2-h*gMW->ftfzeros[fi]->f0s[ti], gMW->ftfzeros[fi]->ts[ti+1], fs/2-h*gMW->ftfzeros[fi]->f0s[ti+1]));
            }
        }
    }

//    cout << "QGVSpectrogram::~drawBackground" << endl;
}

void QGVSpectrogram::draw_grid(QPainter* painter, const QRectF& rect) {
    // Prepare the pens and fonts
    QPen gridPen(QColor(0,0,0));
    gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    painter->setFont(m_gridFont);

    // Horizontal lines

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    // Adapt the lines ordinates to the viewport
    double f = log10(float(viewrect.height()));
    int fi;
    if(f<0) fi=int(f-1);
    else fi = int(f);
    double lstep = pow(10.0, fi);
    int m=1;
    while(int(viewrect.height()/lstep)<3){
        lstep /= 2;
        m++;
    }
    // cout << "lstep=" << lstep << endl;

    // Draw the horizontal lines
    int mn=0;
    painter->setPen(gridPen);
    for(double l=gMW->getFs()/2.0-int((gMW->getFs()/2.0-rect.bottom())/lstep)*lstep; l>=rect.top(); l-=lstep){
        painter->drawLine(QLineF(rect.left(), l, rect.right(), l));
        mn++;
    }

    // Write the ordinates of the horizontal lines
    painter->setPen(m_gridFontPen);
    QTransform trans = transform();
    for(double l=gMW->getFs()/2.0-int((gMW->getFs()/2.0-rect.bottom())/lstep)*lstep; l>=rect.top(); l-=lstep){
        painter->save();
        painter->translate(QPointF(viewrect.left(), l));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());

        QString txt = QString("%1Hz").arg(gMW->getFs()/2.0-l);
        QRectF txtrect = painter->boundingRect(QRectF(), Qt::AlignLeft, txt);
        if(l<viewrect.bottom()-0.75*txtrect.height()/trans.m22())
            painter->drawStaticText(QPointF(0, -0.9*txtrect.height()), QStaticText(txt));
        painter->restore();
    }

    // Vertical lines

    // Adapt the lines absissa to the viewport
    f = std::log10(float(viewrect.width()));
    if(f<0) fi=int(f-1);
    else fi = int(f);
//    std::cout << viewrect.height() << " " << f << " " << fi << endl;
    lstep = pow(10.0, fi);
    m=1;
    while(int(viewrect.width()/lstep)<6){
        lstep /= 2;
        m++;
    }
//    std::cout << "lstep=" << lstep << endl;

    // Draw the vertical lines
    mn = 0;
    painter->setPen(gridPen);
    for(double l=int(rect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->drawLine(QLineF(l, rect.top(), l, rect.bottom()));
        mn++;
    }

    // Write the absissa of the vertical lines
    painter->setPen(m_gridFontPen);
    for(double l=int(rect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->save();
        painter->translate(QPointF(l, viewrect.bottom()-14/trans.m22()));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1s").arg(l, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())));
        painter->restore();
    }
}

QGVSpectrogram::~QGVSpectrogram(){
    m_stftcomputethread->m_mutex_computing.lock();
    m_stftcomputethread->m_mutex_computing.unlock();
    m_stftcomputethread->m_mutex_changingparams.lock();
    m_stftcomputethread->m_mutex_changingparams.unlock();
    delete m_stftcomputethread;
    delete m_dlgSettings;
}
