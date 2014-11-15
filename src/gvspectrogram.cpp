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
    , m_imgSTFT(1, 1, QImage::Format_RGB32)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVSpectrogramWDialogSettings(this);

    QSettings settings;
    WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setValue(settings.value("qgvspectrogram/sldSpectrogramAmplitudeMin", -250).toInt());
    WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setValue(settings.value("qgvspectrogram/sldSpectrogramAmplitudeMax", 0).toInt());

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

    m_stftcomputethread = new STFTComputeThread(this);

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
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    WMainWindow::getMW()->ui->pgbSpectrogramSTFTCompute->hide();
    WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText("");

    connect(m_stftcomputethread, SIGNAL(stftComputing()), this, SLOT(stftComputing()));
    connect(m_stftcomputethread, SIGNAL(stftComputed()), this, SLOT(updateAmplitudeExtent()));
    connect(m_stftcomputethread, SIGNAL(stftComputed()), this, SLOT(updateSTFTPlot()));

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aShowProperties);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(WMainWindow::getMW()->m_dlgSettings->ui->sbToolBarSizes->value(),WMainWindow::getMW()->m_dlgSettings->ui->sbToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    WMainWindow::getMW()->ui->lSpectrogramToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aShowGrid);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(updateDFTSettings()));

    connect(WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin, SIGNAL(valueChanged(int)), this, SLOT(updateSTFTPlot()));
    connect(WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax, SIGNAL(valueChanged(int)), this, SLOT(updateSTFTPlot()));

    updateDFTSettings(); // Prepare a window from loaded settings
}

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
QRectF QGVSpectrogram::removeHiddenMargin(const QRectF& sceneRect){
    const int bugMargin = 2;
    const double mx = sceneRect.width()/viewport()->size().width()*bugMargin;
    const double my = sceneRect.height()/viewport()->size().height()*bugMargin;
    return sceneRect.adjusted(mx, my, -mx, -my);
}

void QGVSpectrogram::stftComputing(){
    WMainWindow::getMW()->ui->pgbSpectrogramSTFTCompute->show();
    WMainWindow::getMW()->ui->lblSpectrogramInfoTxt->setText(QString("Computing ..."));
}

void QGVSpectrogram::updateDFTSettings(){

//    cout << "QGVSpectrogram::updateDFTSettings" << endl;

    int winlen = std::floor(0.5+WMainWindow::getMW()->getFs()*m_dlgSettings->ui->sbWindowSize->value());
    // TODO Check for forced odd or not

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

    computeSTFT();

//    cout << "QGVSpectrogram::~updateDFTSettings" << endl;
}

void QGVSpectrogram::computeSTFT(){
//    cout << "QGVSpectrogram::computeSTFT" << endl;

    FTSound* csnd = WMainWindow::getMW()->getCurrentFTSound(true);
    if(csnd) {
        int stepsize = std::floor(0.5+WMainWindow::getMW()->getFs()*m_dlgSettings->ui->sbStepSize->value());
        int dftlen = pow(2, std::ceil(log2(float(m_win.size())))+m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());

//        cout << "QGVSpectrogram::computeSTFT compute on current sound stepsize=" << stepsize << " dftlen=" << dftlen << endl;

        m_stftcomputethread->compute(csnd, m_win, stepsize, dftlen);
    }

//    cout << "QGVSpectrogram::~computeSTFT" << endl;
}
void QGVSpectrogram::updateAmplitudeExtent(){
//    cout << "QGVSpectrogram::updateAmplitudeExtent" << endl;
    FTSound* csnd = WMainWindow::getMW()->getCurrentFTSound(true);
    if(csnd) {
        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setMaximum(csnd->m_stft_max);
        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setMinimum(csnd->m_stft_min+1);
        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setMaximum(csnd->m_stft_max-1);
        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setMinimum(csnd->m_stft_min);
    }

    // If the current is NOT opaque:
//    if(WMainWindow::getMW()->ftsnds.size()>0){
//        qreal min = 1e300;
//        qreal max = -1e300;
//        for(unsigned int si=0; si<WMainWindow::getMW()->ftsnds.size(); si++){
//            min = std::min(min, WMainWindow::getMW()->ftsnds[si]->m_stft_min);
//            max = std::max(max, WMainWindow::getMW()->ftsnds[si]->m_stft_max);
//        }

//        cout << "updateAmplitudeExtent " << min << ":" << max << endl;

//        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setMaximum(max);
//        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setMinimum(min);
//        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setMaximum(max);
//        WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setMinimum(min);
//    }
//    cout << "QGVSpectrogram::~updateAmplitudeExtent" << endl;
}
void QGVSpectrogram::updateSTFTPlot(){
//    cout << "QGVSpectrogram::updateSTFTPlot" << endl;

    // Fix limits between min and max sliders
    WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->setMinimum(WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->value()+1);
    WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->setMaximum(WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->value()-1);

    FTSound* csnd = WMainWindow::getMW()->getCurrentFTSound(true);
    if(csnd && csnd->m_stft.size()>0) {
        int dftlen = (csnd->m_stft[0].size()-1)*2;

        // Update the image from the sound's STFT
        m_imgSTFT = QImage(csnd->m_stft.size(), dftlen/2+1, QImage::Format_RGB32);

//        cout << WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->value() << ":" << WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->value() << endl;

        QPainter imgpaint(&m_imgSTFT);
        QPen outlinePen(QColor(255, 0, 0));
        outlinePen.setWidth(0);
        imgpaint.setPen(outlinePen);
        imgpaint.setOpacity(1);
        m_imgSTFT.fill(QColor(0,0,255));

        for(size_t si=0; si<csnd->m_stft.size(); si++){
            for(int n=0; n<dftlen/2+1; n++) {
                double y = csnd->m_stft[si][n];

                int color = 256-(256*(y-WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->value())/(WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->value()-WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->value()));

                if(color<0) color = 0;
                else if(color>255) color = 255;
                m_imgSTFT.setPixel(QPoint(si,dftlen/2-n), QColor(color, color, color).rgb());
            }
        }

        m_scene->update();
    }
//    cout << "QGVSpectrogram::~updateSTFTPlot" << endl;
}

void QGVSpectrogram::updateSceneRect() {
    m_scene->setSceneRect(-1.0/WMainWindow::getMW()->getFs(), 0.0, WMainWindow::getMW()->getMaxDuration()+1.0/WMainWindow::getMW()->getFs(), WMainWindow::getMW()->getFs()/2);
}

void QGVSpectrogram::soundsChanged(){
    if(WMainWindow::getMW()->ftsnds.size()>0)
        computeSTFT(); // TODO can be very heavy, should update only the necessary part
}

void QGVSpectrogram::viewSet(QRectF viewrect, bool sync) {

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
}

void QGVSpectrogram::viewSync() {
    if(WMainWindow::getMW()->m_gvWaveform) {
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

        QRectF currect = WMainWindow::getMW()->m_gvWaveform->mapToScene(WMainWindow::getMW()->m_gvWaveform->viewport()->rect()).boundingRect();
        currect.setLeft(viewrect.left());
        currect.setRight(viewrect.right());
        WMainWindow::getMW()->m_gvWaveform->viewSet(currect, false);
    }
}

void QGVSpectrogram::resizeEvent(QResizeEvent* event){
    // Note: Resized is called for all views so better to not forward modifications
//    cout << "QGVSpectrogram::resizeEvent" << endl;

    if((event->oldSize().width()*event->oldSize().height()==0) && (event->size().width()*event->size().height()>0)) {

        updateSceneRect();

        if(WMainWindow::getMW()->m_gvWaveform->viewport()->rect().width()*WMainWindow::getMW()->m_gvWaveform->viewport()->rect().height()>0){
            QRectF spectrorect = WMainWindow::getMW()->m_gvWaveform->mapToScene(WMainWindow::getMW()->m_gvWaveform->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(spectrorect.left());
            viewrect.setRight(spectrorect.right());
            viewrect.setTop(0);
            viewrect.setBottom(WMainWindow::getMW()->getFs()/2);
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);
    }
    else if((event->oldSize().width()*event->oldSize().height()>0) && (event->size().width()*event->size().height()>0))
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

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
//    cout << "viewport rect:" << viewport()->rect().width() << " " << viewport()->rect().height() << endl;
//    cout << "scene view rect: " << viewrect.width() << " " << viewrect.height() << endl;
//    cout << "scene view rect: " << viewrect.left() << ", " << viewrect.right() << "; " << viewrect.top() << ", " << viewrect.bottom() << endl;


    // Draw the sound's spectra
    FTSound* csnd = WMainWindow::getMW()->getCurrentFTSound();
    if(csnd && csnd->m_actionShow->isChecked()) {

//        cout << m_imgSTFT.size().width() << "x" << m_imgSTFT.size().height() << endl;

        QRectF srcrect;// TODO Check time synchro
        srcrect.setLeft(m_imgSTFT.rect().width()*viewrect.left()/m_scene->sceneRect().width());
        srcrect.setRight(m_imgSTFT.rect().width()*viewrect.right()/m_scene->sceneRect().width());
        srcrect.setTop(m_imgSTFT.rect().height()*viewrect.top()/m_scene->sceneRect().height());
        srcrect.setBottom(m_imgSTFT.rect().height()*viewrect.bottom()/m_scene->sceneRect().height());

//        cout << "img src: " << srcrect.left() << ", " << srcrect.right() << "; " << srcrect.top() << ", " << srcrect.bottom() << endl;

//        cout << m_imgSTFT.rect().width() << " " << m_imgSTFT.rect().height() << endl;
        // TODO Set source rect according to viewrect
        painter->drawImage(viewrect, m_imgSTFT, srcrect);
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

void QGVSpectrogram::settingsSave() {
    QSettings settings;
    settings.setValue("qgvspectrogram/m_aShowGrid", m_aShowGrid->isChecked());
    settings.setValue("qgvspectrogram/sldSpectrogramAmplitudeMin", WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMin->value());
    settings.setValue("qgvspectrogram/sldSpectrogramAmplitudeMax", WMainWindow::getMW()->ui->sldSpectrogramAmplitudeMax->value());
    settings.setValue("qgvspectrogram/sbSpectrogramOversamplingFactor", m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());
    settings.setValue("qgvspectrogram/cbWindowSizeForcedOdd", m_dlgSettings->ui->cbWindowSizeForcedOdd->isChecked());
    settings.setValue("qgvspectrogram/cbSpectrogramWindowType", m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex());
    settings.setValue("qgvspectrogram/spWindowNormPower", m_dlgSettings->ui->spWindowNormPower->value());
    settings.setValue("qgvspectrogram/spWindowNormSigma", m_dlgSettings->ui->spWindowNormSigma->value());
    settings.setValue("qgvspectrogram/spWindowExpDecay", m_dlgSettings->ui->spWindowExpDecay->value());
}

QGVSpectrogram::~QGVSpectrogram(){
    m_stftcomputethread->m_mutex_computing.lock();
    m_stftcomputethread->m_mutex_computing.unlock();
    m_stftcomputethread->m_mutex_changingparams.lock();
    m_stftcomputethread->m_mutex_changingparams.unlock();
    delete m_stftcomputethread;
    delete m_dlgSettings;
}
