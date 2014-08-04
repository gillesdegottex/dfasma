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

#include "gvspectrum.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvwaveform.h"
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


void QGVSpectrum::setWindowRange(double tstart, double tend){
    if(tstart==tend) return;

    m_nl = std::max(0, int(0.5+tstart*m_main->getFs()));
    m_nr = int(0.5+std::min(m_main->getMaxLastSampleTime(),tend)*m_main->getFs());
    if(m_nl==m_nr) return;

    m_winlen = m_nr-m_nl+1;
    if(m_winlen<2) return;

    m_win = sigproc::hann(m_winlen); // Allow choice with blackman, no-sidelob-window, etc.
    double winsum = 0.0;
    for(int n=0; n<m_winlen; n++)
        winsum += m_win[n];
    for(int n=0; n<m_winlen; n++)
        m_win[n] /= winsum;

    int dftlen = pow(2, std::ceil(log2(float(m_winlen)))+m_sbDFTOverSampFactor->value());

    m_fftresizethread->resize(dftlen);

    computeDFTs();

    m_scene->invalidate();
}

void QGVSpectrum::computeDFTs(){
//    std::cout << "QGVSpectrum::computeDFTs " << m_winlen << endl;
    if(m_winlen<2)
        return;

    if(m_fftresizethread->m_mutex_resizing.tryLock()){

        int dftlen = m_fft->size();

        m_main->ui->pgbFFTResize->hide();
        m_main->ui->lblSpectrumInfoTxt->setText(QString("DFT size=%1").arg(dftlen));

        for(unsigned int fi=0; fi<m_main->ftsnds.size(); fi++){
            int pol = 1;
            if(m_main->ftsnds[fi]->m_actionInvPolarity->isChecked())
                pol = -1;

            int n = 0;
            int wn = 0;
            for(; n<m_winlen; n++){
                wn = m_nl+n - m_main->ftsnds[fi]->m_delay;
                if(wn>=0 && wn<int(m_main->ftsnds[fi]->wavtoplay->size()))
                    m_fft->in[n] = pol*(*(m_main->ftsnds[fi]->wavtoplay))[wn]*m_win[n];
                else
                    m_fft->in[n] = 0.0;
            }
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute();

            m_main->ftsnds[fi]->m_dft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                m_main->ftsnds[fi]->m_dft[n] = m_fft->out[n];

//            std::cout << "DTF " << fi << " computed" << endl;

            m_scene->invalidate();
        }

        m_fftresizethread->m_mutex_resizing.unlock();
    }

//    std::cout << "~QGVSpectrum::computeDFTs" << endl;
}


QGVSpectrum::QGVSpectrum(WMainWindow* main)
//    : QGraphicsView(main)
    : m_main(main)
    , m_winlen(0)
{
    m_first_start = true;

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setStatusTip(tr("Show &grid"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setChecked(true);
    connect(m_aShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    m_contextmenu.addAction(m_aShowGrid);

    m_sbDFTOverSampFactor = new QSpinBox(&m_contextmenu);
    m_sbDFTOverSampFactor->setMinimum(0);
    m_sbDFTOverSampFactor->setValue(1);
    m_sbDFTOverSampFactor->hide(); // TODO Add somewhere
//    m_contextmenu.addAction("Properties");

    m_fft = new FFTwrapper();
    m_fftresizethread = new FFTResizeThread(m_fft, this);

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -100, 0, 100);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursorHoriz->setPen(cursorPen);
    m_giCursorHoriz->hide();
    m_scene->addItem(m_giCursorHoriz);
    m_giCursorVert = new QGraphicsLineItem(0, 0, m_main->getFs()/2.0, 0);
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
    m_giSelection = new QGraphicsRectItem();
    m_giSelection->hide();
    m_scene->addItem(m_giSelection);
    m_giSelectionTxt = new QGraphicsSimpleTextItem();
    m_giSelectionTxt->hide();
    m_giSelectionTxt->setBrush(QColor(64, 64, 64));
    m_giSelectionTxt->setFont(font);
    m_scene->addItem(m_giSelectionTxt);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giSelection->setPen(selectionPen);
    m_giSelection->setBrush(selectionBrush);
    m_giSelection->setOpacity(0.5);
    m_main->ui->lblSpectrumSelectionTxt->setText("No selection");

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

    m_main->ui->lSpectrumToolBar->addWidget(toolBar);
    m_main->ui->lblSpectrumSelectionTxt->setText("No selection");

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
//    setAlignment(Qt::AlignHCenter4);
    setMouseTracking(true);

    m_main->ui->pgbFFTResize->hide();
    m_main->ui->lblSpectrumInfoTxt->setText("");

    connect(m_fftresizethread, SIGNAL(fftResized(int,int)), this, SLOT(computeDFTs()));
    connect(m_fftresizethread, SIGNAL(fftResizing(int,int)), this, SLOT(fftResizing(int,int)));
}

void QGVSpectrum::soundsChanged(){
    if(m_main->ftsnds.size()>0){
        m_scene->setSceneRect(0.0, -10, m_main->getFs()/2, 225);
        computeDFTs();
    }
    m_scene->update(this->sceneRect());
}

void QGVSpectrum::resizeEvent(QResizeEvent* event){

    Q_UNUSED(event)

//    std::cout << "QGVWaveform::resizeEvent " << viewport()->rect().width() << " visible=" << isVisible() << endl;

    qreal min11 = viewport()->rect().width()/m_scene->sceneRect().width();
    qreal min22 = viewport()->rect().height()/m_scene->sceneRect().height();

    QTransform trans = transform();
    float h11 = trans.m11();
    float h22 = trans.m22();

    // Ensure that the window is not bigger than the spectrum's width
    if(h11<min11) h11=min11;

    // Ensure that the window is not bigger than the floating point precision
    if(h22<min22) h22=min22;

    // Force the dimensions on first call
    if(m_first_start){
        h11 = min11;
        h22 = min22;
        m_first_start = false;
    }

    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    update_texts_dimensions();

//    update_cursor(QPointF(-1,0));

    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void QGVSpectrum::scrollContentsBy(int dx, int dy){

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r);

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r);

    update_cursor(QPointF(-1,0));

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVSpectrum::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVWaveform::wheelEvent " << numDegrees.y() << endl;

    QTransform trans = transform();
    float h11 = trans.m11();
    float h22 = trans.m22();
    h11 += 0.01*h11*numDegrees.y();
    h22 += 0.01*h22*numDegrees.y();

    clipzoom(h11, h22);

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    QPointF p = mapToScene(event->pos());
    update_cursor(p);

    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
//    std::cout << "~QGVWaveform::wheelEvent" << endl;
}

void QGVSpectrum::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(m_main->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setDragMode(QGraphicsView::ScrollHandDrag);
                update_cursor(QPointF(-1,0));
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
        //            m_main->ui->lblSpectrumSelectionTxt->setText(QString("Selection [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));
            }
            else {
                // When selecting
                m_currentAction = CASelecting;
                m_selection_pressedp = p;
                m_mouseSelection.setTopLeft(m_selection_pressedp);
                m_mouseSelection.setBottomRight(m_selection_pressedp);
                selectionClipAndSet();
            }
        }
        else if(m_main->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
                // TODO
            }
            else{
                // When scaling the waveform
                m_currentAction = CAWaveformScale;
                m_selection_pressedp = p;
                setCursor(Qt::SizeVerCursor);
            }
        }
    }
    else if(event->buttons()&Qt::RightButton){
        QPoint posglobal = mapToGlobal(mapFromScene(p)+QPoint(0,0));
        m_contextmenu.exec(posglobal);
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVSpectrum::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    update_cursor(p);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the waveform
        update_cursor(QPointF(-1,0));
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedp.x());
        selectionClipAndSet();
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedp.x());
        selectionClipAndSet();
    }
    else if(m_currentAction==CAModifSelectionTop){
        m_mouseSelection.setTop(p.y()-m_selection_pressedp.y());
        selectionClipAndSet();
    }
    else if(m_currentAction==CAModifSelectionBottom){
        m_mouseSelection.setBottom(p.y()-m_selection_pressedp.y());
        selectionClipAndSet();
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y(), p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y());
        selectionClipAndSet();
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionClipAndSet();
    }
    else if(m_currentAction==CAWaveformScale){
        // When scaling the waveform
        FTSound* currentftsound = m_main->getCurrentFTSound();
        if(currentftsound){
            currentftsound->m_ampscale *= pow(10, -(p.y()-m_selection_pressedp.y())/20.0);
            m_selection_pressedp = p;

            if(currentftsound->m_ampscale>1e10)
                currentftsound->m_ampscale = 1e10;
            else if(currentftsound->m_ampscale<1e-10)
                currentftsound->m_ampscale = 1e-10;

            m_main->m_gvWaveform->soundsChanged();
            soundsChanged();
        }
    }
    else{
        QRect selview = mapFromScene(m_selection).boundingRect();

        if(m_main->ui->actionSelectionMode->isChecked()){
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
        else if(m_main->ui->actionEditMode->isChecked()){
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

void QGVSpectrum::mouseReleaseEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseReleaseEvent " << selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    m_currentAction = CANothing;

    if(m_main->ui->actionSelectionMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
            setCursor(Qt::OpenHandCursor);
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
            setCursor(Qt::OpenHandCursor);
        }
        else{
            if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                setCursor(Qt::OpenHandCursor);
            else
                setCursor(Qt::CrossCursor);
        }
    }
    else if(m_main->ui->actionEditMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
        }
        else{
            setCursor(Qt::SizeVerCursor);
        }
    }

    if(abs(m_selection.width())<=0 || abs(m_selection.height())<=0)
        selectionClear();
    else{
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void QGVSpectrum::keyPressEvent(QKeyEvent* event){

//    cout << "QGVSpectrum::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape)
        selectionClear();

    QGraphicsView::keyPressEvent(event);
}

void QGVSpectrum::selectionClear(){
    m_giSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_giSelection->setRect(m_selection.left(), m_selection.top(), m_selection.width(), m_selection.height());
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    m_main->ui->lblSpectrumSelectionTxt->setText("No selection");
    setCursor(Qt::CrossCursor);
}

void QGVSpectrum::selectionClipAndSet(){

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
    if(m_selection.right()>m_main->getFs()/2.0) m_selection.setRight(m_main->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    m_giSelection->setRect(m_selection);

    m_main->ui->lblSpectrumSelectionTxt->setText(QString("Selection: [%1Hz,%3Hz]%5Hz x [%2dB,%4dB]%6dB").arg(m_selection.left()).arg(-m_selection.top()).arg(m_selection.right()).arg(-m_selection.bottom()).arg(m_selection.width()).arg(m_selection.height()));

    m_giSelectionTxt->setText(QString("%1Hz,%2dB").arg(m_selection.width()).arg(m_selection.height()));

    update_texts_dimensions();

    m_giSelection->show();
    m_giSelectionTxt->show();
    m_aZoomOnSelection->setEnabled(true);
    m_aSelectionClear->setEnabled(true);
}

void QGVSpectrum::update_texts_dimensions(){
    QTransform trans = transform();
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(), 1.0/trans.m22());

    // Cursor
    m_giCursorPositionXTxt->setTransform(txttrans);
    m_giCursorPositionYTxt->setTransform(txttrans);
//    m_giCursorPositionXTxt->setPos(p.x()+12*trans.m11(), p.y()+12*trans.m22());

    // Selection
    QRectF br = m_giSelectionTxt->boundingRect();
    m_giSelectionTxt->setTransform(txttrans);
    m_giSelectionTxt->setPos(m_selection.center()-QPointF(0.5*br.width()/trans.m11(), 0.5*br.height()/trans.m22()));
}

void QGVSpectrum::selectionZoomOn(){

    if(m_selection.width()>0 && m_selection.height()>0){

        centerOn(m_selection.center());

        float h11 = float(viewport()->rect().width())/m_selection.width();
        float h22 = float(viewport()->rect().height())/m_selection.height();

        h11 *= 0.9;
        h22 *= 0.9;

        clipzoom(h11, h22);

        QTransform trans = transform();
        setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

        update_texts_dimensions();

        m_aZoomOnSelection->setEnabled(false);
        m_aUnZoom->setEnabled(true);
    }
}

void QGVSpectrum::azoomin(){
    QTransform trans = transform();
    float h11 = trans.m11();
    float h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;

    clipzoom(h11, h22);

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    update_cursor(QPointF(-1,0));

    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVSpectrum::azoomout(){
    QTransform trans = transform();
    float h11 = trans.m11();
    float h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;

    clipzoom(h11, h22);

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    update_cursor(QPointF(-1,0));

    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVSpectrum::aunzoom(){

    if(m_minsy<-300 || m_maxsy<-300) return;

    centerOn(QPointF(m_main->getFs()/4,-0.5*(m_minsy+m_maxsy)));

    float h11 = float(viewport()->rect().width())/m_scene->sceneRect().width();
    float h22 = float(viewport()->rect().height())/(m_maxsy-m_minsy+20);

    clipzoom(h11, h22);

    QTransform trans = transform();
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    update_texts_dimensions();

//    m_aUnZoom->setEnabled(false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void QGVSpectrum::clipzoom(float& h11, float& h22){
    qreal min11 = viewport()->rect().width()/m_scene->sceneRect().width();
    qreal min22 = viewport()->rect().height()/m_scene->sceneRect().height();
    qreal max11 = viewport()->rect().width()/pow(10, -20); // Clip the zoom because the plot can't be infinitely precise
    qreal max22 = viewport()->rect().height()/pow(10, -20);// Clip the zoom because the plot can't be infinitely precise
    if(h11<min11) h11=min11;
    if(h11>max11) h11=max11;
    if(h22<min22) h22=min22;
    if(h22>max22) h22=max22;
}

void QGVSpectrum::update_cursor(QPointF p){
    if(p.x()==-1){
        m_giCursorHoriz->hide();
        m_giCursorVert->hide();
        m_giCursorPositionXTxt->hide();
        m_giCursorPositionYTxt->hide();
    }
    else {
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        m_giCursorHoriz->show();
        m_giCursorHoriz->setLine(0, p.y(), m_main->getFs()/2.0, p.y());
        m_giCursorVert->show();
        m_giCursorVert->setLine(p.x(), -10000, p.x(), 10000);
        m_giCursorPositionXTxt->show();
        m_giCursorPositionYTxt->show();

        QTransform trans = transform();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
        m_giCursorPositionXTxt->setTransform(txttrans);
        m_giCursorPositionYTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionXTxt->boundingRect();
        qreal x = p.x();
        x = min(x, viewrect.right()-br.width()/trans.m11());

        m_giCursorPositionXTxt->setText(QString("%1Hz").arg(p.x()));
        m_giCursorPositionXTxt->setPos(x, viewrect.top());

        m_giCursorPositionYTxt->setText(QString("%1dB").arg(-p.y()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), p.y()-br.height()/trans.m22());

        QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
        m_scene->invalidate(r);
    }
}

void QGVSpectrum::fftResizing(int prevSize, int newSize){
    Q_UNUSED(prevSize);

    m_main->ui->pgbFFTResize->show();
    m_main->ui->lblSpectrumInfoTxt->setText(QString("Resizing DFT to %1").arg(newSize));
}

void QGVSpectrum::drawBackground(QPainter* painter, const QRectF& rect){

//    cout << "drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    // Draw grid
    if(m_aShowGrid->isChecked())
        draw_grid(painter, rect);

    // Draw spectrum
    for(unsigned int fi=0; fi<m_main->ftsnds.size(); fi++){
        if(!m_main->ftsnds[fi]->m_actionShow->isChecked())
            continue;

        if(m_main->ftsnds[fi]->m_dft.size()<1)
            continue;

//        QTransform trans = transform();
//        float h11 = float(viewport()->rect().width())/(0.5*m_main->getFs());
//        setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

        QPen outlinePen(m_main->ftsnds[fi]->color);
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setBrush(QBrush(m_main->ftsnds[fi]->color));

        int dftlen = (m_main->ftsnds[fi]->m_dft.size()-1)*2;
        float prevx = 0;
        float prevy = 20*log10(abs(m_main->ftsnds[fi]->m_dft[0]));
        m_minsy = prevy;
        m_maxsy = prevy;
        for(int k=0; k<dftlen/2+1; k++){
            float x = m_main->getFs()*k/dftlen;
            float y = 20*log10(m_main->ftsnds[fi]->m_ampscale*abs(m_main->ftsnds[fi]->m_dft[k]));
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
            m_minsy = std::min(m_minsy, y);
            m_maxsy = std::max(m_maxsy, y);
        }
    }

    // Draw the filter response
    if(m_filterresponse.size()>0) {
        QPen outlinePen(QColor(0,0,0));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        int dftlen = (m_filterresponse.size()-1)*2;
        float prevx = 0;
        float prevy = m_filterresponse[0];
        m_minsy = prevy;
        m_maxsy = prevy;
        for(int k=0; k<dftlen/2+1; k++){
            float x = m_main->getFs()*k/dftlen;
            float y = m_filterresponse[k];
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
            m_minsy = std::min(m_minsy, y);
            m_maxsy = std::max(m_maxsy, y);
        }
    }


    // Draw the fundamental frequency grid
    if(!m_main->ftfzeros.empty()) {

        for(size_t fi=0; fi<m_main->ftfzeros.size(); fi++){
            if(!m_main->ftfzeros[fi]->m_actionShow->isChecked())
                continue;

            QPen outlinePen(m_main->ftfzeros[fi]->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            painter->setBrush(QBrush(m_main->ftfzeros[fi]->color));

            double ct = 0.5*(m_nl+m_nr)/m_main->getFs();
            double cf0 = sigproc::interp<double>(m_main->ftfzeros[fi]->ts, m_main->ftfzeros[fi]->f0s, ct, -1.0);

            // cout << ct << ":" << cf0 << endl;
            if(cf0==-1) continue;

            QColor c = m_main->ftfzeros[fi]->color;
            c.setAlphaF(1.0);
            outlinePen.setColor(c);
            painter->setPen(outlinePen);
            painter->drawLine(QLineF(cf0, -3000, cf0, 3000));

            c.setAlphaF(0.5);
            outlinePen.setColor(c);
            painter->setPen(outlinePen);

            for(int h=2; h<int(0.5*m_main->getFs()/cf0)+1; h++) {
                painter->drawLine(QLineF(h*cf0, -3000, h*cf0, 3000));
            }
        }
    }

//    cout << "~drawBackground" << endl;
}

void QGVSpectrum::draw_grid(QPainter* painter, const QRectF& rect){

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

QGVSpectrum::~QGVSpectrum(){
    m_fftresizethread->m_mutex_resizing.lock();
    m_fftresizethread->m_mutex_resizing.unlock();
    m_fftresizethread->m_mutex_changingsizes.lock();
    m_fftresizethread->m_mutex_changingsizes.unlock();
    delete m_fftresizethread;
    delete m_fft;
}
