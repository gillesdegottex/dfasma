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
#include "../external/CFFTW3.h"

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
    , m_main(main)
    , m_first_start(true)
{
    m_ampzoom = 1.0;

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // Grid - Prepare the pens and fonts
    m_gridPen.setColor(QColor(192,192,192));
    m_gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPointSize(8);

    // Mouse Cursor
    giCursor = new QGraphicsLineItem(0, -1, 0, 1);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    giCursor->setPen(cursorPen);
    m_scene->addItem(giCursor);
    giCursorPositionTxt = new QGraphicsSimpleTextItem();
    giCursorPositionTxt->setBrush(QColor(64, 64, 64));
    QFont font;
    font.setPointSize(8);
    giCursorPositionTxt->setFont(font);
    m_scene->addItem(giCursorPositionTxt);

    // Selection
    currentAction = CANothing;
    giSelection = new QGraphicsRectItem();
    giSelection->hide();
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    giSelection->setPen(selectionPen);
    giSelection->setBrush(selectionBrush);
    giSelection->setOpacity(0.5);
    mouseSelection = QRectF(0, -1, 0, 2);
    selection = mouseSelection;
    m_scene->addItem(giSelection);
    m_main->ui->lblSelectionTxt->setText("No selection");

    // Play Cursor
    giPlayCursor = new QGraphicsLineItem(0, -1, 0, 1);
    QPen playCursorPen(QColor(0, 0, 1));
    playCursorPen.setWidth(0);
    giPlayCursor->setPen(playCursorPen);
    giPlayCursor->hide();
    m_scene->addItem(giPlayCursor);

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
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(azoomonselection()));
    m_main->ui->lWaveformToolBar->addWidget(waveformToolBar);

    connect(m_main->ui->sldWaveformAmplitude, SIGNAL(valueChanged(int)), this, SLOT(sldAmplitudeChanged(int)));

//    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); // Use OpenGL

    update_cursor(-1);
}

void QGVWaveform::soundsChanged(){
    if(m_main->hasFilesLoaded())
        m_scene->setSceneRect(-1.0/m_main->getFs(), -1.05/m_ampzoom, m_main->getMaxDuration()+1.0/m_main->getFs(), 2.1/m_ampzoom);

    m_scene->update(this->sceneRect());
}

void QGVWaveform::resizeEvent(QResizeEvent* event){

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

    m_aZoomOnSelection->setEnabled(selection.width()>0);
    update_cursor(-1);
}

void QGVWaveform::scrollContentsBy(int dx, int dy){

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    m_scene->invalidate(QRectF(viewrect.left(), -1.05/m_ampzoom, 5*14/transform().m11(), 2.1/m_ampzoom));

    update_cursor(-1);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVWaveform::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVWaveform::wheelEvent " << numDegrees.y() << endl;

    QTransform trans = transform();
    float h11 = trans.m11();

    h11 += 0.01*h11*numDegrees.y();

    m_aZoomOnSelection->setEnabled(selection.width()>0);
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

//    cout << (event->modifiers().testFlag(Qt::ControlModifier) && event->modifiers().testFlag(Qt::ShiftModifier) && ((event->buttons()&Qt::LeftButton))) << endl;

    if((event->modifiers().testFlag(Qt::ShiftModifier) && !event->modifiers().testFlag(Qt::ControlModifier)) || ((event->buttons()&Qt::RightButton) && (selection.width()==0 || (selection.width()!=0 && (p.x()<selection.left() || p.x()>selection.right()))))) {
        // When scrolling the waveform
        currentAction = CAMoving;
        setDragMode(QGraphicsView::ScrollHandDrag);
        update_cursor(-1);
    }
    else if((event->modifiers().testFlag(Qt::ControlModifier) && !event->modifiers().testFlag(Qt::ShiftModifier)) || ((event->buttons()&Qt::RightButton) && p.x()>=selection.left() && p.x()<=selection.right())){
        if(selection.width()!=0){
            // When scroling the selection
            currentAction = CAMovingSelection;
            selection_pressedx = p.x();
            mouseSelection = selection;
            setCursor(Qt::DragMoveCursor);
            m_main->ui->lblSelectionTxt->setText(QString("Selection: [%1s").arg(selection.left()).append(",%1s] ").arg(selection.right()).append("%1s").arg(selection.width()));
        }
    }
    else if(event->modifiers().testFlag(Qt::ControlModifier) && event->modifiers().testFlag(Qt::ShiftModifier) && ((event->buttons()&Qt::LeftButton))){
        // When scaling the waveform
        currentAction = CAWaveformScale;
        selection_pressedx = p.y();
        setCursor(Qt::SizeVerCursor);
    }
    else if(event->buttons()&Qt::LeftButton){
        QRect selview = mapFromScene(selection).boundingRect();
        if(selection.width()>0 && abs(selview.left()-event->x())<5){
            // Resize left boundary of the selection
            setCursor(Qt::SplitHCursor);
            currentAction = CAModifSelectionLeft;
            selection_pressedx = p.x()-selection.left();
        }
        else if(selection.width()>0 && abs(selview.right()-event->x())<5){
            // Resize right boundary of the selection
            setCursor(Qt::SplitHCursor);
            currentAction = CAModifSelectionRight;
            selection_pressedx = p.x()-selection.right();
        }
        else{
            // When selecting
            currentAction = CASelecting;
            selection_pressedx = p.x();
            mouseSelection.setLeft(selection_pressedx);
            mouseSelection.setRight(selection_pressedx);
            clipandsetselection();
            giSelection->show();
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVWaveform::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    update_cursor(p.x());

    if(currentAction==CAMoving) {
        // When scrolling the waveform
        update_cursor(-1);
        QGraphicsView::mouseMoveEvent(event);
    }
    else if(currentAction==CAModifSelectionLeft){
        mouseSelection.setLeft(p.x()-selection_pressedx);
        clipandsetselection();
    }
    else if(currentAction==CAModifSelectionRight){
        mouseSelection.setRight(p.x()-selection_pressedx);
        clipandsetselection();
    }
    else if(currentAction==CAMovingSelection){
        // When scroling the selection
        mouseSelection.adjust(p.x()-selection_pressedx, 0, p.x()-selection_pressedx, 0);
        clipandsetselection();
        selection_pressedx = p.x();
    }
    else if(currentAction==CASelecting){
        // When selecting
        mouseSelection.setLeft(selection_pressedx);
        mouseSelection.setRight(p.x());
        clipandsetselection();
    }
    else if(currentAction==CAWaveformScale){
        // When scaling the waveform
        int row = m_main->ui->listSndFiles->currentRow();
        if(row>=0){
            m_main->snds[row]->m_ampscale *= pow(10, -10*(p.y()-selection_pressedx)/20.0);
            selection_pressedx = p.y();

            if(m_main->snds[row]->m_ampscale>1e10) m_main->snds[row]->m_ampscale = 1e10;
            else if(m_main->snds[row]->m_ampscale<1e-10) m_main->snds[row]->m_ampscale = 1e-10;

            soundsChanged();
            m_main->m_gvSpectrum->soundsChanged();
        }
    }
    else{
        QRect selview = mapFromScene(selection).boundingRect();
        if(selection.width()>0 && abs(selview.left()-event->x())<5)
            setCursor(Qt::SplitHCursor);
        else if(selection.width()>0 && abs(selview.right()-event->x())<5)
            setCursor(Qt::SplitHCursor);
        else
            setCursor(Qt::ArrowCursor);

        QGraphicsView::mouseMoveEvent(event);
    }

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
}

void QGVWaveform::mouseReleaseEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseReleaseEvent " << selection.width() << endl;

    if(currentAction==CAMoving){
        currentAction = CANothing;
        setDragMode(QGraphicsView::NoDrag);
    }
    else if(currentAction==CAModifSelectionLeft
            || currentAction==CAModifSelectionRight
            || currentAction==CAMovingSelection
            || currentAction==CASelecting
            || currentAction==CAWaveformScale){
        currentAction = CANothing;
        setCursor(Qt::ArrowCursor);
    }

    if(selection.width()<=0){
        m_aZoomOnSelection->setEnabled(false);
        giSelection->hide();
        m_main->ui->lblSelectionTxt->setText("No selection");
    }
    else{
        m_aZoomOnSelection->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void QGVWaveform::keyPressEvent(QKeyEvent* event){

    if(event->key()==Qt::Key_Shift && !event->modifiers().testFlag(Qt::ControlModifier)){
        setDragMode(QGraphicsView::ScrollHandDrag);
        update_cursor(-1);
    }
    else if(event->key()==Qt::Key_Control && !event->modifiers().testFlag(Qt::ShiftModifier)){
        setDragMode(QGraphicsView::NoDrag);
        if(selection.width()>0)
            setCursor(Qt::DragMoveCursor);
    }
    else if((event->key()==Qt::Key_Control && event->modifiers().testFlag(Qt::ShiftModifier)) || (event->key()==Qt::Key_Shift && event->modifiers().testFlag(Qt::ControlModifier))){
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::SizeVerCursor);
    }
}

void QGVWaveform::keyReleaseEvent(QKeyEvent* event){
    setDragMode(QGraphicsView::NoDrag);
    setCursor(Qt::ArrowCursor);
}

void QGVWaveform::clipandsetselection(){
//    cout << "QGVWaveform::clipandsetselection" << endl;

//    if(mouseSelection.left()<-1.0/m_main->getFs()) mouseSelection.setLeft(-1.0/m_main->getFs());
//    if(mouseSelection.right()>m_main->getMaxDuration()) mouseSelection.setRight(m_main->getMaxDuration());

    // Order the selection to avoid negative width
    if(mouseSelection.right()<mouseSelection.left()){
        float tmp = mouseSelection.left();
        mouseSelection.setLeft(mouseSelection.right());
        mouseSelection.setRight(tmp);
    }

    selection = mouseSelection;

    if(selection.left()<0) selection.setLeft(-1.0/m_main->getFs());
    if(selection.right()>m_main->getMaxLastSampleTime()) selection.setRight(m_main->getMaxLastSampleTime()+0.5/m_main->getFs());

    // Clip selection between samples
    selection.setLeft((0.5+int(selection.left()*m_main->getFs()))/m_main->getFs());
    if(mouseSelection.width()==0)
        selection.setRight(selection.left());
    else
        selection.setRight((0.5+int(selection.right()*m_main->getFs()))/m_main->getFs());

    giSelection->setRect(selection.left(), -1, selection.width(), 2);

    m_main->ui->lblSelectionTxt->setText(QString("Selection: [%1s").arg(selection.left()).append(",%1s] ").arg(selection.right()).append("%1s").arg(selection.width()));

    // Spectrum
    m_main->m_gvSpectrum->setSelection(selection.left(), selection.right());

//    cout << "~QGVWaveform::clipandsetselection" << endl;
}

void QGVWaveform::azoomonselection(){
    if(selection.width()>0){
        centerOn(0.5*(selection.left()+selection.right()), 0);

        QTransform trans = transform();
        float h11 = (viewport()->rect().width()-1)/selection.width();

        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

        m_aZoomIn->setEnabled(true);
        m_aZoomOut->setEnabled(true);
        m_aUnZoom->setEnabled(true);

        update_cursor(-1);

        m_aZoomOnSelection->setEnabled(false);
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
    m_aZoomOnSelection->setEnabled(selection.width()>0);

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
    m_aZoomOnSelection->setEnabled(selection.width()>0);

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
    m_aZoomOnSelection->setEnabled(selection.width()>0);

    setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));

    update_cursor(-1);
}

void QGVWaveform::sldAmplitudeChanged(int value){

    Q_UNUSED(value);

    m_ampzoom = 100.0/(100-value);
//    m_ampzoom = 1+value/50.0;

    cout << "GVWaveform::sldAmplitudeChanged v=" << value << " z=" << m_ampzoom << endl;

    if(m_main->hasFilesLoaded()){

        m_scene->setSceneRect(-1.0/m_main->getFs(), -1.05/m_ampzoom, m_main->getMaxDuration()+1.0/m_main->getFs(), 2.1/m_ampzoom);

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
        giCursor->hide();
        giCursorPositionTxt->hide();
    }
    else {
//        giCursorPositionTxt->setText("000000000000");
//        QRectF br = giCursorPositionTxt->boundingRect();

        giCursor->show();
        giCursorPositionTxt->show();
        giCursor->setLine(x, -1, x, 1);
        QString txt = QString("%1s").arg(x);
        giCursorPositionTxt->setText(txt);
        QTransform trans = transform();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
        giCursorPositionTxt->setTransform(txttrans);
        QRectF br = giCursorPositionTxt->boundingRect();
//        QRectF viewrect = mapToScene(QRect(QPoint(0,0), QSize(viewport()->rect().width(), viewport()->rect().height()))).boundingRect();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        x = min(x, float(viewrect.right()-br.width()/trans.m11()));
//        if(x+br.width()/trans.m11()>viewrect.right())
//            x = x - br.width()/trans.m11();
        giCursorPositionTxt->setPos(x, -1/m_ampzoom);
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
            nleft = std::max(0, nleft);

            int nright = int(rect.right()*m_main->getFs())+1;
            nright = std::min(int(m_main->snds[fi]->wav.size()-1), nright);

            // std::cout << nleft << " " << nright << " " << m_main->snds[0]->wav.size()-1 << endl;

            // Draw a line between each sample
            float prevx=nleft/m_main->getFs();
            float y = -a*m_main->snds[fi]->wav[nleft];
            float prevy=y;
            // TODO prob appear with very long waveforms
            for(int n=nleft+1; n<=nright; n++){
                y = -a*m_main->snds[fi]->wav[n];
                painter->drawLine(QLineF(prevx, prevy, n/m_main->getFs(), y));

                prevx = n/m_main->getFs();
                prevy = y;
            }

            // When resolution is big enough, draw the ticks marks at each sample
            float samppixdensity_dotsthr = 0.125;
    //        std::cout << samppixdensity << endl;
            if(samppixdensity<samppixdensity_dotsthr){
                qreal dy = ((samppixdensity_dotsthr-samppixdensity)/samppixdensity_dotsthr)*(1.0/20);

                for(int n=nleft; n<=nright; n++)
                    painter->drawLine(QLineF(n/m_main->getFs(), -a*m_main->snds[fi]->wav[n]-dy, n/m_main->getFs(), -a*m_main->snds[fi]->wav[n]+dy));
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

                int pn = std::max(0, int(0.5+pixelrect.left()*m_main->getFs()));
                int nn = std::min(int(0.5+m_main->snds[fi]->wav.size()-1), int(0.5+pixelrect.right()*m_main->getFs()));
//                std::cout << pn << " " << nn << " " << nn-pn << endl;

                float miny = 1.0;
                float maxy = -1.0;
                float y;
                for(int m=pn; m<=nn; m+=1){
                    y = -a*m_main->snds[fi]->wav[m];
                    miny = std::min(miny, y);
                    maxy = std::max(maxy, y);
                }
                // TODO Don't like the resulting line style
                painter->drawRect(QRectF(pixelrect.left(), miny, pixelrect.width()/10.0, maxy-miny));

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
    painter->setFont(m_gridFont);
    QTransform trans = transform();

    // Horizontal lines

    // Adapt the lines ordinates to the viewport
    double lstep;
    if(m_ampzoom==1){
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
        giPlayCursor->setLine(0, -1, 0, 1);
        giPlayCursor->hide();
    }
    else{
        giPlayCursor->setLine(t, -1, t, 1);
        giPlayCursor->show();
    }
}
