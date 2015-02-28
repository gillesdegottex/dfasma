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
using namespace std;

#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QToolTip>

#include "qthelper.h"

QGVSpectrogram::QGVSpectrogram(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_imgSTFT(1, 1, QImage::Format_RGB32)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVSpectrogramWDialogSettings(this);

    QSettings settings;
    gMW->ui->sldSpectrogramAmplitudeMin->setValue(-settings.value("qgvspectrogram/sldSpectrogramAmplitudeMin", -250).toInt());
    gMW->ui->sldSpectrogramAmplitudeMax->setValue(settings.value("qgvspectrogram/sldSpectrogramAmplitudeMax", 0).toInt());

    m_imgSTFT.fill(Qt::white);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the Spectrogram view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_gridFontPen.setColor(QColor(0,0,0));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPointSize(8);
    m_gridFont.setFamily("Helvetica");

    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setStatusTip(tr("Show &grid"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aShowGrid->setChecked(settings.value("qgvspectrogram/m_aShowGrid", true).toBool());
    connect(m_aShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));

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
    m_aZoomOnSelection->setShortcut(Qt::Key_S);
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));

    gMW->ui->lblSpectrogramSelectionTxt->setText("No selection");

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    gMW->ui->pgbSpectrogramSTFTCompute->hide();
    gMW->ui->lblSpectrogramInfoTxt->setText("");

    connect(m_stftcomputethread, SIGNAL(stftComputing()), this, SLOT(stftComputing()));
    connect(m_stftcomputethread, SIGNAL(stftFinished(bool)), this, SLOT(stftFinished(bool)));
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
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbToolBarSizes->value(),gMW->m_dlgSettings->ui->sbToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lSpectrogramToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aShowGrid);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(updateDFTSettings()));
    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), m_dlgSettings, SLOT(close()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(updateDFTSettings()));

    connect(gMW->ui->sldSpectrogramAmplitudeMin, SIGNAL(valueChanged(int)), this, SLOT(updateSTFTPlot()));
    connect(gMW->ui->sldSpectrogramAmplitudeMax, SIGNAL(valueChanged(int)), this, SLOT(updateSTFTPlot()));

    updateDFTSettings(); // Prepare a window from loaded settings
}

void QGVSpectrogram::updateAmplitudeExtent(){
//    cout << "QGVSpectrogram::updateAmplitudeExtent" << endl;

    if(!gMW->isLoading())
        QToolTip::showText(QCursor::pos(), QString("[%1,%2]dB").arg(-gMW->ui->sldSpectrogramAmplitudeMin->value()).arg(gMW->ui->sldSpectrogramAmplitudeMax->value()), this);

    // If the current is NOT opaque:
    if(gMW->ftsnds.size()>0){
        double mindb = 500;
        double maxdb = -500;
        for(unsigned int si=0; si<gMW->ftsnds.size(); si++){
//            cout << "m_stft_min=" << gMW->ftsnds[si]->m_stft_min << " m_stft_max=" << gMW->ftsnds[si]->m_stft_max << endl;

            if(gMW->ftsnds[si]->m_stft.size()>0){
                mindb = std::min(mindb, gMW->ftsnds[si]->m_stft_min);
                maxdb = std::max(maxdb, gMW->ftsnds[si]->m_stft_max);
            }
        }

//        cout << "mindb=" << mindb << " maxdb=" << maxdb << endl;

        gMW->ui->sldSpectrogramAmplitudeMax->setMaximum(maxdb);
        gMW->ui->sldSpectrogramAmplitudeMax->setMinimum(mindb+1);
        gMW->ui->sldSpectrogramAmplitudeMin->setMinimum(-(maxdb-1));
        gMW->ui->sldSpectrogramAmplitudeMin->setMaximum(-(mindb));
    }

    gMW->ui->sldSpectrogramAmplitudeMax->setMinimum(-gMW->ui->sldSpectrogramAmplitudeMin->value()+1);
    gMW->ui->sldSpectrogramAmplitudeMin->setMinimum(-(gMW->ui->sldSpectrogramAmplitudeMax->value()-1));

//    cout << "QGVSpectrogram::~updateAmplitudeExtent" << endl;
}

void QGVSpectrogram::updateDFTSettings(){
//    cout << "QGVSpectrogram::updateDFTSettings fs=" << gMW->getFs() << endl;

    int winlen = std::floor(0.5+gMW->getFs()*m_dlgSettings->ui->sbWindowSize->value());
    //    cout << "QGVSpectrogram::updateDFTSettings winlen=" << winlen << endl;

    if(winlen%2==0 && m_dlgSettings->ui->cbWindowSizeForcedOdd->isChecked())
        winlen++;

    // Create the window
    int wintype = m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex();
    if(wintype==0)
        m_win = sigproc::hann(winlen);
    else if(wintype==1)
        m_win = sigproc::hamming(winlen);
    else if(wintype==2)
        m_win = sigproc::blackman(winlen);
    else if(wintype==3)
        m_win = sigproc::nutall(winlen);
    else if(wintype==4)
        m_win = sigproc::blackmannutall(winlen);
    else if(wintype==5)
        m_win = sigproc::blackmanharris(winlen);
    else if(wintype==6)
        m_win = sigproc::flattop(winlen);
    else if(wintype==7)
        m_win = sigproc::rectangular(winlen);
    else if(wintype==8)
        m_win = sigproc::normwindow(winlen, m_dlgSettings->ui->spWindowNormSigma->value());
    else if(wintype==9)
        m_win = sigproc::expwindow(winlen, m_dlgSettings->ui->spWindowExpDecay->value());
    else if(wintype==10)
        m_win = sigproc::gennormwindow(winlen, m_dlgSettings->ui->spWindowNormSigma->value(), m_dlgSettings->ui->spWindowNormPower->value());
    else
        throw QString("No window selected");

    // Normalize the window energy to sum=1
    double winsum = 0.0;
    for(int n=0; n<winlen; n++)
        winsum += m_win[n];
    for(int n=0; n<winlen; n++)
        m_win[n] /= winsum;

    updateSTFTPlot();

//    cout << "QGVSpectrogram::~updateDFTSettings" << endl;
}


void QGVSpectrogram::stftComputing(){
//    cout << "QGVSpectrogram::stftComputing" << endl;
    gMW->ui->pgbSpectrogramSTFTCompute->setMaximum(100);
    gMW->ui->pgbSpectrogramSTFTCompute->setValue(0);
    gMW->ui->pgbSpectrogramSTFTCompute->show();
    gMW->ui->lblSpectrogramInfoTxt->setText(QString("Computing STFT..."));
}


template<typename streamtype>
streamtype& operator<<(streamtype& stream, const QGVSpectrogram::ImageParameters& imgparams){
    stream << imgparams.stftparams.snd << " win.size=" << imgparams.stftparams.win.size() << " stepsize=" << imgparams.stftparams.stepsize << " dftlen=" << imgparams.stftparams.dftlen << " amp=[" << imgparams.amplitudeMin << ", " << imgparams.amplitudeMax << "]";
}

void QGVSpectrogram::clearSTFTPlot(){
    m_imgSTFTParams.clear();
    m_imgSTFT.fill(Qt::white);
    m_scene->update();
}

void QGVSpectrogram::stftFinished(bool canceled){
    if(canceled){
        gMW->ui->pbSTFTComputingCancel->hide();
        gMW->ui->pgbSpectrogramSTFTCompute->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("STFT Canceled"));
        clearSTFTPlot();
    }
    else {
        updateSTFTPlot();
    }
}

void QGVSpectrogram::updateSTFTPlot(bool force){
//    COUTD << "QGVSpectrogram::updateSTFTPlot" << endl;

    // Fix limits between min and max sliders
    FTSound* csnd = gMW->getCurrentFTSound(true);
    if(csnd && gMW->ui->actionShowSpectrogram->isChecked()) {
//        cout << "QGVSpectrogram::updateSTFTPlot " << csnd->fileFullPath.toLatin1().constData() << endl;

        if(force){
            csnd->m_stftparams.clear();
            m_imgSTFTParams.clear();
        }

        int stepsize = std::floor(0.5+gMW->getFs()*m_dlgSettings->ui->sbStepSize->value());
        int dftlen = pow(2, std::ceil(log2(float(m_win.size())))+m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());

        if(csnd->m_stftparams != STFTComputeThread::Parameters(csnd, m_win, stepsize, dftlen)){
            m_stftcomputethread->compute(csnd, m_win, stepsize, dftlen);
        }
        else{
            // Be sure to wait for stftComputed (updateSTFTPlot can be called by other means)
            if(m_stftcomputethread->m_mutex_computing.tryLock()){

                if(csnd->m_stft.size()>0){

                    updateAmplitudeExtent();

                    int dftlen = (csnd->m_stft[0].size()-1)*2;

                    ImageParameters reqImgParams(csnd->m_stftparams, gMW->ui->sldSpectrogramAmplitudeMin->value(), gMW->ui->sldSpectrogramAmplitudeMax->value());

                    if(m_imgSTFTParams!=reqImgParams){
                        gMW->ui->pgbSpectrogramSTFTCompute->hide();
                        gMW->ui->pbSTFTComputingCancel->hide();
                        gMW->ui->lblSpectrogramInfoTxt->setText(QString("Updating Image ..."));
                        QCoreApplication::processEvents();

                        m_imgSTFTParams = reqImgParams;

                        // Update the image from the sound's STFT
                        m_imgSTFT = QImage(csnd->m_stft.size(), csnd->m_stft[0].size(), QImage::Format_RGB32);

                        for(size_t si=0; si<csnd->m_stft.size(); si++){
                            for(int n=0; n<int(csnd->m_stft[si].size()); n++) {
                                double y = csnd->m_stft[si][n];

                                int color = 255;
                                if(!std::isinf(y))
                                    color = 256-(256*(y+gMW->ui->sldSpectrogramAmplitudeMin->value())/(gMW->ui->sldSpectrogramAmplitudeMax->value()+gMW->ui->sldSpectrogramAmplitudeMin->value()));

                                if(color<0) color = 0;
                                else if(color>255) color = 255;
                                m_imgSTFT.setPixel(QPoint(si,dftlen/2-n), QColor(color, color, color).rgb());
                            }
                        }
                    }

                    m_scene->update();
                    gMW->ui->lblSpectrogramInfoTxt->setText(QString("DFT size=%1").arg(dftlen));
                }

                m_stftcomputethread->m_mutex_computing.unlock();
            }
        }
    }

//    cout << "QGVSpectrogram::~updateSTFTPlot" << endl;
}

void QGVSpectrogram::updateSceneRect() {
    m_scene->setSceneRect(-1.0/gMW->getFs(), 0.0, gMW->getMaxDuration()+1.0/gMW->getFs(), gMW->getFs()/2);
}

void QGVSpectrogram::soundsChanged(){
    cout << "QGVSpectrogram::soundsChanged" << endl;
    if(gMW->ftsnds.size()>0)
        updateSTFTPlot();
    cout << "QGVSpectrogram::~soundsChanged" << endl;
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
    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r);

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r);

    updateTextsGeometry();
    setMouseCursorPosition(QPointF(-1,0), false);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVSpectrogram::wheelEvent(QWheelEvent* event) {

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

    QPointF p = mapToScene(event->pos());
    setMouseCursorPosition(p, true);
}

void QGVSpectrogram::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setDragMode(QGraphicsView::ScrollHandDrag);
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
                selectionSet(m_mouseSelection);
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
        else {
            QPoint posglobal = mapToGlobal(mapFromScene(p)+QPoint(0,0));
            m_contextmenu.exec(posglobal);
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVSpectrogram::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << selection.width() << endl;

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
        FTSound* currentftsound = gMW->getCurrentFTSound();
        if(currentftsound){
            currentftsound->m_ampscale *= pow(10, -(p.y()-m_selection_pressedp.y())/20.0);
            m_selection_pressedp = p;

            if(currentftsound->m_ampscale>1e10)
                currentftsound->m_ampscale = 1e10;
            else if(currentftsound->m_ampscale<1e-10)
                currentftsound->m_ampscale = 1e-10;

            gMW->m_gvWaveform->soundsChanged();
            soundsChanged();
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

void QGVSpectrogram::keyPressEvent(QKeyEvent* event){

//    cout << "QGVSpectrogram::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape){
        selectionClear();
        gMW->m_gvWaveform->selectionClear();
        playCursorSet(0.0, true);
    }

    QGraphicsView::keyPressEvent(event);
}

void QGVSpectrogram::selectionClear(bool forwardsync) {
    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    selectionSetTextInForm();

    if(forwardsync){
        if(gMW->m_gvWaveform)      gMW->m_gvWaveform->selectionClear(false);
        if(gMW->m_gvSpectrum)      gMW->m_gvSpectrum->selectionClear(false);
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
        str += QString("[%1,%2]%3s").arg(left).arg(right).arg(right-left);

        if (m_selection.height()>0) {
            str += QString(" x [%4,%5]%6Hz").arg(gMW->getFs()/2-m_selection.bottom()).arg(gMW->getFs()/2-m_selection.top()).arg(m_selection.height());
        }
    }

    gMW->ui->lblSpectrogramSelectionTxt->setText(str);
}

void QGVSpectrogram::selectionSet(QRectF selection, bool forwardsync) {
//    FLAG

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

    m_giShownSelection->setRect(m_selection);
    m_giShownSelection->show();

    m_giSelectionTxt->setText(QString("%1s,%2Hz").arg(m_selection.width()).arg(m_selection.height()));
    updateTextsGeometry();

    selectionSetTextInForm();

    playCursorSet(m_selection.left(), true); // Put the play cursor

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());

    if(forwardsync){
        if(gMW->m_gvWaveform)
            gMW->m_gvWaveform->selectionSet(QRectF(m_selection.left(), -1.0, m_selection.width(), 2.0), true, false);

        if(gMW->m_gvSpectrum){
            QRectF rect = gMW->m_gvSpectrum->m_mouseSelection;
            rect.setLeft(gMW->getFs()/2-m_selection.bottom());
            rect.setRight(gMW->getFs()/2-m_selection.top());
            if(rect.height()==0){
                rect.setTop(gMW->m_gvSpectrum->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrum->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrum->selectionSet(rect, false);
        }
        if(gMW->m_gvPhaseSpectrum){
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

    // Selection
    QRectF br = m_giSelectionTxt->boundingRect();
    m_giSelectionTxt->setTransform(txttrans);
    m_giSelectionTxt->setPos(m_selection.center()-QPointF(0.5*br.width()/trans.m11(), 0.5*br.height()/trans.m22()));

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
        zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
        zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
        zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
        zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        viewSet(zoomonrect);

        updateTextsGeometry();
//        if(gMW->m_gvPhaseSpectrum)
//            gMW->m_gvPhaseSpectrum->viewUpdateTexts();
//        if(gMW->m_gvSpectrum)
//            gMW->m_gvSpectrum->viewUpdateTexts();

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
        m_giMouseCursorTxtTime->setText(QString("%1s").arg(p.x()));
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
            if(gMW->m_gvSpectrum)
                gMW->m_gvSpectrum->setMouseCursorPosition(QPointF(0.5*gMW->getFs()-p.y(), 0.0), false);
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

//    double fs = gMW->getFs();

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    // Draw the f0 curve
        // TODO
    // Draw the harmonic grid curve
        // TODO
//    if(!gMW->ftfzeros.empty()) {

//        for(size_t fi=0; fi<gMW->ftfzeros.size(); fi++){
//            if(!gMW->ftfzeros[fi]->m_actionShow->isChecked())
//                continue;

//            QPen outlinePen(gMW->ftfzeros[fi]->color);
//            outlinePen.setWidth(0);
//            painter->setPen(outlinePen);
//            painter->setBrush(QBrush(gMW->ftfzeros[fi]->color));

            // TODO
//        }
//    }

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    // Draw the sound's spectrogram
    FTSound* csnd = gMW->getCurrentFTSound(true);
    if(csnd && csnd->m_actionShow->isChecked() && csnd->m_stftts.size()>0) {

        // Build the piece of STFT which will be drawn in the view
        QRectF srcrect;
        double stftwidth = csnd->m_stftts.back()-csnd->m_stftts.front();
        srcrect.setLeft(0.5+(m_imgSTFT.rect().width()-1)*(viewrect.left()-csnd->m_stftts.front())/stftwidth);
        srcrect.setRight(0.5+(m_imgSTFT.rect().width()-1)*(viewrect.right()-csnd->m_stftts.front())/stftwidth);
        srcrect.setTop(m_imgSTFT.rect().height()*viewrect.top()/m_scene->sceneRect().height());
        srcrect.setBottom(m_imgSTFT.rect().height()*viewrect.bottom()/m_scene->sceneRect().height());
        QRectF trgrect = viewrect;

        // This one is the basic time synchronized version,
        // but it creates flickering when zooming
        //QRectF srcrect = m_imgSTFT.rect();
        //QRectF trgrect = m_scene->sceneRect();
        //trgrect.setLeft(csnd->m_stftts.front()-0.5*csnd->m_stftparams.stepsize/csnd->fs);
        //trgrect.setRight(csnd->m_stftts.back()+0.5*csnd->m_stftparams.stepsize/csnd->fs);

        painter->drawImage(trgrect, m_imgSTFT, srcrect);
    }

    // Draw the grid above the spectrogram
    if(m_aShowGrid->isChecked())
        draw_grid(painter, rect);

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
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1s").arg(l)));
        painter->restore();
    }
}

void QGVSpectrogram::settingsSave() {
    QSettings settings;
    settings.setValue("qgvspectrogram/m_aShowGrid", m_aShowGrid->isChecked());
    settings.setValue("qgvspectrogram/sldSpectrogramAmplitudeMin", -gMW->ui->sldSpectrogramAmplitudeMin->value());
    settings.setValue("qgvspectrogram/sldSpectrogramAmplitudeMax", gMW->ui->sldSpectrogramAmplitudeMax->value());
    settings.setValue("qgvspectrogram/sbSpectrogramOversamplingFactor", m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());
    settings.setValue("qgvspectrogram/cbWindowSizeForcedOdd", m_dlgSettings->ui->cbWindowSizeForcedOdd->isChecked());
    settings.setValue("qgvspectrogram/cbSpectrogramWindowType", m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex());
    settings.setValue("qgvspectrogram/spWindowNormPower", m_dlgSettings->ui->spWindowNormPower->value());
    settings.setValue("qgvspectrogram/spWindowNormSigma", m_dlgSettings->ui->spWindowNormSigma->value());
    settings.setValue("qgvspectrogram/spWindowExpDecay", m_dlgSettings->ui->spWindowExpDecay->value());

    settings.setValue("qgvspectrogram/sbStepSize", m_dlgSettings->ui->sbStepSize->value());
    settings.setValue("qgvspectrogram/sbWindowSize", m_dlgSettings->ui->sbWindowSize->value());
    settings.setValue("qgvspectrogram/sbSpectrogramOversamplingFactor", m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());
}

QGVSpectrogram::~QGVSpectrogram(){
    m_stftcomputethread->m_mutex_computing.lock();
    m_stftcomputethread->m_mutex_computing.unlock();
    m_stftcomputethread->m_mutex_changingparams.lock();
    m_stftcomputethread->m_mutex_changingparams.unlock();
    delete m_stftcomputethread;
    delete m_dlgSettings;
}
