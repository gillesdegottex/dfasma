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

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "gvamplitudespectrum.h"
#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"
#include "gvphasespectrum.h"
#include "gvspectrogram.h"
#include "ftsound.h"
#include "ftlabels.h"

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
#include <QScrollBar>
#include <QMessageBox>


QGVWaveform::QGVWaveform(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_ampzoom(1.0)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    QSettings settings;

    // Grid - Prepare the pens and fonts
    m_gridPen.setColor(QColor(192,192,192));
    m_gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFont.setPixelSize(12);
    m_gridFont.setFamily("Helvetica");
    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setStatusTip(tr("Show &grid"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aShowGrid->setChecked(settings.value("qgvwaveform/m_aShowGrid", true).toBool());
    connect(m_aShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    m_contextmenu.addAction(m_aShowGrid);

    // Mouse Cursor
    m_giCursor = new QGraphicsLineItem(0, -1, 0, 1);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursor->setPen(cursorPen);
    m_scene->addItem(m_giCursor);
    m_giCursorPositionTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionTxt->setBrush(QColor(64, 64, 64));
    QFont font;
    font.setPixelSize(14);
    m_giCursorPositionTxt->setFont(font);
    m_scene->addItem(m_giCursorPositionTxt);

    // Selection
    m_currentAction = CANothing;
    m_giSelection = new QGraphicsRectItem();
    m_giSelection->hide();
    QPen selectionPen(QColor(32, 32, 32));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giSelection->setPen(selectionPen);
    m_giSelection->setBrush(selectionBrush);
    m_giSelection->setOpacity(0.25);
    // QPen mouseselectionPen(QColor(255, 0, 0));
    // mouseselectionPen.setWidth(0);
    // m_giMouseSelection = new QGraphicsRectItem();
    // m_giMouseSelection->setPen(mouseselectionPen);
    // m_scene->addItem(m_giMouseSelection);
    m_giSelection->setOpacity(0.25);
    m_mouseSelection = QRectF(0, -1, 0, 2);
    m_selection = m_mouseSelection;
    m_scene->addItem(m_giSelection);
    WMainWindow::getMW()->ui->lblSelectionTxt->setText("No selection");
    m_giFilteredSelection = new QGraphicsRectItem();
    m_giFilteredSelection->hide();
    QColor filtsecbrush(255, 128, 128);
    QPen filteredSelectionPen(filtsecbrush);
    filteredSelectionPen.setWidth(0);
    m_giFilteredSelection->setPen(filteredSelectionPen);
    QBrush filteredSelectionBrush(filtsecbrush);
    m_giFilteredSelection->setBrush(filteredSelectionBrush);
    m_giFilteredSelection->setOpacity(0.25);
    m_scene->addItem(m_giFilteredSelection);

    // Window
    m_giWindow = new QGraphicsPathItem();
    // QPen pen(QColor(192,192,192));
    QPen pen(QColor(0,0,0));
    pen.setWidth(0);
    m_giWindow->setPen(pen);
    m_giWindow->setOpacity(0.25);
    m_scene->addItem(m_giWindow);
    m_aShowWindow = new QAction(tr("Show window"), this);
    m_aShowWindow->setStatusTip(tr("Show window"));
    m_aShowWindow->setCheckable(true);
    m_aShowWindow->setIcon(QIcon(":/icons/window.svg"));
    m_aShowWindow->setChecked(settings.value("qgvwaveform/m_aShowWindow", true).toBool());
    connect(m_aShowWindow, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    m_contextmenu.addAction(m_aShowWindow);

    m_aShowSTFTWindowCenters = new QAction(tr("Show STFT's window centers"), this);
    m_aShowSTFTWindowCenters->setStatusTip(tr("Show STFT's window centers"));
    m_aShowSTFTWindowCenters->setCheckable(true);
    m_aShowSTFTWindowCenters->setChecked(settings.value("qgvwaveform/m_aShowSTFTWindowCenters", false).toBool());
    connect(m_aShowSTFTWindowCenters, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    m_contextmenu.addAction(m_aShowSTFTWindowCenters);

    // Play Cursor
    m_giPlayCursor = new QGraphicsPathItem();
    QPen playCursorPen(QColor(255, 0, 0));
    playCursorPen.setWidth(0);
    m_giPlayCursor->setPen(playCursorPen);
    m_giPlayCursor->setBrush(QBrush(QColor(255, 0, 0)));
    QPainterPath path;
    path.moveTo(QPointF(0, 1.0));
    path.lineTo(QPointF(0, -1.0));
    path.lineTo(QPointF(1, -1.0));
    path.lineTo(QPointF(1, 1.0));
    m_giPlayCursor->setPath(path);
    playCursorSet(0.0);
    m_scene->addItem(m_giPlayCursor);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    // Build actions
    m_aZoomIn = new QAction(tr("Zoom In"), this);
    m_aZoomIn->setStatusTip(tr("Zoom In"));
    m_aZoomIn->setShortcut(Qt::Key_Plus);
    QIcon zoominicon(":/icons/zoomin.svg");
    m_aZoomIn->setIcon(zoominicon);
    connect(m_aZoomIn, SIGNAL(triggered()), this, SLOT(azoomin()));
    m_aZoomOut = new QAction(tr("Zoom Out"), this);
    m_aZoomOut->setStatusTip(tr("Zoom Out"));
    m_aZoomOut->setShortcut(Qt::Key_Minus);
    QIcon zoomouticon(":/icons/zoomout.svg");
    m_aZoomOut->setIcon(zoomouticon);   
    m_aZoomOut->setEnabled(false);
    connect(m_aZoomOut, SIGNAL(triggered()), this, SLOT(azoomout()));
    m_aUnZoom = new QAction(tr("Un-Zoom"), this);
    m_aUnZoom->setStatusTip(tr("Un-Zoom"));
    QIcon unzoomicon(":/icons/unzoomx.svg");
    m_aUnZoom->setIcon(unzoomicon);
    m_aUnZoom->setEnabled(false);
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(false);
    m_aZoomOnSelection->setShortcut(Qt::Key_S);
    QIcon zoomselectionicon(":/icons/zoomselectionx.svg");
    m_aZoomOnSelection->setIcon(zoomselectionicon);
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));
    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));
    m_aFitViewToSoundsAmplitude = new QAction(tr("Fit the view's amplitude to the max value"), this);
    m_aFitViewToSoundsAmplitude->setIcon(QIcon(":/icons/unzoomy.svg"));
    m_aFitViewToSoundsAmplitude->setStatusTip(tr("Change the amplitude zoom so that to fit to the maximum of amplitude among all sounds"));
    m_contextmenu.addAction(m_aFitViewToSoundsAmplitude);
    connect(WMainWindow::getMW()->ui->btnFitViewToSoundsAmplitude, SIGNAL(clicked()), m_aFitViewToSoundsAmplitude, SLOT(trigger()));
    WMainWindow::getMW()->ui->btnFitViewToSoundsAmplitude->setIcon(QIcon(":/icons/unzoomy.svg"));
    connect(m_aFitViewToSoundsAmplitude, SIGNAL(triggered()), this, SLOT(fitViewToSoundsAmplitude()));

    connect(WMainWindow::getMW()->ui->sldWaveformAmplitude, SIGNAL(valueChanged(int)), this, SLOT(sldAmplitudeChanged(int)));

//    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); // Use OpenGL

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aShowGrid);
//    m_toolBar->addAction(m_aShowWindow);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
//    m_toolBar->addAction(m_aFitViewToSoundsAmplitude);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(WMainWindow::getMW()->m_dlgSettings->ui->sbToolBarSizes->value(),WMainWindow::getMW()->m_dlgSettings->ui->sbToolBarSizes->value()));
//    WMainWindow::getMW()->ui->lWaveformToolBar->addWidget(m_toolBar);
    m_toolBar->setOrientation(Qt::Vertical);
    WMainWindow::getMW()->ui->lWaveformToolBar->addWidget(m_toolBar);

    cursorUpdate(-1);
}

void QGVWaveform::settingsSave() {
    QSettings settings;
    settings.setValue("qgvwaveform/m_aShowGrid", m_aShowGrid->isChecked());
    settings.setValue("qgvwaveform/m_aShowWindow", m_aShowWindow->isChecked());
    settings.setValue("qgvwaveform/m_aShowSTFTWindowCenters", m_aShowSTFTWindowCenters->isChecked());
}

// Remove hard coded margin (Bug 11945)
// See: https://bugreports.qt-project.org/browse/QTBUG-11945
QRectF QGVWaveform::removeHiddenMargin(const QRectF& sceneRect){
    const int bugMargin = 2;
    const double mx = sceneRect.width()/viewport()->size().width()*bugMargin;
    const double my = sceneRect.height()/viewport()->size().height()*bugMargin;
    return sceneRect.adjusted(mx, my, -mx, -my);
}

void QGVWaveform::fitViewToSoundsAmplitude(){
    if(WMainWindow::getMW()->ftsnds.size()>0){
        qreal maxwavmaxamp = 0.0;
        for(unsigned int si=0; si<WMainWindow::getMW()->ftsnds.size(); si++)
            maxwavmaxamp = std::max(maxwavmaxamp, WMainWindow::getMW()->ftsnds[si]->m_ampscale*WMainWindow::getMW()->ftsnds[si]->m_wavmaxamp);

        // Add a small margin
        maxwavmaxamp *= 1.05;

        WMainWindow::getMW()->ui->sldWaveformAmplitude->setValue(100-(maxwavmaxamp*100.0));
    }
}

void QGVWaveform::updateSceneRect() {
    m_scene->setSceneRect(-1.0/WMainWindow::getMW()->getFs(), -1.05*m_ampzoom, WMainWindow::getMW()->getMaxDuration()+1.0/WMainWindow::getMW()->getFs(), 2.1*m_ampzoom);
}

void QGVWaveform::viewSet(QRectF viewrect, bool sync) {

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

        if(viewrect.top()<m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top()-0.01);
        if(viewrect.bottom()>m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom()+0.01);
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        fitInView(removeHiddenMargin(viewrect));

        playCursorUpdate();

        if(sync) viewSync();
    }
}
void QGVWaveform::viewSync() {
    if(WMainWindow::getMW()->m_gvSpectrogram) {
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

        QRectF currect = WMainWindow::getMW()->m_gvSpectrogram->mapToScene(WMainWindow::getMW()->m_gvSpectrogram->viewport()->rect()).boundingRect();
        currect.setLeft(viewrect.left());
        currect.setRight(viewrect.right());
        WMainWindow::getMW()->m_gvSpectrogram->viewSet(currect, false);
    }
}

void QGVWaveform::soundsChanged(){
    m_scene->update();
}

void QGVWaveform::sldAmplitudeChanged(int value){

    m_ampzoom = (100-value)/100.0;

    updateSceneRect();
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setTop(-1.05*m_ampzoom);
    viewrect.setBottom(1.05*m_ampzoom);
    viewSet(viewrect, false);

    cursorUpdate(-1);

    QTransform m;
    m.scale(1.0, m_ampzoom);
    m_giWindow->setTransform(m);

    m_scene->update();
}

void QGVWaveform::azoomin(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(viewrect.left()+viewrect.width()/4);
    viewrect.setRight(viewrect.right()-viewrect.width()/4);
    viewSet(viewrect);

    cursorUpdate(-1);

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(true);
    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void QGVWaveform::azoomout(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(viewrect.left()-viewrect.width()/4);
    viewrect.setRight(viewrect.right()+viewrect.width()/4);
    viewSet(viewrect);

    cursorUpdate(-1);

    m_aZoomOut->setEnabled(true);
    m_aUnZoom->setEnabled(true);
    m_aZoomIn->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void QGVWaveform::aunzoom(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(m_scene->sceneRect().left());
    viewrect.setRight(m_scene->sceneRect().right());
    viewSet(viewrect);

    cursorUpdate(-1);

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(false);
    m_aUnZoom->setEnabled(false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void QGVWaveform::cursorUpdate(float x){
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
        m_giCursorPositionTxt->setPos(x+1/trans.m11(), -1.05*m_ampzoom);
    }
}

void QGVWaveform::resizeEvent(QResizeEvent* event){

    if((event->oldSize().width()*event->oldSize().height()==0) && (event->size().width()*event->size().height()>0)) {

        updateSceneRect();

        if(WMainWindow::getMW()->m_gvSpectrogram->viewport()->rect().width()*WMainWindow::getMW()->m_gvSpectrogram->viewport()->rect().height()>0){
            QRectF spectrorect = WMainWindow::getMW()->m_gvSpectrogram->mapToScene(WMainWindow::getMW()->m_gvSpectrogram->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(spectrorect.left());
            viewrect.setRight(spectrorect.right());
            viewrect.setTop(-10);
            viewrect.setBottom(10);
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);
    }
    else if((event->oldSize().width()*event->oldSize().height()>0) && (event->size().width()*event->size().height()>0))
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    cursorUpdate(-1);
}

void QGVWaveform::scrollContentsBy(int dx, int dy){

//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVWaveform::scrollContentsBy [" << dx << "," << dy << "]" << endl;

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    m_scene->invalidate(QRectF(viewrect.left(), -1.05, 5*14/transform().m11(), 2.10));

    cursorUpdate(-1);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVWaveform::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVWaveform::wheelEvent " << numDegrees.y() << endl;

    if(event->modifiers().testFlag(Qt::ShiftModifier)){
        QScrollBar* sb = horizontalScrollBar();
        sb->setValue(sb->value()-numDegrees.y());
    }
    else {
        QTransform trans = transform();
        float h11 = trans.m11();
        h11 += 0.01*h11*numDegrees.y();
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setTransform(QTransform(h11, trans.m12(), trans.m21(), trans.m22(), 0, 0));
        viewSet();

        QPointF p = mapToScene(QPoint(event->x(),0));
        cursorUpdate(p.x());
        m_aZoomOnSelection->setEnabled(m_selection.width()>0);
        m_aZoomOut->setEnabled(true);
        m_aZoomIn->setEnabled(true);
        m_aUnZoom->setEnabled(true);
    }

//    std::cout << "~QGVWaveform::wheelEvent" << endl;
}

void QGVWaveform::mousePressEvent(QMouseEvent* event){
    // std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()){

            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // cout << "Scrolling the waveform" << endl;
                m_currentAction = CAMoving;
                setDragMode(QGraphicsView::ScrollHandDrag);
                cursorUpdate(-1);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.left()-event->x())<5){
                // Resize left boundary of the selection
                m_currentAction = CAModifSelectionLeft;
                m_mouseSelection = m_selection;
                m_selection_pressedx = p.x()-m_selection.left();
                setCursor(Qt::SplitHCursor);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.right()-event->x())<5){
                // Resize right boundary of the selection
                m_currentAction = CAModifSelectionRight;
                m_mouseSelection = m_selection;
                m_selection_pressedx = p.x()-m_selection.right();
                setCursor(Qt::SplitHCursor);
            }
            else if(m_selection.width()>0 && (event->modifiers().testFlag(Qt::ControlModifier) || (p.x()>=m_selection.left() && p.x()<=m_selection.right()))){
                // cout << "Scrolling the selection" << endl;
                m_currentAction = CAMovingSelection;
                m_selection_pressedx = p.x();
                m_mouseSelection = m_selection;
                setCursor(Qt::ClosedHandCursor);
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
        else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){

            // Look for a nearby marker to modify
            m_ca_pressed_index=-1;
            FTLabels* ftl = WMainWindow::getMW()->getCurrentFTLabels();
            if(ftl){
                for(size_t lli=0; m_ca_pressed_index==-1 && lli<ftl->labels.size(); lli++) {
                    QPoint slp = mapFromScene(QPointF(ftl->starts[lli],0));
                    if(std::abs(slp.x()-event->x())<5) {
                        m_ca_pressed_index = lli;
                        m_currentAction = CALabelModifPosition;
                    }
                }
            }

            if(m_ca_pressed_index==-1) {
                if(event->modifiers().testFlag(Qt::ShiftModifier)){
                    // cout << "Scaling the waveform" << endl;
                    m_currentAction = CAWaveformDelay;
                    m_selection_pressedx = p.x();
                    FTSound* currentftsound = WMainWindow::getMW()->getCurrentFTSound();
                    if(currentftsound)
                        m_tmpdelay = currentftsound->m_delay/WMainWindow::getMW()->getFs();
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
    }
    else if(event->buttons()&Qt::RightButton){
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_currentAction = CAZooming;
            m_selection_pressedx = p.x();
            m_pressed_mouseinviewport = mapFromScene(p);
            m_pressed_scenerect = mapToScene(viewport()->rect()).boundingRect();
            setCursor(Qt::CrossCursor);
        }
        else if (event->modifiers().testFlag(Qt::ControlModifier) &&
                 m_selection.width()>0) {
            m_currentAction = CAStretchSelection;
            m_mouseSelection = m_selection;
            m_selection_pressedx = p.x();
            setCursor(Qt::SplitHCursor);
        }
        else {
            QPoint posglobal = mapToGlobal(event->pos()+QPoint(0,0));
            m_contextmenu.exec(posglobal);
        }
    }

    QGraphicsView::mousePressEvent(event);
    // std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVWaveform::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseMoveEvent" << m_selection.width() << endl;

    QPointF p = mapToScene(event->pos());

    cursorUpdate(p.x());

    if(m_currentAction==CAMoving) {
        // When scrolling the waveform
        cursorUpdate(-1);
        QGraphicsView::mouseMoveEvent(event);
        m_scene->invalidate();
    }
    else if(m_currentAction==CAZooming) {
        double dx = -(event->pos()-m_pressed_mouseinviewport).x()/100.0;

        QRectF newrect = m_pressed_scenerect;

        if(newrect.width()*exp(dx)<(10*1.0/WMainWindow::getMW()->getFs()))
            dx = log((10*1.0/WMainWindow::getMW()->getFs())/newrect.width());

        newrect.setLeft(m_selection_pressedx-(m_selection_pressedx-m_pressed_scenerect.left())*exp(dx));
        newrect.setRight(m_selection_pressedx+(m_pressed_scenerect.right()-m_selection_pressedx)*exp(dx));
        viewSet(newrect);

        QPointF p = mapToScene(event->pos());
        cursorUpdate(p.x());

        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
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
    else if(m_currentAction==CAStretchSelection) {
        // Stretch the selection from its center
        m_mouseSelection.setLeft(m_mouseSelection.left()-(p.x()-m_selection_pressedx));
        m_mouseSelection.setRight(m_mouseSelection.right()+(p.x()-m_selection_pressedx));
        selectionClipAndSet(m_mouseSelection);
        m_selection_pressedx = p.x();
    }
    else if(m_currentAction==CAWaveformScale){
        // Scale the selected waveform
        FTSound* currentftsound = WMainWindow::getMW()->getCurrentFTSound();
        if(currentftsound){
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                m_currentAction = CANothing;
            }
            else {
                currentftsound->m_ampscale *= pow(10, -10*(p.y()-m_selection_pressedx)/20.0);
                m_selection_pressedx = p.y();

                if(currentftsound->m_ampscale>1e10) currentftsound->m_ampscale = 1e10;
                else if(currentftsound->m_ampscale<1e-10) currentftsound->m_ampscale = 1e-10;

                currentftsound->setStatus();

                WMainWindow::getMW()->fileInfoUpdate();
                soundsChanged();
                WMainWindow::getMW()->m_gvSpectrum->soundsChanged();
            }
        }
    }
    else if(m_currentAction==CAWaveformDelay){
        FTSound* currentftsound = WMainWindow::getMW()->getCurrentFTSound();
        if(currentftsound){
            // Check first if the user is not trying to modify a hidden file
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                m_currentAction = CANothing;
            }
            else {
                m_tmpdelay += p.x()-m_selection_pressedx;
                m_selection_pressedx = p.x();
                currentftsound->m_delay = int(0.5+m_tmpdelay*WMainWindow::getMW()->getFs());
                if(m_tmpdelay<0) currentftsound->m_delay--;

                currentftsound->setStatus();

                WMainWindow::getMW()->fileInfoUpdate();
                soundsChanged();
                WMainWindow::getMW()->m_gvSpectrum->soundsChanged();
            }
        }
    }
    else if(m_currentAction==CALabelWritting) {
        FTLabels* ftlabel = WMainWindow::getMW()->getCurrentFTLabels();
        if(ftlabel) {
            m_label_current = std::deque<QString>::iterator();
            ftlabel->sort();
            m_currentAction = CANothing;
        }
    }
    else if(m_currentAction==CALabelModifPosition) {
        FTLabels* ftlabel = WMainWindow::getMW()->getCurrentFTLabels();
        if(ftlabel) {
            ftlabel->starts[m_ca_pressed_index] = p.x();
            ftlabel->m_isedited = true;
            ftlabel->setStatus();
            cursorUpdate(p.x());
            WMainWindow::getMW()->m_gvSpectrogram->m_scene->update();
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
        else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){

            // Check if a marker is close and show the horiz split cursor if true
            bool foundclosemarker = false;
            FTLabels* ftl = WMainWindow::getMW()->getCurrentFTLabels();
            if(ftl) {
                for(size_t lli=0; !foundclosemarker && lli<ftl->labels.size(); lli++) {
                    // cout << ftl->labels[lli].toLatin1().data() << ": " << ftl->starts[lli] << endl;
                    QPoint slp = mapFromScene(QPointF(ftl->starts[lli],0));
                    foundclosemarker = std::abs(slp.x()-event->x())<5;
                }
            }
            if(foundclosemarker)     setCursor(Qt::SplitHCursor);
            else                     setCursor(Qt::CrossCursor);

            if(event->modifiers().testFlag(Qt::ShiftModifier)){
                if(!foundclosemarker)
                    setCursor(Qt::SizeHorCursor);
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(!foundclosemarker)
                    setCursor(Qt::SizeVerCursor);
            }
        }

        QGraphicsView::mouseMoveEvent(event);
    }

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
}

void QGVWaveform::mouseReleaseEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mouseReleaseEvent " << m_selection.width() << endl;

    // Order the mouse selection to avoid negative width
    if(m_mouseSelection.right()<m_mouseSelection.left()){
        float tmp = m_mouseSelection.left();
        m_mouseSelection.setLeft(m_mouseSelection.right());
        m_mouseSelection.setRight(tmp);
    }

    if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()){
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
    else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
        }
        else{
            setCursor(Qt::SizeVerCursor);
            if(m_currentAction==CALabelModifPosition) {
                FTLabels* ftlabel = WMainWindow::getMW()->getCurrentFTLabels();
                if(ftlabel)
                    ftlabel->sort();
            }
        }
    }

    m_currentAction = CANothing;

    if(abs(m_selection.width())==0)
        selectionClear();
    else{
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void QGVWaveform::mouseDoubleClickEvent(QMouseEvent* event){

    QPointF p = mapToScene(event->pos());

    if(WMainWindow::getMW()->ui->actionSelectionMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
            selectSegment(p.x(), (m_selection.size().width()>0));
        }
        else{
            selectSegment(p.x(), false);
        }
    }
    else if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){
    }

}

void QGVWaveform::selectSegment(double x, bool add){
    // Check if a marker is close and show the horiz split cursor if true
    FTLabels* ftl = WMainWindow::getMW()->getCurrentFTLabels(true);
    if(ftl) {
        if(ftl->starts.size()>0){
            if(!add){
                if(x<ftl->starts[0]){
                    selectionClipAndSet(QRectF(0.0, -1.0, ftl->starts[0], 2.0), true);
                }
                else if(x>ftl->starts.back()){
                    selectionClipAndSet(QRectF(ftl->starts.back(), -1.0, WMainWindow::getMW()->getMaxLastSampleTime()-ftl->starts.back(), 2.0), true);
                }
                else{
                    int index = -1;
                    for(size_t lli=0; lli<ftl->labels.size(); lli++)
                        if(x>ftl->starts[lli])
                            index = lli;
                    selectionClipAndSet(QRectF(ftl->starts[index], -1.0, ftl->starts[index+1]-ftl->starts[index], 2.0), true);
                }
            }
            else{
                if(x<m_selection.left()){
                    if(x<ftl->starts[0]){
                        selectionClipAndSet(QRectF(0.0, -1.0, m_selection.right(), 2.0), true);
                    }
                    else{
                        int index = -1;
                        for(size_t lli=0; lli<ftl->labels.size(); lli++)
                            if(x>ftl->starts[lli])
                                index = lli;
                        selectionClipAndSet(QRectF(ftl->starts[index], -1.0, m_selection.right()-ftl->starts[index], 2.0), true);
                    }
                }
                else if(x>m_selection.right()){
                    if(x>ftl->starts.back()){
                        selectionClipAndSet(QRectF(m_selection.left(), -1.0, WMainWindow::getMW()->getMaxLastSampleTime()-m_selection.left(), 2.0), true);
                    }
                    else{
                        int index = -1;
                        for(size_t lli=0; lli<ftl->labels.size(); lli++)
                            if(x>ftl->starts[lli])
                                index = lli;
                        selectionClipAndSet(QRectF(m_selection.left(), -1.0, ftl->starts[index+1]-m_selection.left(), 2.0), true);
                    }
                }
            }
        }
    }
}

void QGVWaveform::keyPressEvent(QKeyEvent* event){

//    cout << "QGVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape) {
        selectionClear();
        playCursorSet(0.0);
    }
    else if(event->key()==Qt::Key_Delete) {
        if(m_currentAction==CALabelModifPosition) {
            FTLabels* ftlabel = WMainWindow::getMW()->getCurrentFTLabels(false);
            if(ftlabel){

                ftlabel->remove(m_ca_pressed_index);
                ftlabel->sort();
                m_currentAction = CANothing;
                m_scene->update();
                WMainWindow::getMW()->m_gvSpectrogram->m_scene->update();
            }
        }
    }
    else {
        if (WMainWindow::getMW()->ui->actionEditMode->isChecked()){
            FTLabels* ftlabel = WMainWindow::getMW()->getCurrentFTLabels(false);
            if(ftlabel){
                if(event->text().size()>0){
                    if(m_currentAction==CALabelWritting && m_label_current!=std::deque<QString>::iterator())
                        m_label_current->append(event->text());
                    else{
                        m_currentAction = CALabelWritting;
                        ftlabel->labels.push_back(event->text());
                        ftlabel->starts.push_back(m_giCursor->line().x1());
                        ftlabel->m_isedited = true;
                        m_label_current = ftlabel->labels.end()-1;
                    }
                    ftlabel->setStatus();
                    m_scene->update();
                    WMainWindow::getMW()->m_gvSpectrogram->m_scene->update();
                }
            }
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void QGVWaveform::selectionClear(){
    m_giSelection->hide();
    m_selection = QRectF(0, -1, 0, 2);
    m_giSelection->setRect(m_selection.left(), -1, m_selection.width(), 2);
    m_aZoomOnSelection->setEnabled(false);
    WMainWindow::getMW()->ui->lblSelectionTxt->setText("No selection");
    m_aSelectionClear->setEnabled(false);
    WMainWindow::getMW()->m_gvSpectrum->m_scene->update();
    WMainWindow::getMW()->m_gvPhaseSpectrum->m_scene->update();
}

void QGVWaveform::selectionClipAndSet(QRectF selection, bool winforceupdate){
    // m_giMouseSelection->setRect(selection);

    double fs = WMainWindow::getMW()->getFs();

    // Order the selection to avoid negative width
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }

    size_t prevwinlen = 0;
    if(WMainWindow::getMW()->m_gvSpectrum)
        prevwinlen = WMainWindow::getMW()->m_gvSpectrum->m_win.size();
    m_selection = selection;

    // Clip selection on exact sample times
    m_selection.setLeft((int(0.5+m_selection.left()*fs))/fs);
    if(selection.width()==0)
        m_selection.setRight(m_selection.left());
    else
        m_selection.setRight((int(0.5+m_selection.right()*fs))/fs);

    if(m_selection.left()<0) m_selection.setLeft(0.0);
    if(m_selection.left()>WMainWindow::getMW()->getMaxLastSampleTime()-1.0/fs) m_selection.setLeft(WMainWindow::getMW()->getMaxLastSampleTime()-1.0/fs);
    if(m_selection.right()<1.0/fs) m_selection.setRight(1.0/fs);
    if(m_selection.right()>WMainWindow::getMW()->getMaxLastSampleTime()) m_selection.setRight(WMainWindow::getMW()->getMaxLastSampleTime());

    // Adjust parity of the window size according to option
    int nl = std::max(0, int(0.5+m_selection.left()*fs));
    int nr = int(0.5+std::min(WMainWindow::getMW()->getMaxLastSampleTime(),m_selection.right())*fs);
    int winlen = nr-nl+1;
    if(winlen%2==0 && WMainWindow::getMW()->m_gvSpectrum->m_dlgSettings->ui->cbWindowSizeForcedOdd->isChecked()) {
        if(m_currentAction==CAModifSelectionLeft)
            m_selection.setLeft(m_selection.left()+1.0/fs);
        else if(m_currentAction==CAModifSelectionRight)
            m_selection.setRight(m_selection.right()-1.0/fs);
        else {
//            cout << "here ? " << m_mouseSelection.left() << " " << m_mouseSelection.right() << endl;
            if(m_mouseSelection.right()>m_mouseSelection.left())
                m_selection.setRight(m_selection.right()-1.0/fs);
            else
                m_selection.setLeft(m_selection.left()+1.0/fs);
        }
    }

    // Set the visible selection encompassing the actual selection
    m_giSelection->setRect(m_selection.left()-0.5/fs, -1, m_selection.width()+1.0/fs, 2);

    // WMainWindow::getMW()->ui->lblSelectionTxt->setText(QString("[%1").arg(m_selection.left()).append(",%1] ").arg(m_selection.right()).append("%1 s").arg(m_selection.width())); // start, end and duration
    WMainWindow::getMW()->ui->lblSelectionTxt->setText(QString("%1s selection ").arg(m_selection.width(), 0,'f',4).append(" at %1s").arg(m_selection.center().x(), 0,'f',4)); // duration and center

    m_giSelection->show();

    playCursorSet(m_selection.left()); // Put the play cursor

    if(m_selection.width()>0){
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }
    else{
        m_aZoomOnSelection->setEnabled(false);
        m_aSelectionClear->setEnabled(false);
    }

    // The selection's width can vary up to eps, even when it is only shifted

    // Spectrum
    WMainWindow::getMW()->m_gvSpectrum->setWindowRange(m_selection.left(), m_selection.right(), winforceupdate);

    // Update the visible window
    if(WMainWindow::getMW()->m_gvSpectrum->m_win.size()>0 && (winforceupdate || WMainWindow::getMW()->m_gvSpectrum->m_win.size() != prevwinlen)) {

        qreal winmax = 0.0;
        for(size_t n=0; n<WMainWindow::getMW()->m_gvSpectrum->m_win.size(); n++)
            winmax = std::max(winmax, WMainWindow::getMW()->m_gvSpectrum->m_win[n]);
        winmax = 1.0/winmax;

        QPainterPath path;

        qreal prevx = 0;
        qreal prevy = WMainWindow::getMW()->m_gvSpectrum->m_win[0]*winmax;
        path.moveTo(QPointF(prevx, -prevy));
        qreal y;
        for(size_t n=1; n<WMainWindow::getMW()->m_gvSpectrum->m_win.size(); n++) {
            qreal x = n/fs;
            y = WMainWindow::getMW()->m_gvSpectrum->m_win[n];
            y *= winmax;
            path.lineTo(QPointF(x, -y));
            prevx = x;
            prevy = y;
        }

        // Add the vertical line
        qreal x = ((WMainWindow::getMW()->m_gvSpectrum->m_win.size()-1)/2.0)/fs;
        path.moveTo(QPointF(x, 2.0));
        path.lineTo(QPointF(x, -1.0));

        m_giWindow->setPath(path);
    }
    m_giWindow->setPos(QPointF(m_selection.left(), 0));
    m_giWindow->show();
}

void QGVWaveform::selectionZoomOn(){
    if(m_selection.width()>0){

        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        viewrect.setLeft(m_selection.left()-0.1*m_selection.width());
        viewrect.setRight(m_selection.right()+0.1*m_selection.width());
        viewSet(viewrect);

        m_aZoomIn->setEnabled(true);
        m_aZoomOut->setEnabled(true);
        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(false);

        cursorUpdate(-1);
    }
}

//class QTimeTracker2 : public QTime {
//public:
//    QTimeTracker2(){
//    }

//    std::deque<int> past_elapsed;

//    float getAveragedElapsed(){
//        float m = 0;
//        for(unsigned int i=0; i<past_elapsed.size(); i++){
//            m += past_elapsed[i];
//        }
//        return m / past_elapsed.size();
//    }

//    void storeElapsed(){
//        past_elapsed.push_back(elapsed());
//        if (past_elapsed.size()>10000)
//            past_elapsed.pop_front();
//        restart();
//    }
//};

void QGVWaveform::drawBackground(QPainter* painter, const QRectF& rect){

//    static QTimeTracker2 time_tracker;
//    time_tracker.restart();

    if(m_aShowGrid->isChecked())
        draw_grid(painter, rect);

    draw_waveform(painter, rect);

    m_giWindow->setVisible(m_aShowWindow->isChecked() && m_selection.width()>0.0);

//    time_tracker.storeElapsed();

//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGraphicsView::drawBackground [" << rect.left() << "," << rect.right() << "]x[" << rect.top() << "," << rect.bottom() << "] avg=" << time_tracker.getAveragedElapsed() << "(" << time_tracker.past_elapsed.size() << ") last=" << time_tracker.past_elapsed.back() << endl;
}

void QGVWaveform::draw_waveform(QPainter* painter, const QRectF& rect){

//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVWaveform::draw_waveform [" << rect.width() << "]" << endl;

    double fs = WMainWindow::getMW()->getFs();

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    double samppixdensity = (viewrect.right()-viewrect.left())*fs/viewport()->rect().width();

//    samppixdensity=0.01;
    if(samppixdensity<4) {
//         cout << "Draw lines between each sample in the updated rect" << endl;

        for(size_t fi=0; fi<WMainWindow::getMW()->ftsnds.size(); fi++){
            if(!WMainWindow::getMW()->ftsnds[fi]->m_actionShow->isChecked())
                continue;

            WAVTYPE a = WMainWindow::getMW()->ftsnds[fi]->m_ampscale;
            if(WMainWindow::getMW()->ftsnds[fi]->m_actionInvPolarity->isChecked())
                a *=-1.0;

            QPen wavpen(WMainWindow::getMW()->ftsnds[fi]->color);
            wavpen.setWidth(0);
            painter->setPen(wavpen);

            int delay = WMainWindow::getMW()->ftsnds[fi]->m_delay;
            int nleft = int((rect.left())*fs)-delay;
            int nright = int((rect.right())*fs)+1-delay;
            nleft = std::max(nleft, -delay);
            nleft = std::max(nleft, 0);
            nleft = std::min(nleft, int(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1)-delay);
            nleft = std::min(nleft, int(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1));
            nright = std::max(nright, -delay);
            nright = std::max(nright, 0);
            nright = std::min(nright, int(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1)-delay);
            nright = std::min(nright, int(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1));

            // Draw a line between each sample value
            WAVTYPE dt = 1.0/fs;
            WAVTYPE prevx = (nleft+delay)*dt;
            a *= -1;
            WAVTYPE* data = WMainWindow::getMW()->ftsnds[fi]->wavtoplay->data();
            WAVTYPE prevy = a*(*(data+nleft));

            WAVTYPE x, y;
            for(int n=nleft; n<=nright; ++n){
                x = (n+delay)*dt;
                y = a*(*(data+n));

                painter->drawLine(QLineF(prevx, prevy, x, y));

                prevx = x;
                prevy = y;
            }

            // When resolution is big enough, draw tick marks at each sample
            double samppixdensity_dotsthr = 0.125;
            if(samppixdensity<samppixdensity_dotsthr){
                qreal markhalfheight = m_ampzoom*(1.0/10)*((samppixdensity_dotsthr-samppixdensity)/samppixdensity_dotsthr);

                for(int n=nleft; n<=nright; n++){
                    x = (n+delay)*dt;
                    y = a*(*(data+n));
                    painter->drawLine(QLineF(x, y-markhalfheight, x, y+markhalfheight));
                }
            }

            painter->drawLine(QLineF(double(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1)/fs, -1.0, double(WMainWindow::getMW()->ftsnds[fi]->wav.size()-1)/fs, 1.0));
        }
    }
    else {
//        cout << "Plot only one line per pixel, in order to reduce computation time" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        for(size_t fi=0; fi<WMainWindow::getMW()->ftsnds.size(); fi++){
            if(!WMainWindow::getMW()->ftsnds[fi]->m_actionShow->isChecked())
                continue;

            double a = WMainWindow::getMW()->ftsnds[fi]->m_ampscale;
            if(WMainWindow::getMW()->ftsnds[fi]->m_actionInvPolarity->isChecked())
                a *= -1.0;

            QPen outlinePen(WMainWindow::getMW()->ftsnds[fi]->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);

            QRect pixrect = mapFromScene(rect).boundingRect();
            QRect fullpixrect = mapFromScene(viewrect).boundingRect();

            double s2p = -a*fullpixrect.height()/viewrect.height(); // Scene to pixel
            double p2n = fs*double(viewrect.width())/double(fullpixrect.width()-1); // Pixel to scene
            double yzero = fullpixrect.height()/2;

            WAVTYPE* yp = WMainWindow::getMW()->ftsnds[fi]->wavtoplay->data();

            int winpixdelay = horizontalScrollBar()->value(); // - 1.0/p2n; // The magic value to make everything plot at the same place whatever the scroll

            int snddelay = WMainWindow::getMW()->ftsnds[fi]->m_delay;
            int ns = int((pixrect.left()+winpixdelay)*p2n)-snddelay;
            for(int i=pixrect.left(); i<=pixrect.right(); i++) {

                int ne = int((i+1+winpixdelay)*p2n)-snddelay;

                if(ns>=0 && ne<int(WMainWindow::getMW()->ftsnds[fi]->wav.size())) {
                    WAVTYPE ymin = 1.0;
                    WAVTYPE ymax = -1.0;
                    WAVTYPE* ypp = yp+ns;
                    WAVTYPE y;
                    for(int n=ns; n<=ne; n++) {
                        y = *ypp;
                        ymin = std::min(ymin, y);
                        ymax = std::max(ymax, y);
                        ypp++;
                    }
                    ymin *= s2p;
                    ymax *= s2p;
                    ymin = int(ymin+0.5);
                    ymax = int(ymax+0.5);
                    painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
                }

                ns = ne;
            }
        }

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }

    // Plot STFT's window centers
    if(m_aShowSTFTWindowCenters->isChecked()){
        FTSound* currentftsound = WMainWindow::getMW()->getCurrentFTSound();
        if(currentftsound && currentftsound->m_actionShow->isChecked()){
            QPen outlinePen(currentftsound->color);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
//            painter->setBrush(QBrush(WMainWindow::getMW()->ftlabels[fi]->color));

            for(size_t wci=0; wci<currentftsound->m_stftts.size(); wci++){
                painter->drawLine(QLineF(currentftsound->m_stftts[wci], -1.0, currentftsound->m_stftts[wci], 1.0));
            }
        }
    }

    // Plot labels
    QTransform trans = transform();
    for(size_t fi=0; fi<WMainWindow::getMW()->ftlabels.size(); fi++){
        if(!WMainWindow::getMW()->ftlabels[fi]->m_actionShow->isChecked())
            continue;

        QPen outlinePen(WMainWindow::getMW()->ftlabels[fi]->color);
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setBrush(QBrush(WMainWindow::getMW()->ftlabels[fi]->color));

//        cout << WMainWindow::getMW()->ftlabels[fi]->starts.size() << " " << WMainWindow::getMW()->ftlabels[fi]->ends.size() << endl;

        for(size_t li=0; li<WMainWindow::getMW()->ftlabels[fi]->starts.size(); li++){
            // TODO plot only labels which can be seen
            double start = WMainWindow::getMW()->ftlabels[fi]->starts[li];
            painter->drawLine(QLineF(start, -1.0, start, 1.0));

            painter->save();
            painter->translate(QPointF(start+2.0/trans.m11(), viewrect.top()+12/trans.m22()));
            painter->scale(1.0/trans.m11(), 1.0/trans.m22());
            painter->drawStaticText(QPointF(0, 0), QStaticText(WMainWindow::getMW()->ftlabels[fi]->labels[li]));
            painter->restore();
        }
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
        while(int(float(viewrect.height())/lstep)<4){ // TODO use log2
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

void QGVWaveform::playCursorUpdate() {
    QTransform trans = transform();
    QTransform cursortrans = QTransform::fromScale(1.0/trans.m11(), 1.0);
    m_giPlayCursor->setTransform(cursortrans);
}

void QGVWaveform::playCursorSet(double t){
    if(t==-1){
        if(m_selection.width()>0)
            playCursorSet(m_selection.left());
        else
            m_giPlayCursor->setPos(QPointF(m_initialPlayPosition, 0));
    }
    else{
        m_giPlayCursor->setPos(QPointF(t, 0));
        m_giPlayCursor->show();
        playCursorUpdate();
    }
}
