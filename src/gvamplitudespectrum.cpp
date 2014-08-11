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

#include "gvamplitudespectrum.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"
#include "gvwaveform.h"
#include "gvphasespectrum.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "../external/FFTwrapper.h"
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

// TODO option to keep ratio between zoomx and zoomy so that zoomy is also limited

FFTResizeThread::FFTResizeThread(FFTwrapper* fft, QObject* parent)
    : QThread(parent)
    , m_fft(fft)
    , m_size_resizing(-1)
    , m_size_todo(-1)
{
//    setPriority(QThread::IdlePriority);
}

/*int FFTResizeThread::size() {

    m_mutex_changingsizes.lock();

    int s = m_size_current;

    m_mutex_changingsizes.unlock();

    return s;
}*/

void FFTResizeThread::resize(int newsize) {

    m_mutex_changingsizes.lock();

//    std::cout << "FFTResizeThread::resize resizing=" << m_size_resizing << " todo=" << newsize << endl;

    if(m_mutex_resizing.tryLock()){
        // Not resizing

        if(newsize==m_fft->size()){
            m_mutex_resizing.unlock();
        }
        else{
            m_size_resizing = newsize;
            emit fftResizing(m_fft->size(), m_size_resizing);
            start();
        }
    }
    else{
        // Resizing

        if(newsize!=m_size_resizing) {
            m_size_todo = newsize;  // Ask to resize again, once the current resizing is finished
            emit fftResizing(m_size_resizing, m_size_todo); // This also gives the impression to the user that the FFT is resizing to m_size_todo whereas, at this point, it is still resizing to m_size_resizing
        }
    }

//    std::cout << "~FFTResizeThread::resize" << endl;

    m_mutex_changingsizes.unlock();
}

void FFTResizeThread::run() {

    bool resize = true;
    do{
        int prevSize = m_fft->size();

//        std::cout << "FFTResizeThread::run " << prevSize << "=>" << m_size_resizing << endl;

        m_fft->resize(m_size_resizing);

//        std::cout << "FFTResizeThread::run resize finished" << endl;

        // Check if it has to be resized again
        m_mutex_changingsizes.lock();
        if(m_size_todo!=-1){
//            m_mutex_resizing.unlock();
            m_size_resizing = m_size_todo;
            m_size_todo = -1;
            m_mutex_changingsizes.unlock();
    //        cout << "FFTResizeThread::run emit fftResizing " << m_size_resizing << endl;
    //        emit fftResizing(m_cfftw3->size(), m_size_resizing);
        }
        else{
//            m_mutex_resizing.unlock();
            m_size_resizing = -1;
            m_mutex_changingsizes.unlock();
            emit fftResized(prevSize, m_fft->size());
            resize = false;
        }

    }
    while(resize);
    m_mutex_resizing.unlock();

//    std::cout << "~FFTResizeThread::run m_size_resizing=" << m_size_resizing << " m_size_todo=" << m_size_todo << endl;
}


QGVAmplitudeSpectrum::QGVAmplitudeSpectrum(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_winlen(0)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVAmplitudeSpectrumWDialogSettings(this);

    // Load the settings
    QSettings settings;
    m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->setValue(settings.value("qgvamplitudespectrum/sbSpectrumAmplitudeRangeMin", -215).toInt());
    m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->setValue(settings.value("qgvamplitudespectrum/sbSpectrumAmplitudeRangeMax", 10).toInt());
    m_dlgSettings->ui->sbSpectrumOversamplingFactor->setValue(settings.value("qgvamplitudespectrum/sbSpectrumOversamplingFactor", 1).toInt());

    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setStatusTip(tr("Show &grid"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setChecked(settings.value("qgvspectrum/m_aShowGrid", true).toBool());
    connect(m_aShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));

    m_fft = new FFTwrapper();
    m_fftresizethread = new FFTResizeThread(m_fft, this);

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -100, 0, 100);
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
    WMainWindow::getMW()->ui->lblSpectrumSelectionTxt->setText("No selection");

    // Build actions
    QToolBar* toolBar = new QToolBar(this);
    m_aZoomIn = new QAction(tr("Zoom In"), this);;
    m_aZoomIn->setStatusTip(tr("Zoom In"));
    m_aZoomIn->setShortcut(Qt::Key_Plus);
    m_aZoomIn->setIcon(QIcon(":/icons/zoomin.svg"));
    toolBar->addAction(m_aZoomIn);
    connect(m_aZoomIn, SIGNAL(triggered()), this, SLOT(azoomin()));
    m_aZoomOut = new QAction(tr("Zoom Out"), this);;
    m_aZoomOut->setStatusTip(tr("Zoom Out"));
    m_aZoomOut->setShortcut(Qt::Key_Minus);
    m_aZoomOut->setIcon(QIcon(":/icons/zoomout.svg"));
    toolBar->addAction(m_aZoomOut);
    connect(m_aZoomOut, SIGNAL(triggered()), this, SLOT(azoomout()));
    m_aUnZoom = new QAction(tr("Un-Zoom"), this);
    m_aUnZoom->setStatusTip(tr("Un-Zoom"));
    m_aUnZoom->setShortcut(Qt::Key_Z);
    m_aUnZoom->setIcon(QIcon(":/icons/unzoomxy.svg"));
    toolBar->addAction(m_aUnZoom);
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(false);
    m_aZoomOnSelection->setShortcut(Qt::Key_S);
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    toolBar->addAction(m_aZoomOnSelection);
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));
    toolBar->addAction(m_aSelectionClear);

    WMainWindow::getMW()->ui->lSpectrumToolBar->addWidget(toolBar);
    WMainWindow::getMW()->ui->lblSpectrumSelectionTxt->setText("No selection");

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    WMainWindow::getMW()->ui->pgbFFTResize->hide();
    WMainWindow::getMW()->ui->lblSpectrumInfoTxt->setText("");

    connect(m_fftresizethread, SIGNAL(fftResized(int,int)), this, SLOT(computeDFTs()));
    connect(m_fftresizethread, SIGNAL(fftResizing(int,int)), this, SLOT(fftResizing(int,int)));

    m_minsy = -std::numeric_limits<double>::infinity();
    m_maxsy = std::numeric_limits<double>::infinity();
    updateSceneRect();

    // Build the context menu
    m_contextmenu.addAction(m_aShowGrid);
    m_contextmenu.addSeparator();
    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the spectrum view"));
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(updateSceneRect()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(updateDFTSettings()));
}

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
QRectF QGVAmplitudeSpectrum::removeHiddenMargin(const QRectF& sceneRect){
    const int bugMargin = 2;
    const double mx = sceneRect.width()/viewport()->size().width()*bugMargin;
    const double my = sceneRect.height()/viewport()->size().height()*bugMargin;
    return sceneRect.adjusted(mx, my, -mx, -my);
}

void QGVAmplitudeSpectrum::setWindowRange(double tstart, double tend){
    if(tstart==tend) return;

    m_nl = std::max(0, int(0.5+tstart*WMainWindow::getMW()->getFs()));
    m_nr = int(0.5+std::min(WMainWindow::getMW()->getMaxLastSampleTime(),tend)*WMainWindow::getMW()->getFs());
    if(m_nl==m_nr) return;

    m_winlen = m_nr-m_nl+1;

    updateDFTSettings();
}

void QGVAmplitudeSpectrum::updateDFTSettings(){

    updateSceneRect();

    if(m_winlen<2) return;

    m_win = sigproc::hann(m_winlen);
    double winsum = 0.0;
    for(int n=0; n<m_winlen; n++)
        winsum += m_win[n];
    for(int n=0; n<m_winlen; n++)
        m_win[n] /= winsum;

    int dftlen = pow(2, std::ceil(log2(float(m_winlen)))+m_dlgSettings->ui->sbSpectrumOversamplingFactor->value());

    m_fftresizethread->resize(dftlen);

    computeDFTs();
}

void QGVAmplitudeSpectrum::computeDFTs(){
//    std::cout << "QGVAmplitudeSpectrum::computeDFTs " << m_winlen << endl;
    if(m_winlen<2)
        return;

    if(m_fftresizethread->m_mutex_resizing.tryLock()){

        int dftlen = m_fft->size();

        WMainWindow::getMW()->ui->pgbFFTResize->hide();
        WMainWindow::getMW()->ui->lblSpectrumInfoTxt->setText(QString("DFT size=%1").arg(dftlen));

        m_minsy = std::numeric_limits<double>::infinity();
        m_maxsy = 0;
        for(unsigned int fi=0; fi<WMainWindow::getMW()->ftsnds.size(); fi++){
            int pol = 1;
            if(WMainWindow::getMW()->ftsnds[fi]->m_actionInvPolarity->isChecked())
                pol = -1;

            int n = 0;
            int wn = 0;
            for(; n<m_winlen; n++){
                wn = m_nl+n - WMainWindow::getMW()->ftsnds[fi]->m_delay;
                if(wn>=0 && wn<int(WMainWindow::getMW()->ftsnds[fi]->wavtoplay->size()))
                    m_fft->in[n] = pol*(*(WMainWindow::getMW()->ftsnds[fi]->wavtoplay))[wn]*m_win[n];
                else
                    m_fft->in[n] = 0.0;
            }
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute();

            WMainWindow::getMW()->ftsnds[fi]->m_dft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++) {
                WMainWindow::getMW()->ftsnds[fi]->m_dft[n] = m_fft->out[n];
                double y = std::abs(WMainWindow::getMW()->ftsnds[fi]->m_dft[n]);
                m_minsy = std::min(m_minsy, y);
                m_maxsy = std::max(m_maxsy, y);
            }

//            std::cout << "DTF " << fi << " computed" << endl;

        }

        m_minsy = 20*log10(m_minsy)-3;
        m_maxsy = 20*log10(m_maxsy)+3;
        if(m_minsy<m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value())
            m_minsy=m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value();
        if(m_maxsy>m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value())
            m_maxsy=m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value();

        m_fftresizethread->m_mutex_resizing.unlock();

        m_scene->invalidate();
        WMainWindow::getMW()->m_gvPhaseSpectrum->m_scene->invalidate();
    }

//    std::cout << "~QGVAmplitudeSpectrum::computeDFTs" << endl;
}

void QGVAmplitudeSpectrum::settingsSave() {
    QSettings settings;
    settings.setValue("qgvspectrum/m_aShowGrid", m_aShowGrid->isChecked());
    settings.setValue("qgvspectrum/sbSpectrumOversamplingFactor", m_dlgSettings->ui->sbSpectrumOversamplingFactor->value());
    settings.setValue("qgvspectrum/sbSpectrumAmplitudeRangeMin", m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value());
    settings.setValue("qgvspectrum/sbSpectrumAmplitudeRangeMax", m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value());
}

void QGVAmplitudeSpectrum::updateSceneRect() {
    m_scene->setSceneRect(0.0, -m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value(), WMainWindow::getMW()->getFs()/2, (m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value()-m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value()));

    if(m_minsy<m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value())
        m_minsy=m_dlgSettings->ui->sbSpectrumAmplitudeRangeMin->value();
    if(m_maxsy>m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value())
        m_maxsy=m_dlgSettings->ui->sbSpectrumAmplitudeRangeMax->value();

    viewFixAndRefresh();

    if(WMainWindow::getMW()->m_gvPhaseSpectrum)
        WMainWindow::getMW()->m_gvPhaseSpectrum->updateSceneRect();
}

void QGVAmplitudeSpectrum::soundsChanged(){
    if(WMainWindow::getMW()->ftsnds.size()>0){
        computeDFTs();
    }
    m_scene->update();
}

void QGVAmplitudeSpectrum::viewFixAndRefresh() {
//    cout << "QGVAmplitudeSpectrum::viewFixAndRefresh" << endl;
//    cout << m_scene->sceneRect().width() << " " << m_scene->sceneRect().height() << endl;

    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();

    qreal min11 = viewport()->rect().width()/m_scene->sceneRect().width();
    qreal min22 = viewport()->rect().height()/m_scene->sceneRect().height();
    qreal max11 = viewport()->rect().width()/pow(10, -20); // Clip the zoom because the plot can't be infinitely precise
    qreal max22 = viewport()->rect().height()/pow(10, -20);// Clip the zoom because the plot can't be infinitely precise
    if(h11<min11) h11=min11;
    if(h11>max11) h11=max11;
    if(h22<min22) h22=min22;
    if(h22>max22) h22=max22;

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

//    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    cout << "viewrect: " << viewrect.width() << "," << viewrect.height() << endl;

    viewUpdateTexts();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);

//    cout << "QGVAmplitudeSpectrum::~viewFixAndRefresh" << endl;
}

void QGVAmplitudeSpectrum::viewSet(QRectF viewrect) {
//    cout << "QGVAmplitudeSpectrum::viewSet" << endl;

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();
    if(viewrect!=currentviewrect) {
        if(viewrect.top()>m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top());
        if(viewrect.bottom()<m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom());
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());
        fitInView(removeHiddenMargin(viewrect));
        viewFixAndRefresh();
    }

//    cout << "QGVAmplitudeSpectrum::~viewSet" << endl;
}

void QGVAmplitudeSpectrum::viewChangesRequested() {
//    cout << "QGVAmplitudeSpectrum::viewChangesRequested" << endl;

    viewFixAndRefresh();

    if(WMainWindow::getMW()->m_gvPhaseSpectrum) {
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QRectF currect = WMainWindow::getMW()->m_gvPhaseSpectrum->mapToScene(WMainWindow::getMW()->m_gvPhaseSpectrum->viewport()->rect()).boundingRect();
        currect.setLeft(viewrect.left());
        currect.setRight(viewrect.right());
        WMainWindow::getMW()->m_gvPhaseSpectrum->viewSet(currect);
    }

//    cout << "QGVAmplitudeSpectrum::~viewChangesRequested" << endl;
}

void QGVAmplitudeSpectrum::resizeEvent(QResizeEvent* event){

//    cout << "QGVAmplitudeSpectrum::resizeEvent" << endl;

    QRectF viewrect = mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect();

    if(viewrect.width()==0 && viewrect.height()==0)
        viewrect = m_scene->sceneRect();

    if(viewrect.left()<m_scene->sceneRect().left()) viewrect.setLeft(m_scene->sceneRect().left());
    if(viewrect.right()>m_scene->sceneRect().right()) viewrect.setRight(m_scene->sceneRect().right());
    if(viewrect.top()<m_scene->sceneRect().top()) viewrect.setTop(m_scene->sceneRect().top());
    if(viewrect.bottom()>m_scene->sceneRect().bottom()) viewrect.setBottom(m_scene->sceneRect().bottom());

    fitInView(removeHiddenMargin(viewrect));

    viewChangesRequested();

    cursorUpdate(QPointF(-1,0));

//    cout << "QGVAmplitudeSpectrum::~resizeEvent" << endl;
}

void QGVAmplitudeSpectrum::scrollContentsBy(int dx, int dy){
//    cout << "QGVAmplitudeSpectrum::scrollContentsBy" << endl;

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r);

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r);

    cursorUpdate(QPointF(-1,0));

    QGraphicsView::scrollContentsBy(dx, dy);

//    cout << "QGVAmplitudeSpectrum::~scrollContentsBy" << endl;
}

void QGVAmplitudeSpectrum::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVAmplitudeSpectrum::wheelEvent " << numDegrees.y() << endl;

    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 += 0.01*h11*numDegrees.y();
    h22 += 0.01*h22*numDegrees.y();
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    viewChangesRequested();

    QPointF p = mapToScene(event->pos());
    cursorUpdate(p);

//    std::cout << "~QGVAmplitudeSpectrum::wheelEvent" << endl;
}

void QGVAmplitudeSpectrum::mousePressEvent(QMouseEvent* event){
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

void QGVAmplitudeSpectrum::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    cursorUpdate(p);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
        viewChangesRequested();
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

        fitInView(removeHiddenMargin(newrect));

        viewChangesRequested();

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

void QGVAmplitudeSpectrum::mouseReleaseEvent(QMouseEvent* event) {
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

void QGVAmplitudeSpectrum::keyPressEvent(QKeyEvent* event){

//    cout << "QGVAmplitudeSpectrum::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape)
        selectionClear();

    QGraphicsView::keyPressEvent(event);
}

void QGVAmplitudeSpectrum::selectionClear() {
    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    WMainWindow::getMW()->m_gvPhaseSpectrum->selectionClear();

    selectionSetTextInForm();
}

void QGVAmplitudeSpectrum::selectionSetTextInForm() {

    QString str;

//    cout << "QGVAmplitudeSpectrum::selectionSetText: " << m_selection.height() << " " << WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height() << endl;

    if (m_selection.height()==0 && WMainWindow::getMW()->m_gvPhaseSpectrum->m_selection.height()==0) {
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

    WMainWindow::getMW()->ui->lblSpectrumSelectionTxt->setText(str);
}

void QGVAmplitudeSpectrum::selectionChangesRequested() {
    selectionFixAndRefresh();

    WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setLeft(m_mouseSelection.left());
    WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setRight(m_mouseSelection.right());
    if(WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.height()==0) {
        WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setTop(-M_PI);
        WMainWindow::getMW()->m_gvPhaseSpectrum->m_mouseSelection.setBottom(M_PI);
    }
    WMainWindow::getMW()->m_gvPhaseSpectrum->selectionFixAndRefresh();

    selectionSetTextInForm();
}

void QGVAmplitudeSpectrum::selectionFixAndRefresh() {

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

    m_giSelectionTxt->setText(QString("%1Hz,%2dB").arg(m_selection.width()).arg(m_selection.height()));
    m_giSelectionTxt->show();
    viewUpdateTexts();

    selectionSetTextInForm();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());
}

void QGVAmplitudeSpectrum::viewUpdateTexts() {
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

void QGVAmplitudeSpectrum::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        QRectF adj = mapToScene(QRect(0,0,10,10)).boundingRect();
        zoomonrect.adjust(-adj.width(), -adj.height(), adj.width(), adj.height());
        fitInView(removeHiddenMargin(zoomonrect));
        viewChangesRequested();

        cursorUpdate(QPointF(-1,0));
        m_aZoomOnSelection->setEnabled(false);
    }
}

void QGVAmplitudeSpectrum::azoomin(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewChangesRequested();

    cursorUpdate(QPointF(-1,0));
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVAmplitudeSpectrum::azoomout(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewChangesRequested();

    cursorUpdate(QPointF(-1,0));
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVAmplitudeSpectrum::aunzoom(){

    fitInView(removeHiddenMargin(QRectF(0.0, -m_maxsy, WMainWindow::getMW()->getFs()/2, -(m_minsy-m_maxsy))));
    viewChangesRequested();

    if(WMainWindow::getMW()->m_gvPhaseSpectrum)
        WMainWindow::getMW()->m_gvPhaseSpectrum->m_scene->setSceneRect(0.0, -M_PI, WMainWindow::getMW()->getFs()/2, 2*M_PI);

    cursorUpdate(QPointF(-1,0));
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void QGVAmplitudeSpectrum::cursorUpdate(QPointF p) {

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
    WMainWindow::getMW()->m_gvPhaseSpectrum->m_giCursorVert->setLine(line);
    WMainWindow::getMW()->m_gvPhaseSpectrum->cursorFixAndRefresh();
}

void QGVAmplitudeSpectrum::cursorFixAndRefresh() {
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
        m_giCursorPositionXTxt->setText(QString("%1Hz").arg(m_giCursorVert->line().x1()));
        m_giCursorPositionXTxt->setPos(x, viewrect.top());

        m_giCursorPositionYTxt->setText(QString("%1dB").arg(-m_giCursorHoriz->line().y1()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), m_giCursorHoriz->line().y1()-br.height()/trans.m22());
    }
}


void QGVAmplitudeSpectrum::fftResizing(int prevSize, int newSize){
    Q_UNUSED(prevSize);

    WMainWindow::getMW()->ui->pgbFFTResize->show();
    WMainWindow::getMW()->ui->lblSpectrumInfoTxt->setText(QString("Resizing DFT to %1").arg(newSize));
}

void QGVAmplitudeSpectrum::drawBackground(QPainter* painter, const QRectF& rect){

//    cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVAmplitudeSpectrum::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    // Draw grid
    if(m_aShowGrid->isChecked())
        draw_grid(painter, rect);

    // Draw spectrum
    for(unsigned int fi=0; fi<WMainWindow::getMW()->ftsnds.size(); fi++){
        if(!WMainWindow::getMW()->ftsnds[fi]->m_actionShow->isChecked())
            continue;

        if(WMainWindow::getMW()->ftsnds[fi]->m_dft.size()<1)
            continue;

//        QTransform trans = transform();
//        float h11 = float(viewport()->rect().width())/(0.5*WMainWindow::getMW()->getFs());
//        setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

        QPen outlinePen(WMainWindow::getMW()->ftsnds[fi]->color);
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setBrush(QBrush(WMainWindow::getMW()->ftsnds[fi]->color));

        int dftlen = (WMainWindow::getMW()->ftsnds[fi]->m_dft.size()-1)*2;
        double prevx = 0;
        double prevy = 20*log10(abs(WMainWindow::getMW()->ftsnds[fi]->m_dft[0]));
        double a = WMainWindow::getMW()->ftsnds[fi]->m_ampscale;
        std::complex<WAVTYPE>* data = WMainWindow::getMW()->ftsnds[fi]->m_dft.data();
        int kmin = std::max(0, int(dftlen*rect.left()/WMainWindow::getMW()->getFs()));
        int kmax = std::min(dftlen/2+1, int(1+dftlen*rect.right()/WMainWindow::getMW()->getFs()));
        for(int k=kmin; k<=kmax; k++){
            double x = WMainWindow::getMW()->getFs()*k/dftlen;
            double y = 20*log10(a*abs(*(data+k)));
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }

    // Draw the filter response
    if(m_filterresponse.size()>0) {
        QPen outlinePen(QColor(0,0,0));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        int dftlen = (m_filterresponse.size()-1)*2;
        double prevx = 0;
        double prevy = m_filterresponse[0];
        for(int k=0; k<dftlen/2+1; k++){
            double x = WMainWindow::getMW()->getFs()*k/dftlen;
            double y = m_filterresponse[k];
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }


    // Draw the f0 grids
    if(!WMainWindow::getMW()->ftfzeros.empty()) {

        for(size_t fi=0; fi<WMainWindow::getMW()->ftfzeros.size(); fi++){
            if(!WMainWindow::getMW()->ftfzeros[fi]->m_actionShow->isChecked())
                continue;

            QPen outlinePen(WMainWindow::getMW()->ftfzeros[fi]->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            painter->setBrush(QBrush(WMainWindow::getMW()->ftfzeros[fi]->color));

            double ct = 0.5*(m_nl+m_nr)/WMainWindow::getMW()->getFs();
            double cf0 = sigproc::nearest<double>(WMainWindow::getMW()->ftfzeros[fi]->ts, WMainWindow::getMW()->ftfzeros[fi]->f0s, ct, -1.0);

            // cout << ct << ":" << cf0 << endl;
            if(cf0==-1) continue;

            QColor c = WMainWindow::getMW()->ftfzeros[fi]->color;
            c.setAlphaF(1.0);
            outlinePen.setColor(c);
            painter->setPen(outlinePen);
            painter->drawLine(QLineF(cf0, -3000, cf0, 3000));

            c.setAlphaF(0.5);
            outlinePen.setColor(c);
            painter->setPen(outlinePen);

            for(int h=2; h<int(0.5*WMainWindow::getMW()->getFs()/cf0)+1; h++) {
                painter->drawLine(QLineF(h*cf0, -3000, h*cf0, 3000));
            }
        }
    }

//    cout << "QGVAmplitudeSpectrum::~drawBackground" << endl;
}

void QGVAmplitudeSpectrum::draw_grid(QPainter* painter, const QRectF& rect){

    // Plot the grid

    // Prepare the pens and fonts
    // TODO put this in the constructor to limit the allocations in this function
    QPen gridPen(QColor(192,192,192)); //192
    gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    QPen thinGridPen(QColor(224,224,224));
    thinGridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    QPen gridFontPen(QColor(128,128,128));
    gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    QFont font;
    font.setPointSize(8);
    painter->setFont(font);

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
    painter->setPen(gridFontPen);
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
    f = log10(float(viewrect.width()));
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
    painter->setPen(gridFontPen);
    for(double l=int(viewrect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->save();
        painter->translate(QPointF(l, viewrect.bottom()-14/trans.m22()));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1Hz").arg(l)));
        painter->restore();
    }
}

QGVAmplitudeSpectrum::~QGVAmplitudeSpectrum(){
    m_fftresizethread->m_mutex_resizing.lock();
    m_fftresizethread->m_mutex_resizing.unlock();
    m_fftresizethread->m_mutex_changingsizes.lock();
    m_fftresizethread->m_mutex_changingsizes.unlock();
    delete m_fftresizethread;
    delete m_fft;
}
