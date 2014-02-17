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

#include "gvwaveform.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvspectrum.h"

#include <iostream>
using namespace std;

#include <QToolBar>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include <QSplitter>
#include <QDebug>
#include <QGLWidget>
#include <QAudioDeviceInfo>
#include <QStaticText>
#include <QIcon>
#include <QTime>
#include <QAction>

QGVWaveform::QGVWaveform(WMainWindow* main)
    : QGraphicsView(main)
    , m_first_start(true)
    , m_main(main)
    , m_ampzoom(1.0)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_scene->setSceneRect(-1.0/m_main->getFs(), -1.05*m_ampzoom, m_main->getMaxDuration()+1.0/m_main->getFs(), 2.1*m_ampzoom);

    // Grid - Prepare the pens and fonts
    m_gridPen.setColor(QColor(192,192,192));
    m_gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPointSize(8);

    // Mouse Cursor
    m_giCursor = new QGraphicsLineItem(0, -1, 0, 1);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursor->setPen(cursorPen);
    m_scene->addItem(m_giCursor);
    m_giCursorPositionTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionTxt->setBrush(QColor(64, 64, 64));
    QFont font;
    font.setPointSize(8);
    m_giCursorPositionTxt->setFont(font);
    m_scene->addItem(m_giCursorPositionTxt);

    // Selection
    m_currentAction = CANothing;
    m_giSelection = new QGraphicsRectItem();
    m_giSelection->hide();
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giSelection->setPen(selectionPen);
    m_giSelection->setBrush(selectionBrush);
    m_giSelection->setOpacity(0.5);
    m_mouseSelection = QRectF(0, -1, 0, 2);
    m_selection = m_mouseSelection;
    m_scene->addItem(m_giSelection);
    m_main->ui->lblSelectionTxt->setText("No selection");

    // Play Cursor
    m_giPlayCursor = new QGraphicsLineItem(0, -1, 0, 1);
    QPen playCursorPen(QColor(0, 0, 1));
    playCursorPen.setWidth(0);
    m_giPlayCursor->setPen(playCursorPen);
    m_giPlayCursor->hide();
    m_scene->addItem(m_giPlayCursor);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
//    setAlignment(Qt::AlignHCenter4);
    setMouseTracking(true);

    // Build actions
    QToolBar* waveformToolBar = new QToolBar(this);
    m_aZoomIn = new QAction(tr("Zoom In"), this);
    m_aZoomIn->setStatusTip(tr("Zoom In"));
    m_aZoomIn->setShortcut(Qt::Key_Plus);
    QIcon zoominicon(":/icons/zoomin.svg");
    m_aZoomIn->setIcon(zoominicon);
    waveformToolBar->addAction(m_aZoomIn);
    connect(m_aZoomIn, SIGNAL(triggered()), this, SLOT(azoomin()));
    m_aZoomOut = new QAction(tr("Zoom Out"), this);
    m_aZoomOut->setStatusTip(tr("Zoom Out"));
    m_aZoomOut->setShortcut(Qt::Key_Minus);
    QIcon zoomouticon(":/icons/zoomout.svg");
    m_aZoomOut->setIcon(zoomouticon);   
    m_aZoomOut->setEnabled(false);
    waveformToolBar->addAction(m_aZoomOut);
    connect(m_aZoomOut, SIGNAL(triggered()), this, SLOT(azoomout()));
    m_aUnZoom = new QAction(tr("Un-Zoom"), this);
    m_aUnZoom->setStatusTip(tr("Un-Zoom"));
    m_aUnZoom->setShortcut(Qt::Key_Z);
    QIcon unzoomicon(":/icons/unzoomx.svg");
    m_aUnZoom->setIcon(unzoomicon);
    m_aUnZoom->setEnabled(false);
    waveformToolBar->addAction(m_aUnZoom);
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(false);
    m_aZoomOnSelection->setShortcut(Qt::Key_S);
    QIcon zoomselectionicon(":/icons/zoomselectionx.svg");
    m_aZoomOnSelection->setIcon(zoomselectionicon);
    waveformToolBar->addAction(m_aZoomOnSelection);
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));
    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));
    waveformToolBar->addAction(m_aSelectionClear);
    m_main->ui->lWaveformToolBar->addWidget(waveformToolBar);
    m_aFitViewToSoundsAmplitude = new QAction(tr("Fit view to sounds max amplitude"), this);
    m_aFitViewToSoundsAmplitude->setStatusTip(tr("Change the amplitude zoom so that to fit to the maximum of amplitude among all sounds"));
    connect(m_aFitViewToSoundsAmplitude, SIGNAL(triggered()), this, SLOT(fitViewToSoundsAmplitude()));
    m_contextmenu.addAction(m_aFitViewToSoundsAmplitude);

    connect(m_main->ui->sldWaveformAmplitude, SIGNAL(valueChanged(int)), this, SLOT(sldAmplitudeChanged(int)));

//    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); // Use OpenGL

    update_cursor(-1);
}

void QGVWaveform::fitViewToSoundsAmplitude(){
    if(m_main->hasFilesLoaded()){
        qreal maxwavmaxamp = 0.0;
        for(unsigned int si=0; si<m_main->snds.size(); si++)
            maxwavmaxamp = std::max(maxwavmaxamp, m_main->snds[si]->m_ampscale*m_main->snds[si]->m_wavmaxamp);

        // Add a small margin
        maxwavmaxamp *= 1.05;

        m_main->ui->sldWaveformAmplitude->setValue(100-(maxwavmaxamp*100.0));

//        cout << "sldWaveformAmplitude " << 100-(100.0/m_gvWaveform->m_ampzoom) << endl;
    }
}

void QGVWaveform::soundsChanged(){
    if(m_main->hasFilesLoaded()){
        m_scene->setSceneRect(-1.0/m_main->getFs(), -1.05*m_ampzoom, m_main->getMaxDuration()+1.0/m_main->getFs(), 2.1*m_ampzoom);
    }

    m_scene->update(this->sceneRect());
}

void QGVWaveform::resizeEvent(QResizeEvent* event){

//    cout << "QGVWaveform::resizeEvent" << endl;

    Q_UNUSED(event)

    qreal min11 = viewport()->rect().width()/m_scene->sceneRect().width();
    qreal min22 = (viewport()->rect().height())/m_scene->sceneRect().height();

    QTransform trans = transform();
    float h11 = trans.m11();

    // Ensure that the window is not bigger than the waveforms duration
    if(h11<min11) h11=min11;

    // Ensure that the window fits always the scene rectangle
    float h22 = min22;

    // Force the dimensions on first call
    if(m_first_start){
        h11 = min11;
        h22 = min22;
        m_first_start = false;
    }

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    centerOn(QPointF(viewrect.center().x(), 0.0));

    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));

    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
    m_aZoomOut->setEnabled(h11>min11);
    m_aUnZoom->setEnabled(h11>min11);
    update_cursor(-1);
}

void QGVWaveform::scrollContentsBy(int dx, int dy){

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    m_scene->invalidate(QRectF(viewrect.left(), -1.05*m_ampzoom, 5*14/transform().m11(), 2.1*m_ampzoom));

    update_cursor(-1);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVWaveform::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVWaveform::wheelEvent " << numDegrees.y() << endl;

    QTransform trans = transform();
    float h11 = trans.m11();

    h11 += 0.01*h11*numDegrees.y();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
    m_aZoomOut->setEnabled(true);
    m_aZoomIn->setEnabled(true);
    m_aUnZoom->setEnabled(true);

    qreal min11 = viewport()->rect().width()/(10*1.0/m_main->getFs());
    if(h11>=min11){
        h11=min11;
        m_aZoomIn->setEnabled(false);
    }

    qreal max11 = viewport()->rect().width()/m_main->getMaxDuration();
    if(h11<=max11){
        h11=max11;
        m_aZoomOut->setEnabled(false);
        m_aUnZoom->setEnabled(false);
    }

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

    QPointF p = mapToScene(QPoint(event->x(),0));
    update_cursor(p.x());

//    std::cout << "~QGVWaveform::wheelEvent" << endl;
}

void QGVWaveform::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(m_main->ui->actionSelectionMode->isChecked()){

            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // cout << "Scrolling the waveform" << endl;
                m_currentAction = CAMoving;
                setDragMode(QGraphicsView::ScrollHandDrag);
                update_cursor(-1);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.left()-event->x())<5){
                // Resize left boundary of the selection
                setCursor(Qt::SplitHCursor);
                m_currentAction = CAModifSelectionLeft;
                m_selection_pressedx = p.x()-m_selection.left();
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.right()-event->x())<5){
                // Resize right boundary of the selection
                setCursor(Qt::SplitHCursor);
                m_currentAction = CAModifSelectionRight;
                m_selection_pressedx = p.x()-m_selection.right();
            }
            else if(m_selection.width()>0 && (event->modifiers().testFlag(Qt::ControlModifier) || (p.x()>=m_selection.left() && p.x()<=m_selection.right()))){
                // cout << "Scrolling the selection" << endl;
                m_currentAction = CAMovingSelection;
                m_selection_pressedx = p.x();
                m_mouseSelection = m_selection;
                setCursor(Qt::ClosedHandCursor);
                m_main->ui->lblSelectionTxt->setText(QString("Selection: [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier)){
                // When selecting
                m_currentAction = CASelecting;
                m_selection_pressedx = p.x();
                m_mouseSelection.setLeft(m_selection_pressedx);
                m_mouseSelection.setRight(m_selection_pressedx);
                selectionClipAndSet(m_mouseSelection);
                m_giSelection->show();
            }
        }
        else if(m_main->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
                // cout << "Scaling the waveform" << endl;
                m_currentAction = CAWaveformDelay;
                m_selection_pressedx = p.x();
                int row = m_main->ui->listSndFiles->currentRow();
                if(row>=0)
                    m_tmpdelay = m_main->snds[row]->m_delay/m_main->getFs();
                setCursor(Qt::SizeHorCursor);
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                // cout << "Scaling the waveform" << endl;
                m_currentAction = CAWaveformScale;
                m_selection_pressedx = p.y();
                setCursor(Qt::SizeVerCursor);
            }
        }
    }
    else if(event->buttons()&Qt::RightButton){
        // cout << "Calling context menu" << endl;
        int contextmenuheight = 0;
        QPoint posglobal = mapToGlobal(mapFromScene(p)+QPoint(0,contextmenuheight/2));
        m_contextmenu.exec(posglobal);
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVWaveform::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << m_selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    update_cursor(p.x());

    if(m_currentAction==CAMoving) {
        // When scrolling the waveform
        update_cursor(-1);
        QGraphicsView::mouseMoveEvent(event);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedx);
        selectionClipAndSet(m_mouseSelection);
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedx);
        selectionClipAndSet(m_mouseSelection);
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedx, 0, p.x()-m_selection_pressedx, 0);
        selectionClipAndSet(m_mouseSelection);
        m_selection_pressedx = p.x();
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setLeft(m_selection_pressedx);
        m_mouseSelection.setRight(p.x());
        selectionClipAndSet(m_mouseSelection);
    }
    else if(m_currentAction==CAWaveformScale){
        // When scaling the waveform
        int row = m_main->ui->listSndFiles->currentRow();
        if(row>=0){
            m_main->snds[row]->m_ampscale *= pow(10, -10*(p.y()-m_selection_pressedx)/20.0);
            m_selection_pressedx = p.y();

            if(m_main->snds[row]->m_ampscale>1e10) m_main->snds[row]->m_ampscale = 1e10;
            else if(m_main->snds[row]->m_ampscale<1e-10) m_main->snds[row]->m_ampscale = 1e-10;

            soundsChanged();
            m_main->m_gvSpectrum->soundsChanged();
        }
    }
    else if(m_currentAction==CAWaveformDelay){
        int row = m_main->ui->listSndFiles->currentRow();
        if(row>=0){
            m_tmpdelay += p.x()-m_selection_pressedx;
            m_selection_pressedx = p.x();
            m_main->snds[row]->m_delay = int(0.5+m_tmpdelay*m_main->getFs());
            if(m_tmpdelay<0) m_main->snds[row]->m_delay--;

            soundsChanged();
            m_main->m_gvSpectrum->soundsChanged();
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
                if(m_selection.width()>0 && abs(selview.left()-event->x())<5)
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && abs(selview.right()-event->x())<5)
                    setCursor(Qt::SplitHCursor);
                else if(p.x()>=m_selection.left() && p.x()<=m_selection.right())
                    setCursor(Qt::OpenHandCursor);
                else
                    setCursor(Qt::CrossCursor);
            }
        }
        else if(m_main->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
                setCursor(Qt::SizeHorCursor);
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                setCursor(Qt::SizeVerCursor);
            }
        }

        QGraphicsView::mouseMoveEvent(event);
    }

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
}

void QGVWaveform::mouseReleaseEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseReleaseEvent " << m_selection.width() << endl;

    m_currentAction = CANothing;

    if(m_main->ui->actionSelectionMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
            setCursor(Qt::OpenHandCursor);
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
            setCursor(Qt::OpenHandCursor);
        }
        else{
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

    if(abs(m_selection.width())==0)
        selectionClear();
    else{
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void QGVWaveform::keyPressEvent(QKeyEvent* event){

//    cout << "QGVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape)
        selectionClear();

    QGraphicsView::keyPressEvent(event);
}

void QGVWaveform::selectionClear(){
    m_giSelection->hide();
    m_selection = QRectF(0, -1, 0, 2);
    m_giSelection->setRect(m_selection.left(), -1, m_selection.width(), 2);
    m_aZoomOnSelection->setEnabled(false);
    m_main->ui->lblSelectionTxt->setText("No selection");
    m_aSelectionClear->setEnabled(false);
}

void QGVWaveform::selectionClipAndSet(QRectF selection){
//    cout << "QGVWaveform::selectionClipAndSet " << selection.width() << endl;

//    if(mouseSelection.left()<-1.0/m_main->getFs()) mouseSelection.setLeft(-1.0/m_main->getFs());
//    if(mouseSelection.right()>m_main->getMaxDuration()) mouseSelection.setRight(m_main->getMaxDuration());

    // Order the selection to avoid negative width
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }

    m_selection = selection;

    // Clip selection at samples time
    m_selection.setLeft((int(0.5+m_selection.left()*m_main->getFs()))/m_main->getFs());
    if(selection.width()==0)
        m_selection.setRight(m_selection.left());
    else
        m_selection.setRight((int(0.5+m_selection.right()*m_main->getFs()))/m_main->getFs());

    if(m_selection.left()<0) m_selection.setLeft(0.0);
    if(m_selection.right()>m_main->getMaxLastSampleTime()) m_selection.setRight(m_main->getMaxLastSampleTime());

    // Set the visible selection "embracing" the actual selection
    m_giSelection->setRect(m_selection.left()-0.5/m_main->getFs(), -1, m_selection.width()+1.0/m_main->getFs(), 2);

    m_main->ui->lblSelectionTxt->setText(QString("Selection: [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));

    m_giSelection->show();
    if(selection.width()>0){
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }
    else{
        m_aZoomOnSelection->setEnabled(false);
        m_aSelectionClear->setEnabled(false);
    }

    // Spectrum
    m_main->m_gvSpectrum->setWindowRange(m_selection.left(), m_selection.right());

//    cout << "~QGVWaveform::selectionClipAndSet" << endl;
}

void QGVWaveform::selectionZoomOn(){
    if(m_selection.width()>0){
        centerOn(0.5*(m_selection.left()+m_selection.right()), 0);

        QTransform trans = transform();
        float h11 = (viewport()->rect().width()-1)/m_selection.width();

        h11 *= 0.9;

        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

        m_aZoomIn->setEnabled(true);
        m_aZoomOut->setEnabled(true);
        m_aUnZoom->setEnabled(true);

//        selectionClipAndSet();

        m_aZoomOnSelection->setEnabled(false);

        update_cursor(-1);
    }
}

void QGVWaveform::azoomin(){
    QTransform trans = transform();
    float h11 = trans.m11();
    h11 *= 1.5;

    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    qreal min11 = viewport()->rect().width()/(10*1.0/m_main->getFs());
    if(h11>=min11){
        h11=min11;
        m_aZoomIn->setEnabled(false);
    }
    m_aZoomOut->setEnabled(true);
    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

    update_cursor(-1);
}

void QGVWaveform::azoomout(){
    QTransform trans = transform();
    float h11 = trans.m11();
    h11 /= 1.5;

    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    qreal max11 = viewport()->rect().width()/m_main->getMaxDuration();
    if(h11<=max11){
        h11=max11;
        m_aZoomOut->setEnabled(false);
        m_aUnZoom->setEnabled(false);
    }
    m_aZoomIn->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

    update_cursor(-1);
}

void QGVWaveform::aunzoom(){
    QTransform trans = transform();
    float h11 = float(viewport()->rect().width())/(m_main->getMaxDuration());

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(false);
    m_aUnZoom->setEnabled(false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);

    setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

    update_cursor(-1);
}

void QGVWaveform::sldAmplitudeChanged(int value){

    Q_UNUSED(value);

    m_ampzoom = (100-value)/100.0;
//    m_ampzoom = 1+value/50.0;

//    cout << "GVWaveform::sldAmplitudeChanged v=" << value << " z=" << m_ampzoom << endl;

    if(m_main->hasFilesLoaded()){

        m_scene->setSceneRect(-1.0/m_main->getFs(), -1.05*m_ampzoom, m_main->getMaxDuration()+1.0/m_main->getFs(), 2.1*m_ampzoom);

        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        centerOn(QPointF(viewrect.center().x(), 0.0));

        qreal h22 = (viewport()->rect().height())/m_scene->sceneRect().height();

        QTransform trans = transform();
        setTransform(QTransform(trans.m11(), trans.m12(), trans.m21(), h22, 0, 0));
    }

    update_cursor(-1);

    m_scene->invalidate();
}

void QGVWaveform::update_cursor(float x){
    if(x==-1){
        m_giCursor->hide();
        m_giCursorPositionTxt->hide();
    }
    else {
//        giCursorPositionTxt->setText("000000000000");
//        QRectF br = giCursorPositionTxt->boundingRect();

        m_giCursor->show();
        m_giCursorPositionTxt->show();
        m_giCursor->setLine(x, -1, x, 1);
        QString txt = QString("%1s").arg(x);
        m_giCursorPositionTxt->setText(txt);
        QTransform trans = transform();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
        m_giCursorPositionTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionTxt->boundingRect();
//        QRectF viewrect = mapToScene(QRect(QPoint(0,0), QSize(viewport()->rect().width(), viewport()->rect().height()))).boundingRect();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        x = min(x, float(viewrect.right()-br.width()/trans.m11()));
//        if(x+br.width()/trans.m11()>viewrect.right())
//            x = x - br.width()/trans.m11();
        m_giCursorPositionTxt->setPos(x, -1*m_ampzoom);
    }
}

/*class QTimeTracker : public QTime {
public:
    QTimeTracker(){
    }

    std::deque<int> past_elapsed;

    int getAveragedElapsed(){
        int m = 0;
        for(unsigned int i=0; i<past_elapsed.size(); i++){
            m += past_elapsed[i];
        }
        return m / past_elapsed.size();
    }

    void storeElapsed(){
        past_elapsed.push_back(elapsed());
        restart();
    }
};

class QTimeTracker2 : public QTime {
public:
    QTimeTracker2(){
        start();
    }

    std::deque<int> past_elapsed;

    int getAveragedElapsed(){
        int m = 0;
        for(unsigned int i=0; i<past_elapsed.size(); i++){
            m += past_elapsed[i];
        }
        return m / past_elapsed.size();
    }

    void storeElapsed(){
        past_elapsed.push_back(elapsed());
        restart();
    }
};*/

void QGVWaveform::drawBackground(QPainter* painter, const QRectF& rect){

//    static QTimeTracker2 time_tracker;
//    time_tracker.storeElapsed();
//    std::cout << time_tracker.past_elapsed.back() << endl;

//    static QTimeTracker time_tracker;
//    time_tracker.restart();

    draw_grid(painter, rect);
    draw_waveform(painter, rect);

//    time_tracker.storeElapsed();

//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGraphicsView::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << " avg elapsed:" << time_tracker.getAveragedElapsed() << "(" << time_tracker.past_elapsed.back() << ")" << endl;
}

void QGVWaveform::draw_waveform(QPainter* painter, const QRectF& rect){

//    cout << "drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    // TODO move to constructor
    QPen outlinePen(m_main->snds[0]->color);
    outlinePen.setWidth(0);
    painter->setPen(outlinePen);
    painter->setBrush(QBrush(m_main->snds[0]->color));

//    QRectF viewrect = mapToScene(QRect(QPoint(0,0), QSize(viewport()->rect().width(), viewport()->rect().height()))).boundingRect();
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    float samppixdensity = (viewrect.right()-viewrect.left())*m_main->getFs()/viewport()->rect().width();

//    samppixdensity=3;
    if(samppixdensity<10){
//        std::cout << "Full resolution" << endl;

        for(unsigned int fi=0; fi<m_main->snds.size(); fi++){
            if(!m_main->snds[fi]->m_actionShow->isChecked())
                continue;

            float a = m_main->snds[fi]->m_ampscale;
            if(m_main->snds[fi]->m_actionInvPolarity->isChecked())
                a *=-1.0;

            QPen outlinePen(m_main->snds[fi]->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            painter->setBrush(QBrush(m_main->snds[fi]->color));

            int nleft = int(rect.left()*m_main->getFs());
            int nright = int(rect.right()*m_main->getFs())+1;

            // std::cout << nleft << " " << nright << " " << m_main->snds[0]->wav.size()-1 << endl;

            // Draw a line between each sample
            float prevx=nleft/m_main->getFs() + m_main->snds[fi]->m_delay/m_main->getFs();
            float currx=0.0;
            float y;
            if(nleft>=0 && nleft<int(m_main->snds[fi]->wav.size()))
                y = -a*m_main->snds[fi]->wav[nleft];
            else
                y = 0.0;
            float prevy=y;
            // TODO prob appear with very long waveforms
            for(int n=nleft+1; n<=nright; n++){
                int wn = n - m_main->snds[fi]->m_delay;
                if(wn>=0 && wn<int(m_main->snds[fi]->wav.size()))
                    y = -a*m_main->snds[fi]->wav[wn];
                else
                    y = 0.0;
                currx = n/m_main->getFs();
                painter->drawLine(QLineF(prevx, prevy, currx, y));

                prevx = currx;
                prevy = y;
            }

            // When resolution is big enough, draw the ticks marks at each sample
            float samppixdensity_dotsthr = 0.125;
    //        std::cout << samppixdensity << endl;
            if(samppixdensity<samppixdensity_dotsthr){
                qreal dy = ((samppixdensity_dotsthr-samppixdensity)/samppixdensity_dotsthr)*(1.0/20);

                for(int n=nleft; n<=nright; n++){
                    int wn = n - m_main->snds[fi]->m_delay;
                    if(wn>=0 && wn<int(m_main->snds[fi]->wav.size())){
                        currx = n/m_main->getFs();
                        y = -a*m_main->snds[fi]->wav[wn];
                        painter->drawLine(QLineF(currx, y-dy, currx, y+dy));
                    }
                }
            }
        }

        m_main->ui->lblInfoTxt->setText("Full resolution waveform plot");
    }
    else
    {
        // Plot only one line per pixel, in order to improve computational efficiency
//        std::cout << "Compressed resolution" << endl;

        QRectF updateRect = mapFromScene(rect).boundingRect();

        for(unsigned int fi=0; fi<m_main->snds.size(); fi++){
            if(!m_main->snds[fi]->m_actionShow->isChecked())
                continue;

            float a = m_main->snds[fi]->m_ampscale;
            if(m_main->snds[fi]->m_actionInvPolarity->isChecked())
                a *=-1.0;

            QPen outlinePen(m_main->snds[fi]->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            painter->setBrush(QBrush(m_main->snds[fi]->color));

//            float prevx=0;
//            float prevy=0;
            for(int px=updateRect.left(); px<updateRect.right(); px+=1){
                QRectF pixelrect = mapToScene(QRect(QPoint(px,0), QSize(1, 1))).boundingRect();

    //        std::cout << "t: " << pt.x() << " " << t.x() << " " << nt.x() << endl;
    //        std::cout << "ti: " << int(pt.x()*m_main->getFs()) << " " << int(t.x()*m_main->getFs()) << " " << int(nt.x()*m_main->snds[0]->fs) << endl;

                int pn = std::max(qint64(0), qint64(0.5+pixelrect.left()*m_main->getFs())-m_main->snds[fi]->m_delay);
                int nn = std::min(qint64(0.5+m_main->snds[fi]->wav.size()-1), qint64(0.5+pixelrect.right()*m_main->getFs())-m_main->snds[fi]->m_delay);
//                std::cout << pn << " " << nn << " " << nn-pn << endl;

                int maxm = m_main->snds[fi]->wav.size()-1;

                if(nn>=0 && pn<=maxm){
                    float miny = 1.0;
                    float maxy = -1.0;
                    float y;
                    for(int m=pn; m<=nn && m<=maxm; m++){
                        y = -a*m_main->snds[fi]->wav[m];
                        miny = std::min(miny, y);
                        maxy = std::max(maxy, y);
                    }
                    // TODO Don't like the resulting line style
                    painter->drawRect(QRectF(pixelrect.left(), miny, pixelrect.width()/10.0, maxy-miny));
                }

//                int m = pn;
//                float y = -a*m_main->snds[fi]->wav[m];

//        //        std::cout << px << ": " << miny << " " << maxy << endl;

//                painter->drawLine(QLineF(prevx, prevy, pixelrect.left(), y));
//                prevx = pixelrect.left();
//                prevy = y;
            }
        }

        // Tell that the waveform is simplified
        m_main->ui->lblInfoTxt->setText("Quick plot using simplified waveform");
    }
}

void QGVWaveform::draw_grid(QPainter* painter, const QRectF& rect){

//    std::cout << "QGVWaveform::draw_grid " << rect.left() << " " << rect.right() << endl;

    // Plot the grid

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    if(std::isnan(viewrect.width()))
//        return;

    painter->setFont(m_gridFont);
    QTransform trans = transform();

    // Horizontal lines

    // Adapt the lines ordinates to the viewport
    double lstep;
    if(m_ampzoom==1.0){
        lstep = 0.25;
    }
    else{
        double f = log10(float(viewrect.height()));
        int fi;
        if(f<0) fi=int(f-1);
        else fi = int(f);
        lstep = pow(10.0, fi+1);
        int m=1;
        while(int(float(viewrect.height())/lstep)<4){
            lstep /= 2;
            m++;
        }
    }

    // Draw the horizontal lines
    int mn = 0;
    painter->setPen(m_gridPen);
    for(double l=0.0; l<=float(viewrect.height()); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(rect.left(), l, rect.right(), l));
        painter->drawLine(QLineF(rect.left(), -l, rect.right(), -l));
        mn++;
    }

    // Write the ordinates of the horizontal lines
    painter->setPen(m_gridFontPen);
    for(double l=0.0; l<=float(viewrect.height()); l+=lstep){
        painter->save();
        painter->translate(QPointF(viewrect.left(), l));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        QString txt = QString("%1").arg(-l);
        QRectF txtrect = painter->boundingRect(QRectF(), Qt::AlignLeft, txt);
        if(l<viewrect.bottom()-txtrect.height()/trans.m22())
            painter->drawStaticText(QPointF(0, -0.5*txtrect.bottom()), QStaticText(txt));
        painter->restore();
        painter->save();
        painter->translate(QPointF(viewrect.left(), -l));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        txt = QString("%1").arg(l);
        txtrect = painter->boundingRect(QRectF(), Qt::AlignLeft, txt);
        painter->drawStaticText(QPointF(0, -0.5*txtrect.bottom()), QStaticText(txt));
        painter->restore();
    }

    // Vertical lines

    // Adapt the lines absissa to the viewport
    double f = log10(float(viewrect.width()));
    int fi;
    if(f<0) fi=int(f-1);
    else fi = int(f);
    lstep = pow(10.0, fi);
    int m = 1;
    while(int(viewrect.width()/lstep)<6){
        lstep /= 2;
        m++;
    }

    // Draw the vertical lines
    mn = 0;
    painter->setPen(m_gridPen);
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

void QGVWaveform::setPlayCursor(double t){
    if(t==-1){
        m_giPlayCursor->setLine(0, -1, 0, 1);
        m_giPlayCursor->hide();
    }
    else{
        m_giPlayCursor->setLine(t, -1, t, 1);
        m_giPlayCursor->show();
    }
}
