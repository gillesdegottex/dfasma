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
#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"
#include "gvwaveform.h"
#include "fftresizethread.h"
#include "../external/FFTwrapper.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "ftsound.h"
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

QGVSpectrogram::QGVSpectrogram(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_winlen(0)
    , m_dftlen(0)
    , m_imgSTFT(1, 1, QImage::Format_RGB32)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVSpectrogramWDialogSettings(this);

    QSettings settings;

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the Spectrogram view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPointSize(6);
    m_gridFont.setFamily("Helvetica");

    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setStatusTip(tr("Show &grid"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aShowGrid->setChecked(settings.value("qgvspectrogram/m_aShowGrid", true).toBool());
    connect(m_aShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));

    m_fft = new FFTwrapper();
    m_fftresizethread = new FFTResizeThread(m_fft, this);

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -1000, 0, 1000);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursorHoriz->setPen(cursorPen);
    m_giCursorHoriz->hide();
    m_scene->addItem(m_giCursorHoriz);
    m_giCursorVert = new QGraphicsLineItem(0, 0, WMainWindow::getMW()->getFs()/2.0, 0);
    m_giCursorVert->setPen(cursorPen);
    m_giCursorVert->hide();
    m_scene->addItem(m_giCursorVert);
    QFont font;
    font.setPointSize(8);
    m_giCursorPositionXTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionXTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionXTxt->setFont(font);
    m_giCursorPositionXTxt->hide();
    m_scene->addItem(m_giCursorPositionXTxt);
    m_giCursorPositionYTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionYTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionYTxt->setFont(font);
    m_giCursorPositionYTxt->hide();
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
    WMainWindow::getMW()->ui->lblSpectrogramSelectionTxt->setText("No selection");

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
    m_aUnZoom->setShortcut(Qt::Key_Z);
    m_aUnZoom->setIcon(QIcon(":/icons/unzoomxy.svg"));
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
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

    WMainWindow::getMW()->ui->lblSpectrogramSelectionTxt->setText("No selection");

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    WMainWindow::getMW()->ui->pgbSpectrogramFFTResize->hide();
    WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText("");

    connect(m_fftresizethread, SIGNAL(fftResized(int,int)), this, SLOT(computeDFTs()));
    connect(m_fftresizethread, SIGNAL(fftResizing(int,int)), this, SLOT(fftResizing(int,int)));

    m_minsy = m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value();
    m_maxsy = m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMax->value();
    updateSceneRect();

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
    m_toolBar->addAction(m_aShowProperties);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomIn);
    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    WMainWindow::getMW()->ui->lSpectrogramToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aShowGrid);
    m_contextmenu.addSeparator();
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(settingsModified()));

    updateDFTSettings();
}

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
QRectF QGVSpectrogram::removeHiddenMargin(const QRectF& sceneRect){
    const int bugMargin = 2;
    const double mx = sceneRect.width()/viewport()->size().width()*bugMargin;
    const double my = sceneRect.height()/viewport()->size().height()*bugMargin;
    return sceneRect.adjusted(mx, my, -mx, -my);
}

void QGVSpectrogram::settingsModified(){
    updateSceneRect();
    // TODO
    updateDFTSettings();
//    if(WMainWindow::getMW()->m_gvWaveform)
//        WMainWindow::getMW()->m_gvWaveform->selectionClipAndSet(WMainWindow::getMW()->m_gvWaveform->m_mouseSelection, true);
}

void QGVSpectrogram::fftResizing(int prevSize, int newSize){
    Q_UNUSED(prevSize);

    WMainWindow::getMW()->ui->pgbSpectrogramFFTResize->show();
    WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText(QString("Resizing DFT to %1").arg(newSize));
}

void QGVSpectrogram::updateDFTSettings(){

//    cout << "QGVSpectrogram::updateDFTSettings" << endl;

    updateSceneRect();

    m_winlen = std::floor(0.5+WMainWindow::getMW()->getFs()*m_dlgSettings->ui->sbWindowSize->value());

    // Create the window
    int wintype = m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex();
    if(wintype==0)
        m_win = sigproc::hann(m_winlen);
    else if(wintype==1)
        m_win = sigproc::hamming(m_winlen);
    else if(wintype==2)
        m_win = sigproc::blackman(m_winlen);
    else if(wintype==3)
        m_win = sigproc::nutall(m_winlen);
    else if(wintype==4)
        m_win = sigproc::blackmannutall(m_winlen);
    else if(wintype==5)
        m_win = sigproc::blackmanharris(m_winlen);
    else if(wintype==6)
        m_win = sigproc::flattop(m_winlen);
    else if(wintype==7)
        m_win = sigproc::rectangular(m_winlen);
    else if(wintype==8)
        m_win = sigproc::normwindow(m_winlen, m_dlgSettings->ui->spWindowNormSigma->value());
    else if(wintype==9)
        m_win = sigproc::expwindow(m_winlen, m_dlgSettings->ui->spWindowExpDecay->value());
    else if(wintype==10)
        m_win = sigproc::gennormwindow(m_winlen, m_dlgSettings->ui->spWindowNormSigma->value(), m_dlgSettings->ui->spWindowNormPower->value());
    else
        throw QString("No window selected");

    // Normalize the window energy to sum=1
    double winsum = 0.0;
    for(int n=0; n<m_winlen; n++)
        winsum += m_win[n];
    for(int n=0; n<m_winlen; n++)
        m_win[n] /= winsum;

    // Set the DFT length
    m_dftlen = pow(2, std::ceil(log2(float(m_winlen)))+m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());

//    computeDFTs();
}

void QGVSpectrogram::computeDFTs(){
    std::cout << "QGVSpectrogram::computeDFTs " << m_winlen << endl;

    if(m_winlen<2)
        return;

    m_fftresizethread->resize(m_dftlen);

    if(m_fftresizethread->m_mutex_resizing.tryLock()){

        int dftlen = m_fft->size(); // Local copy of the actual dftlen

        FTSound* csnd = WMainWindow::getMW()->getCurrentFTSound();
        if(csnd) {

            double fs = WMainWindow::getMW()->getFs();
            int stepsize = std::floor(0.5+fs*m_dlgSettings->ui->sbStepSize->value());
            m_nbsteps = std::floor((WMainWindow::getMW()->getMaxLastSampleTime()*fs - (m_winlen-1)/2)/stepsize);

            m_imgSTFT = QImage(m_nbsteps, m_dftlen/2+1, QImage::Format_RGB32);

            WMainWindow::getMW()->ui->pgbSpectrogramFFTResize->hide();
            WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText(QString("DFT size=%1").arg(dftlen));

            m_minsy = std::numeric_limits<double>::infinity();
            m_maxsy = -std::numeric_limits<double>::infinity();

            QPainter imgpaint(&m_imgSTFT);
            QPen outlinePen(QColor(255, 0, 0));
            outlinePen.setWidth(0);
            imgpaint.setPen(outlinePen);
            imgpaint.setOpacity(1);
            m_imgSTFT.fill(QColor(0,0,255));

            for(int si=0; si<m_nbsteps; si++){
                int n = 0;
                int wn = 0;
//                cout << si*stepsize/WMainWindow::getMW()->getFs() << endl;
                for(; n<m_winlen; n++){
                    wn = si*stepsize+n - csnd->m_delay;
                    if(wn>=0 && wn<int(csnd->wavtoplay->size()))
                        m_fft->in[n] = (*(csnd->wavtoplay))[wn]*m_win[n];
                    else
                        m_fft->in[n] = 0.0;
                }
                for(; n<dftlen; n++)
                    m_fft->in[n] = 0.0;

                m_fft->execute(); // Compute the DFT

                for(n=0; n<dftlen/2+1; n++) {
                    double y = std::log(std::abs(m_fft->out[n]));
                    int color = 256*(sigproc::log2db*y-m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value())/(m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMax->value()-m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value());
                    if(color<0) color = 0;
                    else if(color>255) color = 255;
                    m_imgSTFT.setPixel(QPoint(si,n), QColor(color, color, color).rgb());
                    m_minsy = std::min(m_minsy, y);
                    m_maxsy = std::max(m_maxsy, y);
                }
            }

            m_minsy = sigproc::log2db*m_minsy;
            m_maxsy = sigproc::log2db*m_maxsy;

            m_imgSTFT = m_imgSTFT.mirrored(false, true);

            m_fftresizethread->m_mutex_resizing.unlock();

            m_scene->invalidate();
        }
    }

    std::cout << "~QGVSpectrogram::computeDFTs" << endl;
}

void QGVSpectrogram::settingsSave() {
    QSettings settings;
    settings.setValue("qgvspectrogram/m_aShowGrid", m_aShowGrid->isChecked());
    settings.setValue("qgvspectrogram/sbSpectrogramAmplitudeRangeMin", m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value());
    settings.setValue("qgvspectrogram/sbSpectrogramAmplitudeRangeMax", m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMax->value());
    settings.setValue("qgvspectrogram/sbSpectrogramOversamplingFactor", m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());
    settings.setValue("qgvspectrogram/cbWindowSizeForcedOdd", m_dlgSettings->ui->cbWindowSizeForcedOdd->isChecked());
    settings.setValue("qgvspectrogram/cbSpectrogramWindowType", m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex());
    settings.setValue("qgvspectrogram/spWindowNormPower", m_dlgSettings->ui->spWindowNormPower->value());
    settings.setValue("qgvspectrogram/spWindowNormSigma", m_dlgSettings->ui->spWindowNormSigma->value());
    settings.setValue("qgvspectrogram/spWindowExpDecay", m_dlgSettings->ui->spWindowExpDecay->value());
}

void QGVSpectrogram::updateSceneRect() {
//    cout << "QGVSpectrogram::updateSceneRect" << endl;
    m_scene->setSceneRect(0.0, 0.0, WMainWindow::getMW()->getMaxLastSampleTime(), WMainWindow::getMW()->getFs()/2);

//    if(WMainWindow::getMW()->m_gvPhaseSpectrum)
//        WMainWindow::getMW()->m_gvPhaseSpectrum->updateSceneRect();
//    if(WMainWindow::getMW()->m_gvSpectrum)
//        WMainWindow::getMW()->m_gvSpectrum->updateSceneRect();
}

void QGVSpectrogram::soundsChanged(){
//    if(WMainWindow::getMW()->ftsnds.size()>0)
//        computeDFTs();
//    m_scene->update();
}

void QGVSpectrogram::viewSet(QRectF viewrect, bool sync) {
//    cout << "QGVSpectrogram::viewSet" << endl;

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

        fitInView(removeHiddenMargin(viewrect));

        if(sync) viewSync();
    }

//    cout << "QGVSpectrogram::~viewSet" << endl;
}

void QGVSpectrogram::viewSync() {

//    if(WMainWindow::getMW()->m_gvPhaseSpectrum) {
//        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

//        QRectF currect = WMainWindow::getMW()->m_gvPhaseSpectrum->mapToScene(WMainWindow::getMW()->m_gvPhaseSpectrum->viewport()->rect()).boundingRect();
//        currect.setLeft(viewrect.left());
//        currect.setRight(viewrect.right());
//        WMainWindow::getMW()->m_gvPhaseSpectrum->viewSet(currect, false);

////        QPointF p = WMainWindow::getMW()->m_gvPhaseSpectrum->mapToScene(WMainWindow::getMW()->m_gvPhaseSpectrum->viewport()->rect()).boundingRect().center();
////        p.setX(viewrect.center().x());
////        WMainWindow::getMW()->m_gvPhaseSpectrum->centerOn(p);
//    }
}

void QGVSpectrogram::resizeEvent(QResizeEvent* event){
    // Note: Resized is called for all views so better to not forward modifications
//    cout << "QGVSpectrogram::resizeEvent" << endl;

    QRectF oldviewrect = mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect();

//    cout << "QGVSpectrogram::resizeEvent old viewrect: " << oldviewrect.left() << "," << oldviewrect.right() << " X " << oldviewrect.top() << "," << oldviewrect.bottom() << endl;

    if(oldviewrect.width()==0 && oldviewrect.height()==0) {
        updateSceneRect();
        viewSet(m_scene->sceneRect(), false);
    }
    else
        viewSet(oldviewrect, false);

    viewUpdateTexts();

    cursorUpdate(QPointF(-1,0));
//    cout << "QGVSpectrogram::~resizeEvent" << endl;
}

void QGVSpectrogram::scrollContentsBy(int dx, int dy) {

    // Invalidate the necessary parts
    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r);

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r);

    viewUpdateTexts();
    cursorUpdate(QPointF(-1,0));

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
    cursorUpdate(p);
}

void QGVSpectrogram::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setDragMode(QGraphicsView::ScrollHandDrag);
                cursorUpdate(QPointF(-1,0));
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
        //            WMainWindow::getMW()->ui->lblSpectrumSelectionTxt->setText(QString("Selection [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));
            }
            else {
                // When selecting
                m_currentAction = CASelecting;
                m_selection_pressedp = p;
                m_mouseSelection.setTopLeft(m_selection_pressedp);
                m_mouseSelection.setBottomRight(m_selection_pressedp);
                selectionFixAndRefresh();
            }
        }
        else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){
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

    cursorUpdate(p);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
        cursorUpdate(QPointF(-1,0));
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
        cursorUpdate(mapToScene(event->pos()));

        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedp.x());
        selectionChangesRequested();
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedp.x());
        selectionChangesRequested();
    }
    else if(m_currentAction==CAModifSelectionTop){
        m_mouseSelection.setTop(p.y()-m_selection_pressedp.y());
        selectionChangesRequested();
    }
    else if(m_currentAction==CAModifSelectionBottom){
        m_mouseSelection.setBottom(p.y()-m_selection_pressedp.y());
        selectionChangesRequested();
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y(), p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y());
        selectionChangesRequested();
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionChangesRequested();
    }
    else if(m_currentAction==CAWaveformScale){
        // When scaling the waveform
        FTSound* currentftsound = WMainWindow::getMW()->getCurrentFTSound();
        if(currentftsound){
            currentftsound->m_ampscale *= pow(10, -(p.y()-m_selection_pressedp.y())/20.0);
            m_selection_pressedp = p;

            if(currentftsound->m_ampscale>1e10)
                currentftsound->m_ampscale = 1e10;
            else if(currentftsound->m_ampscale<1e-10)
                currentftsound->m_ampscale = 1e-10;

            WMainWindow::getMW()->m_gvWaveform->soundsChanged();
            soundsChanged();
        }
    }
    else{
        QRect selview = mapFromScene(m_selection).boundingRect();

        if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()){
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
        else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){
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

    if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()) {
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
    else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()) {
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

    if(event->key()==Qt::Key_Escape)
        selectionClear();

    QGraphicsView::keyPressEvent(event);
}

void QGVSpectrogram::selectionClear() {
    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

//    if(WMainWindow::getMW()->m_gvPhaseSpectrum)
//        WMainWindow::getMW()->m_gvPhaseSpectrum->selectionClear();
//    if(WMainWindow::getMW()->m_gvSpectrum)
//        WMainWindow::getMW()->m_gvSpectrum->selectionClear();

    selectionSetTextInForm();
}

void QGVSpectrogram::selectionSetTextInForm() {

    QString str;

//    cout << "QGVSpectrogram::selectionSetText: " << m_selection.height() << " " << WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height() << endl;

//    if (m_selection.height()==0 && WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height()==0) {
    if (m_selection.height()==0 && m_selection.width()==0) {
        str = "No selection";
    }
    else {
        str += QString("Selection: ");

        double left, right;
        if(m_selection.height()>0) {
            left = m_selection.left();
            right = m_selection.right();
        }
        else {
            left = WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.left();
            right = WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.right();
        }
        str += QString("[%1,%2]%3 Hz").arg(left).arg(right).arg(right-left);

        if (m_selection.height()>0) {
            str += QString(" x [%4,%5]%6 dB").arg(-m_selection.bottom()).arg(-m_selection.top()).arg(m_selection.height());
        }
        if (WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height()>0) {
            str += QString(" x [%7,%8]%9 rad").arg(-WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.bottom()).arg(-WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.top()).arg(WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height());
        }
    }

    WMainWindow::getMW()->ui->lblSpectrogramSelectionTxt->setText(str);
}

void QGVSpectrogram::selectionChangesRequested() {
    selectionFixAndRefresh();

//    if(WMainWindow::getMW()->m_gvPhaseSpectrum) {
//        WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setLeft(m_mouseSelection.left());
//        WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setRight(m_mouseSelection.right());
//        if(WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.height()==0) {
//            WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setTop(-M_PI);
//            WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setBottom(M_PI);
//        }
//        WMainWindow::getMW()->m_gvPhaseSpectrum->selectionFixAndRefresh();
//    }

    selectionSetTextInForm();
}

void QGVSpectrogram::selectionFixAndRefresh() {

    // Order the selection to avoid negative width and negative height
    if(m_mouseSelection.right()<m_mouseSelection.left()){
        float tmp = m_mouseSelection.left();
        m_mouseSelection.setLeft(m_mouseSelection.right());
        m_mouseSelection.setRight(tmp);
    }
    if(m_mouseSelection.top()>m_mouseSelection.bottom()){
        float tmp = m_mouseSelection.top();
        m_mouseSelection.setTop(m_mouseSelection.bottom());
        m_mouseSelection.setBottom(tmp);
    }

    m_selection = m_mouseSelection;

    if(m_selection.left()<0) m_selection.setLeft(0);
    if(m_selection.right()>WMainWindow::getMW()->getFs()/2.0) m_selection.setRight(WMainWindow::getMW()->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    m_giShownSelection->setRect(m_selection);
    m_giShownSelection->show();

    m_giSelectionTxt->setText(QString("%1s,%2Hz").arg(m_selection.width()).arg(m_selection.height()));
//    m_giSelectionTxt->show();
    viewUpdateTexts();

    selectionSetTextInForm();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());
}

void QGVSpectrogram::viewUpdateTexts() {
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

void QGVSpectrogram::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
        zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
        zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
        zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        viewSet(zoomonrect);

        viewUpdateTexts();
//        if(WMainWindow::getMW()->m_gvPhaseSpectrum)
//            WMainWindow::getMW()->m_gvPhaseSpectrum->viewUpdateTexts();
//        if(WMainWindow::getMW()->m_gvSpectrum)
//            WMainWindow::getMW()->m_gvSpectrum->viewUpdateTexts();

        cursorUpdate(QPointF(-1,0));
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

    cursorUpdate(QPointF(-1,0));
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

    cursorUpdate(QPointF(-1,0));
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVSpectrogram::aunzoom(){

    QRectF rect = QRectF(0.0, -m_maxsy, WMainWindow::getMW()->getFs()/2, (m_maxsy-m_minsy));

    if(rect.bottom()>(-m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value()))
        rect.setBottom(-m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMin->value());
    if(rect.top()<(-m_maxsy>m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMax->value()))
        rect.setTop(-m_dlgSettings->ui->sbSpectrogramAmplitudeRangeMax->value());

    viewSet(rect, false);

//    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    cout << "QGVSpectrogram::aunzoom viewrect: " << viewrect.left() << "," << viewrect.right() << " X " << viewrect.top() << "," << viewrect.bottom() << endl;

//    if(WMainWindow::getMW()->m_gvPhaseSpectrum) {
//        WMainWindow::getMW()->m_gvPhaseSpectrum->viewSet(QRectF(0.0, -M_PI, WMainWindow::getMW()->getFs()/2, 2*M_PI), false);
//    }

//    cursorUpdate(QPointF(-1,0));
//    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void QGVSpectrogram::cursorUpdate(QPointF p) {

    QLineF line;
    line.setP1(QPointF(p.x(), m_giCursorVert->line().y1()));
    line.setP2(QPointF(p.x(), m_giCursorVert->line().y2()));
    m_giCursorVert->setLine(line);
    line.setP1(QPointF(m_giCursorHoriz->line().x1(), p.y()));
    line.setP2(QPointF(m_giCursorHoriz->line().x2(), p.y()));
    m_giCursorHoriz->setLine(line);
    cursorFixAndRefresh();

    line.setP1(QPointF(p.x(), m_giCursorVert->line().y1()));
    line.setP2(QPointF(p.x(), m_giCursorVert->line().y2()));
//    WMainWindow::getMW()->m_gvPhaseSpectrum->m_giCursorVert->setLine(line);
//    WMainWindow::getMW()->m_gvPhaseSpectrum->cursorFixAndRefresh();
}

void QGVSpectrogram::cursorFixAndRefresh() {
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
        m_giCursorHoriz->setLine(viewrect.right()-50/trans.m11(), m_giCursorHoriz->line().y1(), WMainWindow::getMW()->getFs()/2.0, m_giCursorHoriz->line().y1());
        m_giCursorVert->show();
        m_giCursorVert->setLine(m_giCursorVert->line().x1(), viewrect.top(), m_giCursorVert->line().x1(), viewrect.top()+18/trans.m22());
        m_giCursorPositionXTxt->show();
        m_giCursorPositionYTxt->show();

        m_giCursorPositionXTxt->setTransform(txttrans);
        m_giCursorPositionYTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionXTxt->boundingRect();
        qreal x = m_giCursorVert->line().x1()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());
        m_giCursorPositionXTxt->setText(QString("%1s").arg(m_giCursorVert->line().x1()));
        m_giCursorPositionXTxt->setPos(x, viewrect.top());

        m_giCursorPositionYTxt->setText(QString("%1Hz").arg(-m_giCursorHoriz->line().y1()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), m_giCursorHoriz->line().y1()-br.height()/trans.m22());
    }
}

void QGVSpectrogram::drawBackground(QPainter* painter, const QRectF& rect){

//    cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVSpectrogram::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    double fs = WMainWindow::getMW()->getFs();

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    // Draw the f0 grids
//    if(!WMainWindow::getMW()->ftfzeros.empty()) {

//        for(size_t fi=0; fi<WMainWindow::getMW()->ftfzeros.size(); fi++){
//            if(!WMainWindow::getMW()->ftfzeros[fi]->m_actionShow->isChecked())
//                continue;

//            QPen outlinePen(WMainWindow::getMW()->ftfzeros[fi]->color);
//            outlinePen.setWidth(0);
//            painter->setPen(outlinePen);
//            painter->setBrush(QBrush(WMainWindow::getMW()->ftfzeros[fi]->color));

            // TODO
//        }
//    }

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    cout << viewrect.width() << " " << viewrect.height() << endl;

    // Draw the sound's spectra
    // TODO only the current one
    for(size_t fi=0; fi<WMainWindow::getMW()->ftsnds.size(); fi++){
        if(!WMainWindow::getMW()->ftsnds[fi]->m_actionShow->isChecked())
            continue;

//        cout << m_nbsteps << " " << m_dftlen << endl;

        // TODO Set source rect according to viewrect
        painter->drawImage(viewrect, m_imgSTFT, m_imgSTFT.rect());
    }

    // Draw the grid above the spectrogram
    // TODO
//    if(m_aShowGrid->isChecked())
//        draw_grid(painter, rect);

//    cout << "QGVSpectrogram::~drawBackground" << endl;
}

void QGVSpectrogram::draw_grid(QPainter* painter, const QRectF& rect) {
    // Prepare the pens and fonts
    // TODO put this in the constructor to limit the allocations in this function
    QPen gridPen(QColor(192,192,192)); //192
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

    // Draw the horizontal lines
    int mn=0;
    painter->setPen(gridPen);
    for(double l=int(viewrect.top()/lstep)*lstep; l<=rect.bottom(); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(rect.left(), l, rect.right(), l));
        mn++;
    }

    // Write the ordinates of the horizontal lines
    painter->setPen(m_gridFontPen);
    QTransform trans = transform();
    for(double l=int(viewrect.top()/lstep)*lstep; l<=rect.bottom(); l+=lstep){
        painter->save();
        painter->translate(QPointF(viewrect.left(), l));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());

        QString txt = QString("%1dB").arg(-l);
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
    for(double l=int(viewrect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(l, rect.top(), l, rect.bottom()));
        mn++;
    }

    // Write the absissa of the vertical lines
    painter->setPen(m_gridFontPen);
    for(double l=int(viewrect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->save();
        painter->translate(QPointF(l, viewrect.bottom()-14/trans.m22()));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1s").arg(l)));
        painter->restore();
    }
}

QGVSpectrogram::~QGVSpectrogram(){
    m_fftresizethread->m_mutex_resizing.lock();
    m_fftresizethread->m_mutex_resizing.unlock();
    m_fftresizethread->m_mutex_changingsizes.lock();
    m_fftresizethread->m_mutex_changingsizes.unlock();
    delete m_fftresizethread;
    delete m_fft;
    delete m_dlgSettings;
}