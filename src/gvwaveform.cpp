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

#include <QtGlobal>
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

#include "qthelper.h"

QGVWaveform::QGVWaveform(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_ftlabel_current_index(-1)
    , m_scrolledx(0)
    , m_ampzoom(1.0)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // Grid - Prepare the pens and fonts
    m_gridPen.setColor(QColor(192,192,192));
    m_gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    m_aWaveformShowGrid = new QAction(tr("Show &grid"), this);
    m_aWaveformShowGrid->setObjectName("m_aWaveformShowGrid");
    m_aWaveformShowGrid->setStatusTip(tr("Show &grid"));
    m_aWaveformShowGrid->setCheckable(true);
    m_aWaveformShowGrid->setChecked(true);
    m_aWaveformShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aWaveformShowGrid);
    connect(m_aWaveformShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(update()));
    m_contextmenu.addAction(m_aWaveformShowGrid);
    m_aWaveformShowSelectedWaveformOnTop = new QAction(tr("Show selected wav on top"), this);
    m_aWaveformShowSelectedWaveformOnTop->setObjectName("m_aWaveformShowSelectedWaveformOnTop");
    m_aWaveformShowSelectedWaveformOnTop->setStatusTip(tr("Show the selected waveform on top of all others"));
    m_aWaveformShowSelectedWaveformOnTop->setCheckable(true);
    m_aWaveformShowSelectedWaveformOnTop->setChecked(true);
//    gMW->m_settings.add(m_aWaveformShowSelectedWaveformOnTop);
    connect(m_aWaveformShowSelectedWaveformOnTop, SIGNAL(triggered()), m_scene, SLOT(update()));
//    m_contextmenu.addAction(m_aShowSelectedWaveformOnTop);

    // Mouse Cursor
    m_giMouseCursorLine = new QGraphicsLineItem(0, -1, 0, 1);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giMouseCursorLine->setPen(cursorPen);
    m_scene->addItem(m_giMouseCursorLine);
    m_giMouseCursorTxt = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxt->setBrush(QColor(64, 64, 64));
    QFont font("Helvetica", 10);
    m_giMouseCursorTxt->setFont(font);
    m_scene->addItem(m_giMouseCursorTxt);

    // Selection
    m_currentAction = CANothing;
    m_giSelection = new QGraphicsRectItem();
    m_giSelection->hide();
    QPen selectionPen(QColor(32, 32, 32));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giSelection->setPen(selectionPen);
    m_giSelection->setBrush(selectionBrush);
    m_giSelection->setOpacity(0.33);
    // QPen mouseselectionPen(QColor(255, 0, 0));
    // mouseselectionPen.setWidth(0);
    // m_giMouseSelection = new QGraphicsRectItem();
    // m_giMouseSelection->setPen(mouseselectionPen);
    // m_scene->addItem(m_giMouseSelection);
    m_mouseSelection = QRectF(0, -1, 0, 2);
    m_selection = m_mouseSelection;
    m_scene->addItem(m_giSelection);
    gMW->ui->lblSelectionTxt->setText("No selection");
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
    m_aWaveformShowWindow = new QAction(tr("Show DFT's window"), this);
    m_aWaveformShowWindow->setObjectName("m_aWaveformShowWindow");
    m_aWaveformShowWindow->setStatusTip(tr("Show DFT's window"));
    m_aWaveformShowWindow->setCheckable(true);
    m_aWaveformShowWindow->setIcon(QIcon(":/icons/window.svg"));
    gMW->m_settings.add(m_aWaveformShowWindow);
    connect(m_aWaveformShowWindow, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    connect(m_aWaveformShowWindow, SIGNAL(toggled(bool)), this, SLOT(updateSelectionText()));
    m_contextmenu.addAction(m_aWaveformShowWindow);
    m_aWaveformShowSTFTWindowCenters = new QAction(tr("Show STFT's window centers"), this);
    m_aWaveformShowSTFTWindowCenters->setObjectName("m_aWaveformShowSTFTWindowCenters");
    m_aWaveformShowSTFTWindowCenters->setStatusTip(tr("Show STFT's window centers"));
    m_aWaveformShowSTFTWindowCenters->setCheckable(true);
    m_aWaveformShowSTFTWindowCenters->setChecked(false);
    gMW->m_settings.add(m_aWaveformShowSTFTWindowCenters);
    connect(m_aWaveformShowSTFTWindowCenters, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));
    m_contextmenu.addAction(m_aWaveformShowSTFTWindowCenters);

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
    playCursorSet(0.0, false);
    m_scene->addItem(m_giPlayCursor);

    showScrollBars(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked());
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), this, SLOT(showScrollBars(bool)));
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
    //m_aZoomOnSelection->setShortcut(Qt::Key_S); // This one creates "ambiguous" shortcuts
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
    connect(gMW->ui->btnFitViewToSoundsAmplitude, SIGNAL(clicked()), m_aFitViewToSoundsAmplitude, SLOT(trigger()));
    gMW->ui->btnFitViewToSoundsAmplitude->setIcon(QIcon(":/icons/unzoomy.svg"));
    connect(m_aFitViewToSoundsAmplitude, SIGNAL(triggered()), this, SLOT(fitViewToSoundsAmplitude()));

    connect(gMW->ui->sldWaveformAmplitude, SIGNAL(valueChanged(int)), this, SLOT(sldAmplitudeChanged(int)));

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
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(),gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
//    gMW->ui->lWaveformToolBar->addWidget(m_toolBar);
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lWaveformToolBar->addWidget(m_toolBar);

    setMouseCursorPosition(-1, false);
}

void QGVWaveform::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        verticalScrollBar()->setEnabled(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void QGVWaveform::fitViewToSoundsAmplitude(){
    if(gMW->ftsnds.size()>0){
        WAVTYPE maxwavmaxamp = 0.0;
        for(unsigned int si=0; si<gMW->ftsnds.size(); si++)
            if(gMW->ftsnds[si]->isVisible())
                maxwavmaxamp = std::max(maxwavmaxamp, gMW->ftsnds[si]->m_ampscale*gMW->ftsnds[si]->m_wavmaxamp);

        if(maxwavmaxamp==0.0)
            maxwavmaxamp = 1.0;

        // Add a small margin
        maxwavmaxamp *= 1.05;

        gMW->ui->sldWaveformAmplitude->setValue(100-(maxwavmaxamp*100.0));
    }
}

void QGVWaveform::updateSceneRect() {
    m_scene->setSceneRect(-1.0/gMW->getFs(), -1.05*m_ampzoom, gMW->getMaxDuration()+1.0/gMW->getFs(), 2.1*m_ampzoom);
//    updateTextsGeometry();
}

void QGVWaveform::viewSet(QRectF viewrect, bool sync) {

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

//    cout << "QGVWaveform::viewSet: viewrect=" << viewrect << endl;
//    cout << "QGVWaveform::viewSet: currentviewrect=" << currentviewrect << endl;

    if(viewrect!=currentviewrect) {
        if(viewrect.top()<m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top()-0.01);
        if(viewrect.bottom()>m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom()+0.01);
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        fitInView(removeHiddenMargin(this, viewrect));

        updateTextsGeometry();

        if(sync){
            if(gMW->m_gvSpectrogram && gMW->ui->actionShowSpectrogram->isChecked()) {
                QRectF spectrorect = gMW->m_gvSpectrogram->mapToScene(gMW->m_gvSpectrogram->viewport()->rect()).boundingRect();
                spectrorect.setLeft(viewrect.left());
                spectrorect.setRight(viewrect.right());
                gMW->m_gvSpectrogram->viewSet(spectrorect, false);
            }
        }
    }
}

void QGVWaveform::sldAmplitudeChanged(int value){

    m_ampzoom = (100-value)/100.0;

    updateSceneRect();
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setTop(-1.05*m_ampzoom);
    viewrect.setBottom(1.05*m_ampzoom);
    viewSet(viewrect, false);

    setMouseCursorPosition(-1, false);

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

    setMouseCursorPosition(-1, false);

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

    setMouseCursorPosition(-1, false);

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

    setMouseCursorPosition(-1, false);

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(false);
    m_aUnZoom->setEnabled(false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void QGVWaveform::setMouseCursorPosition(double position, bool forwardsync) {
    if(position==-1){
        m_giMouseCursorLine->hide();
        m_giMouseCursorTxt->hide();
    }
    else {
        m_giMouseCursorLine->show();
        m_giMouseCursorTxt->show();

        m_giMouseCursorLine->setPos(position, 0.0);

        QString txt = QString("%1s").arg(position, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value());
        m_giMouseCursorTxt->setText(txt);

        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
        m_giMouseCursorTxt->setTransform(txttrans);
        m_giMouseCursorTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        QRectF br = m_giMouseCursorTxt->boundingRect();
//        QRectF viewrect = mapToScene(QRect(QPoint(0,0), QSize(viewport()->rect().width(), viewport()->rect().height()))).boundingRect();
        position = min(position, double(viewrect.right()-br.width()/trans.m11()));
//        if(x+br.width()/trans.m11()>viewrect.right())
//            x = x - br.width()/trans.m11();
        m_giMouseCursorTxt->setPos(position+1/trans.m11(), viewrect.top()-3/trans.m22());

        if(forwardsync){
            if(gMW->m_gvSpectrogram)
                gMW->m_gvSpectrogram->setMouseCursorPosition(QPointF(position, 0.0), false);
        }
    }
}

void QGVWaveform::resizeEvent(QResizeEvent* event){
//    COUTD << "QGVWaveform::resizeEvent oldSize: " << event->oldSize().isEmpty() << endl;

    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {
        // The view didn't exist (or was hidden?) and it becomes visible.

        updateSceneRect();

        if(gMW->m_gvSpectrogram->viewport()->rect().width()*gMW->m_gvSpectrogram->viewport()->rect().height()>0){
            QRectF spectrorect = gMW->m_gvSpectrogram->mapToScene(gMW->m_gvSpectrogram->viewport()->rect()).boundingRect();

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
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
//        m_force_redraw = event->oldSize().width()!=event->size().width();
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    setMouseCursorPosition(-1, false);
}

void QGVWaveform::scrollContentsBy(int dx, int dy){
//    COUTD << "QGVWaveform::scrollContentsBy " << dx << "," << dy << endl;

    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    // TODO SPEEDUP This one is really a problem because, when scrolling right, it invalidates basically all
    //      the view (the invalidated rect on the right merge with the invalidated rect below).
    m_scene->invalidate(QRectF(viewrect.left(), -1.05, 5*14/transform().m11(), 2.10));

    setMouseCursorPosition(-1, false);

    m_scrolledx += dx;

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVWaveform::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    std::cout << "QGVWaveform::wheelEvent " << numDegrees.y() << endl;

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

//    cout << "QGVWaveform::wheelEvent: " << viewrect << endl;

    if(event->modifiers().testFlag(Qt::ShiftModifier)){
        QScrollBar* sb = horizontalScrollBar();
        sb->setValue(sb->value()-numDegrees.y());
    }
    else if((viewrect.width()>10.0/gMW->getFs() && numDegrees.y()>0) || numDegrees.y()<0) {
        double g = double(mapToScene(event->pos()).x()-viewrect.left())/viewrect.width();
        QRectF newrect = mapToScene(viewport()->rect()).boundingRect();
        newrect.setLeft(newrect.left()+g*0.01*viewrect.width()*numDegrees.y());
        newrect.setRight(newrect.right()-(1-g)*0.01*viewrect.width()*numDegrees.y());
        if(newrect.width()<10.0/gMW->getFs()){
           newrect.setLeft(newrect.center().x()-5.0/gMW->getFs());
           newrect.setRight(newrect.center().x()+5.0/gMW->getFs());
        }

        viewSet(newrect);

//        QPointF p = mapToScene(QPoint(event->x(),0));
//        setMouseCursorPosition(p.x(), true);
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
    QRect selview = mapFromScene(m_giSelection->boundingRect()).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(event->modifiers().testFlag(Qt::ShiftModifier)) {
            // cout << "Scrolling the waveform" << endl;
            m_currentAction = CAMoving;
            setMouseCursorPosition(-1, false);
        }
        else {
            if(gMW->ui->actionSelectionMode->isChecked()){

                if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.left()-event->x())<5){
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
                else if(m_selection.width()>0 && (event->modifiers().testFlag(Qt::ControlModifier) || (event->x()>=selview.left() && event->x()<=selview.right()))){
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
                    selectionSet(m_mouseSelection);
                    m_giSelection->show();
                }
            }
            else if(gMW->ui->actionEditMode->isChecked()){

                // Look for a nearby marker to modify
                m_ca_pressed_index=-1;
                FTLabels* ftl = gMW->getCurrentFTLabels();
                if(ftl){
                    for(int lli=0; m_ca_pressed_index==-1 && lli<int(ftl->starts.size()); lli++) {
                        QPoint slp = mapFromScene(QPointF(ftl->starts[lli],0));
                        if(std::abs(slp.x()-event->x())<5) {
                            m_ca_pressed_index = lli;
                            m_currentAction = CALabelModifPosition;
                        }
                    }
                }

                if(m_ca_pressed_index==-1) {
                    if(event->modifiers().testFlag(Qt::ShiftModifier)){
                    }
                    else if(event->modifiers().testFlag(Qt::ControlModifier)){
                        // cout << "Scaling the waveform" << endl;
                        m_currentAction = CAWaveformDelay;
                        m_selection_pressedx = p.x();
                        FTSound* currentftsound = gMW->getCurrentFTSound();
                        if(currentftsound)
                            m_tmpdelay = currentftsound->m_delay/gMW->getFs();
                        setCursor(Qt::SizeHorCursor);
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

    setMouseCursorPosition(p.x(), true);

    if(m_currentAction==CAMoving) {
        // When scrolling along the waveform
        setMouseCursorPosition(-1, false);
        QGraphicsView::mouseMoveEvent(event);
    }
    else if(m_currentAction==CAZooming) {
        double dx = -(event->pos()-m_pressed_mouseinviewport).x()/100.0;

        QRectF newrect = m_pressed_scenerect;

        if(newrect.width()*exp(dx)<(10*1.0/gMW->getFs()))
            dx = log((10*1.0/gMW->getFs())/newrect.width());

        newrect.setLeft(m_selection_pressedx-(m_selection_pressedx-m_pressed_scenerect.left())*exp(dx));
        newrect.setRight(m_selection_pressedx+(m_pressed_scenerect.right()-m_selection_pressedx)*exp(dx));
        viewSet(newrect);

        QPointF p = mapToScene(event->pos());
        setMouseCursorPosition(p.x(), true);

        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedx);
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedx);
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedx, 0, p.x()-m_selection_pressedx, 0);
        selectionSet(m_mouseSelection);
        m_selection_pressedx = p.x();
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setLeft(m_selection_pressedx);
        m_mouseSelection.setRight(p.x());
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAStretchSelection) {
        // Stretch the selection from its center
        m_mouseSelection.setLeft(m_mouseSelection.left()-(p.x()-m_selection_pressedx));
        m_mouseSelection.setRight(m_mouseSelection.right()+(p.x()-m_selection_pressedx));
        selectionSet(m_mouseSelection);
        m_selection_pressedx = p.x();
    }
    else if(m_currentAction==CAWaveformScale){
        // Scale the selected waveform
        FTSound* currentftsound = gMW->getCurrentFTSound();
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

                currentftsound->needDFTUpdate();
                currentftsound->setStatus();

                m_scene->update();
                gMW->m_gvAmplitudeSpectrum->updateDFTs();
                gMW->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
            }
        }
    }
    else if(m_currentAction==CAWaveformDelay){
        FTSound* currentftsound = gMW->getCurrentFTSound();
        if(currentftsound){
            // Check first if the user is not trying to modify a hidden file
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                m_currentAction = CANothing;
            }
            else {
                m_tmpdelay += p.x()-m_selection_pressedx;
                m_selection_pressedx = p.x();
                currentftsound->m_delay = int(0.5+m_tmpdelay*gMW->getFs());
                if(m_tmpdelay<0) currentftsound->m_delay--;

                currentftsound->needDFTUpdate();
                currentftsound->setStatus();

                m_scene->update();
                gMW->m_gvAmplitudeSpectrum->updateDFTs();
                gMW->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
            }
        }
    }
    else if(m_currentAction==CALabelWritting) {
        FTLabels* ftlabel = gMW->getCurrentFTLabels();
        if(ftlabel) {
            m_ftlabel_current_index = -1;
            m_currentAction = CANothing;
        }
    }
    else if(m_currentAction==CALabelModifPosition) {
        FTLabels* ftlabel = gMW->getCurrentFTLabels();
        if(ftlabel) {
            ftlabel->moveLabel(m_ca_pressed_index, p.x());
            updateTextsGeometry();
        }
    }
    else{
        QRect selview = mapFromScene(m_giSelection->boundingRect()).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(m_selection.width()>0 && abs(selview.left()-event->x())<5)
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && abs(selview.right()-event->x())<5)
                    setCursor(Qt::SplitHCursor);
                else if(event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::OpenHandCursor);
                else
                    setCursor(Qt::CrossCursor);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){

            // Check if a marker is close and show the horiz split cursor if true
            bool foundclosemarker = false;
            FTLabels* ftl = gMW->getCurrentFTLabels();
            if(ftl) {
                for(int lli=0; !foundclosemarker && lli<ftl->getNbLabels(); lli++) {
                    // cout << ftl->labels[lli].toLatin1().data() << ": " << ftl->starts[lli] << endl;
                    QPoint slp = mapFromScene(QPointF(ftl->starts[lli],0));
                    foundclosemarker = std::abs(slp.x()-event->x())<5;
                }
            }
            if(foundclosemarker)     setCursor(Qt::SplitHCursor);
            else                     setCursor(Qt::CrossCursor);

            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
                if(!foundclosemarker)
                    setCursor(Qt::SizeHorCursor);
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

    if(gMW->ui->actionSelectionMode->isChecked()){
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
    else if(gMW->ui->actionEditMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
        }
        else{
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

//    if(gMW->ui->actionSelectionMode->isChecked()){
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)){
            if(m_selection.left()<=p.x() && p.x()<=m_selection.right())
                selectRemoveSegment(p.x());
            else
                selectSegment(p.x(), (m_selection.size().width()>0));
        }
        else{
            selectSegment(p.x(), false);
        }
//    }
//    else if(gMW->ui->actionEditMode->isChecked()){
//    }

}

void QGVWaveform::selectSegmentFindStartEnd(double x, FTLabels* ftl, double& start, double& end){
    start = -1;
    end = -1;
    if(ftl && ftl->getNbLabels()>0) {
        if(x<ftl->starts[0]){
            start = 0.0;
            end = ftl->starts[0];
        }
        else if(x>ftl->starts.back()){
            start = ftl->starts.back();
            end = gMW->getMaxLastSampleTime();
        }
        else{
            int index = -1;
            for(int lli=0; lli<ftl->getNbLabels(); lli++) // TODO Simplify and Speed up
                if(x>ftl->starts[lli])
                    index = lli;
            start = ftl->starts[index];
            end = ftl->starts[index+1];
        }
    }
}

void QGVWaveform::selectSegment(double x, bool add){
    // Check if a marker is close and show the horiz split cursor if true
    FTLabels* ftl = gMW->getCurrentFTLabels(true);
    if(ftl && ftl->getNbLabels()>0) {
        if(ftl->starts.size()>0){
            double start, end;
            selectSegmentFindStartEnd(x, ftl, start, end);
            if(add){
                start = std::min(m_selection.left(), start);
                end = std::max(m_selection.right(), end);
            }
            selectionSet(QRectF(start, -1.0, end-start, 2.0));
        }
    }
}

void QGVWaveform::selectRemoveSegment(double x){
    FTLabels* ftl = gMW->getCurrentFTLabels(true);
    if(ftl && ftl->getNbLabels()>0) {
        if(m_selection.width()>0 && m_selection.left()<=x && x<=m_selection.right()){
            double start, end;
            selectSegmentFindStartEnd(x, ftl, start, end);
            double playstart = start;

            if(x<m_selection.center().x()){ // if double-clicked on the left of the center, remove on the left
                start = std::max(end, m_selection.left());
                end = m_selection.right();

                if(end-start==0)
                    selectionClear();
                else
                    selectionSet(QRectF(start, -1.0, end-start, 2.0));
            }
            else{
                end = std::min(start, m_selection.right());
                start = m_selection.left();

                if(end-start==0)
                    selectionClear();
                else
                    selectionSet(QRectF(start, -1.0, end-start, 2.0));
            }

            playCursorSet(playstart, true);
        }
    }
}


void QGVWaveform::keyPressEvent(QKeyEvent* event){

//    COUTD << "QGVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Delete) {
        if(m_currentAction==CALabelModifPosition) {
            FTLabels* ftlabel = gMW->getCurrentFTLabels(false);
            if(ftlabel){
                ftlabel->removeLabel(m_ca_pressed_index);
                m_currentAction = CANothing;
            }
        }
    }
    else if(event->key()==Qt::Key_S){
        if(!FTGraphicsLabelItem::isEditing())
            selectionZoomOn();
    }
    else if(event->key()==Qt::Key_Escape) {
        if(!hasSelection()) {
            if(!gMW->m_gvSpectrogram->hasSelection()
                && !gMW->m_gvAmplitudeSpectrum->hasSelection())
                playCursorSet(0.0, true);

            gMW->m_gvAmplitudeSpectrum->selectionClear();
        }
        selectionClear(true);
    }
    else {
        if (gMW->ui->actionEditMode->isChecked()){
            FTLabels* ftlabel = gMW->getCurrentFTLabels(false);
            if(ftlabel && !FTGraphicsLabelItem::isEditing()){
                if(event->text().size()>0){
                    if(m_currentAction==CALabelWritting && m_ftlabel_current_index!=-1){
                        ftlabel->changeText(m_ftlabel_current_index, ftlabel->waveform_labels[m_ftlabel_current_index]->toPlainText()+event->text());
                    }
                    else{
                        m_currentAction = CALabelWritting;
                        ftlabel->addLabel(m_giMouseCursorLine->pos().x(), event->text());
                        m_ftlabel_current_index = ftlabel->getNbLabels()-1;
                        gMW->fileInfoUpdate();
                    }
                    updateTextsGeometry(); // TODO Could be avoided maybe
                }
            }
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void QGVWaveform::selectionClear(bool forwardsync){
    m_giSelection->hide();
    m_selection = QRectF(0, -1, 0, 2);
    m_giSelection->setRect(m_selection.left(), -1, m_selection.width(), 2);
    gMW->ui->lblSelectionTxt->setText("No selection");
    gMW->m_gvAmplitudeSpectrum->m_scene->update();
    gMW->m_gvPhaseSpectrum->m_scene->update();

    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    if(forwardsync){
        if(gMW->m_gvSpectrogram){
            if(gMW->m_gvSpectrogram->m_giShownSelection->isVisible()) {
                QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
                if(std::abs(rect.top()-gMW->m_gvSpectrogram->m_scene->sceneRect().top())<std::numeric_limits<double>::epsilon()
                    && std::abs(rect.bottom()-gMW->m_gvSpectrogram->m_scene->sceneRect().bottom())<std::numeric_limits<double>::epsilon()){
                    gMW->m_gvSpectrogram->selectionClear();
                }
                else {
                    rect.setLeft(gMW->m_gvSpectrogram->m_scene->sceneRect().left());
                    rect.setRight(gMW->m_gvSpectrogram->m_scene->sceneRect().right());
                    gMW->m_gvSpectrogram->selectionSet(rect, false);
                }
            }
        }

//        if(gMW->m_gvSpectrogram) gMW->m_gvSpectrogram->selectionClear(false);
    }
}

void QGVWaveform::fixTimeLimitsToSamples(QRectF& selection, const QRectF& mouseSelection, int action) {
    double fs = gMW->getFs();

    // Clip selection on exact sample times
    selection.setLeft((int(0.5+selection.left()*fs))/fs);
    if(selection.width()==0)
        selection.setRight(selection.left());
    else
        selection.setRight((int(0.5+selection.right()*fs))/fs);

    if(selection.left()<0) selection.setLeft(0.0);
    if(selection.left()>gMW->getMaxLastSampleTime()-1.0/fs) selection.setLeft(gMW->getMaxLastSampleTime()-1.0/fs);
    if(selection.right()<1.0/fs) selection.setRight(1.0/fs);
    if(selection.right()>gMW->getMaxLastSampleTime()) selection.setRight(gMW->getMaxLastSampleTime());

    // Adjust parity of the window size according to option
    int nl = std::max(0, int(0.5+selection.left()*fs));
    int nr = int(0.5+std::min(gMW->getMaxLastSampleTime(),selection.right())*fs);
    int winlen = nr-nl+1;
    if(winlen%2==0 && gMW->m_gvAmplitudeSpectrum->m_dlgSettings->ui->cbAmplitudeSpectrumWindowSizeForcedOdd->isChecked()) {
        if(action==CAModifSelectionLeft)
            selection.setLeft(selection.left()+1.0/fs);
        else if(action==CAModifSelectionRight)
            selection.setRight(selection.right()-1.0/fs);
        else {
            if(mouseSelection.right()>mouseSelection.left())
                selection.setRight(selection.right()-1.0/fs);
            else
                selection.setLeft(selection.left()+1.0/fs);
        }
    }
}

void QGVWaveform::selectionSet(QRectF selection, bool forwardsync){
//    COUTD << "QGVWaveform::selectionSet" << endl;

    double fs = gMW->getFs();

    // Order the selection to avoid negative width
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }

    m_selection = selection;

    fixTimeLimitsToSamples(m_selection, m_mouseSelection, m_currentAction);

    // Set the visible selection encompassing the actual selection
    m_giSelection->setRect(m_selection.left()-0.5/fs, -1, m_selection.width()+1.0/fs, 2);
    m_giSelection->show();

    playCursorSet(m_selection.left(), true); // Put the play cursor

    if(m_selection.width()>0){
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }
    else{
        m_aZoomOnSelection->setEnabled(false);
        m_aSelectionClear->setEnabled(false);
    }

    if(forwardsync){
        if(gMW->m_gvSpectrogram){
            QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
            rect.setLeft(m_selection.left());
            rect.setRight(m_selection.right());

            if(gMW->m_gvAmplitudeSpectrum && gMW->m_gvAmplitudeSpectrum->m_giShownSelection->isVisible()){
                rect.setTop(gMW->getFs()/2-gMW->m_gvAmplitudeSpectrum->m_selection.right());
                rect.setBottom(gMW->getFs()/2-gMW->m_gvAmplitudeSpectrum->m_selection.left());
            }
            else{
                rect.setTop(gMW->m_gvSpectrogram->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrogram->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrogram->selectionSet(rect, false);
        }
    }

    // The selection's width can vary up to eps, even when it is only shifted

    // Spectrum
    if(gMW->m_gvAmplitudeSpectrum
       && (gMW->m_gvAmplitudeSpectrum->isVisible() || gMW->m_gvPhaseSpectrum->isVisible()))
        gMW->m_gvAmplitudeSpectrum->setWindowRange(m_selection.left(), m_selection.right());

    // Update the visible window
    if(gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()>0
       && (int(gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()) != m_windowParams.winlen)) {

        FFTTYPE winmax = 0.0;
        for(size_t n=0; n<gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size(); n++)
            winmax = std::max(winmax, gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win[n]);
        winmax = 1.0/winmax;

        QPainterPath path;

        qreal prevx = 0;
        qreal prevy = gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win[0]*winmax;
        path.moveTo(QPointF(prevx, -prevy));
        qreal y;
        for(size_t n=1; n<gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size(); n++) {
            qreal x = n/fs;
            y = gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win[n];
            y *= winmax;
            path.lineTo(QPointF(x, -y));
            prevx = x;
            prevy = y;
        }

        // Add the vertical line
        qreal winrelcenter = ((gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()-1)/2.0)/fs;
        path.moveTo(QPointF(winrelcenter, 2.0));
        path.lineTo(QPointF(winrelcenter, -1.0));

        m_giWindow->setPath(path);
    }

    updateSelectionText();

    m_giWindow->setPos(QPointF(m_selection.left(), 0));
    m_giWindow->show();
}

void QGVWaveform::updateSelectionText(){
    // gMW->ui->lblSelectionTxt->setText(QString("[%1").arg(m_selection.left()).append(",%1] ").arg(m_selection.right()).append("%1 s").arg(m_selection.width())); // start, end and duration

    if(gMW->m_gvWaveform->m_aWaveformShowWindow->isChecked() && gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()>0)
        gMW->ui->lblSelectionTxt->setText(QString("%1s window centered at %2").arg(gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()/gMW->getFs(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.left()+((gMW->m_gvAmplitudeSpectrum->m_trgDFTParameters.win.size()-1)/2.0)/gMW->getFs(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and center
    else
        gMW->ui->lblSelectionTxt->setText(QString("[%1,%2]%3s").arg(m_selection.left(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.right(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.width(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and start
//    gMW->ui->lblSelectionTxt->setText(QString("%1s selection ").arg(m_selection.width(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).append(" starting at %1s").arg(m_selection.left(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and start
}

void QGVWaveform::selectionZoomOn(){
    if(m_selection.width()>0){

        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            viewrect.setLeft(m_selection.left()-0.1*m_selection.width());
            viewrect.setRight(m_selection.right()+0.1*m_selection.width());
        }
        else {
            viewrect.setLeft(m_selection.left());
            viewrect.setRight(m_selection.right());
        }
        viewSet(viewrect);

        m_aZoomIn->setEnabled(true);
        m_aZoomOut->setEnabled(true);
        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(false);

        setMouseCursorPosition(-1, false);
    }
}

void QGVWaveform::updateTextsGeometry(){
    QTransform trans = transform();
    QTransform cursortrans = QTransform::fromScale(1.0/trans.m11(), 1.0);
    m_giPlayCursor->setTransform(cursortrans);
    m_giMouseCursorTxt->setTransform(cursortrans);

    // Tell the labels to update their texts
    for(size_t fi=0; fi<gMW->ftlabels.size(); fi++)
        gMW->ftlabels[fi]->updateTextsGeometry();
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

//    cout << "QGVWaveform::drawBackground viewport: " << viewport()->rect() << endl;

//    QRect pixrect = mapFromScene(rect).boundingRect();
//    COUTD << pixrect << endl;

//    static QTimeTracker2 time_tracker;
//    time_tracker.restart();

//    cout << "QGVWaveform::drawBackground rect:" << rect << endl;

    if(m_aWaveformShowGrid->isChecked())
        draw_grid(painter, rect);

    // Plot STFT's window centers
    if(m_aWaveformShowSTFTWindowCenters->isChecked()){
        FTSound* currentftsound = gMW->getCurrentFTSound(true);
        if(currentftsound && currentftsound->m_actionShow->isChecked()){
            QPen outlinePen(QColor(192, 192, 192)); // currentftsound->color
            outlinePen.setStyle(Qt::DashLine);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
//            painter->setBrush(QBrush(gMW->ftlabels[fi]->color));

            for(size_t wci=0; wci<currentftsound->m_stftts.size(); wci++){
                painter->drawLine(QLineF(currentftsound->m_stftts[wci], -1.0, currentftsound->m_stftts[wci], 1.0));
            }
        }
    }

    draw_allwaveforms(painter, rect);

    m_giWindow->setVisible(m_aWaveformShowWindow->isChecked() && m_selection.width()>0.0);

//    time_tracker.storeElapsed();

//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGraphicsView::drawBackground [" << rect.left() << "," << rect.right() << "]x[" << rect.top() << "," << rect.bottom() << "] avg=" << time_tracker.getAveragedElapsed() << "(" << time_tracker.past_elapsed.size() << ") last=" << time_tracker.past_elapsed.back() << endl;
}

void QGVWaveform::draw_allwaveforms(QPainter* painter, const QRectF& rect){
//    COUTD << "QGVWaveform::draw_allwaveforms " << rect << endl;
//    std::cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": QGVWaveform::draw_waveform [" << rect.width() << "]" << endl;

    // First shift the visible waveforms according to the previous scrolling
    // (This cannot be done in scrollContentsBy because this &^$%#@ function is called when resizing the
    //  spectrogram, because they are synchronized horizontally (Using: connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int))); onnect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));)).
//    COUTD << m_scrolledx << endl;
    if(m_currentAction!=CAZooming && gMW->m_gvSpectrogram->m_currentAction!=QGVSpectrogram::CAZooming){
        for(size_t fi=0; fi<gMW->ftsnds.size(); fi++){
            if(!gMW->ftsnds[fi]->m_actionShow->isChecked())
                continue;

            if(gMW->ftsnds[fi]->m_wavpx_min.size()>0){
                int ddx = m_scrolledx;
                if(ddx>0){
                    while(ddx>0){
                        gMW->ftsnds[fi]->m_wavpx_min.pop_back();
                        gMW->ftsnds[fi]->m_wavpx_max.pop_back();
                        gMW->ftsnds[fi]->m_wavpx_min.push_front(0.0);
                        gMW->ftsnds[fi]->m_wavpx_max.push_front(0.0);
                        ddx--;
                    }
                }
                else if(ddx<0){
                    while(ddx<0){
                        gMW->ftsnds[fi]->m_wavpx_min.pop_front();
                        gMW->ftsnds[fi]->m_wavpx_max.pop_front();
                        gMW->ftsnds[fi]->m_wavpx_min.push_back(0.0);
                        gMW->ftsnds[fi]->m_wavpx_max.push_back(0.0);
                        ddx++;
                    }
                }
            }
        }
    }
    m_scrolledx = 0;

    FTSound* currsnd = gMW->getCurrentFTSound(true);
    for(size_t fi=0; fi<gMW->ftsnds.size(); fi++)
        if(!m_aWaveformShowSelectedWaveformOnTop->isChecked() || gMW->ftsnds[fi]!=currsnd)
            draw_waveform(painter, rect, gMW->ftsnds[fi]);
    if(currsnd && m_aWaveformShowSelectedWaveformOnTop->isChecked())
        draw_waveform(painter, rect, currsnd);
}

void QGVWaveform::draw_waveform(QPainter* painter, const QRectF& rect, FTSound* snd){
    if(!snd->m_actionShow->isChecked())
        return;

    double fs = gMW->getFs();

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    double samppixdensity = (viewrect.right()-viewrect.left())*fs/viewport()->rect().width();

//    samppixdensity=0.01;
    if(samppixdensity<4) {
//        COUTD << "draw_waveform: Draw lines between each sample" << endl;

        WAVTYPE a = snd->m_ampscale;
        if(snd->m_actionInvPolarity->isChecked())
            a *=-1.0;

        QPen wavpen(snd->color);
        wavpen.setWidth(0);
        painter->setPen(wavpen);

        int delay = snd->m_delay;
        int nleft = int((rect.left())*fs)-delay;
        int nright = int((rect.right())*fs)+1-delay;
        nleft = std::max(nleft, -delay);
        nleft = std::max(nleft, 0);
        nleft = std::min(nleft, int(snd->wav.size()-1)-delay);
        nleft = std::min(nleft, int(snd->wav.size()-1));
        nright = std::max(nright, -delay);
        nright = std::max(nright, 0);
        nright = std::min(nright, int(snd->wav.size()-1)-delay);
        nright = std::min(nright, int(snd->wav.size()-1));

        // Draw a line between each sample value
        WAVTYPE dt = 1.0/fs;
        WAVTYPE prevx = (nleft+delay)*dt;
        a *= -1;
        WAVTYPE* data = snd->wavtoplay->data();
        WAVTYPE prevy = a*(*(data+nleft));

        WAVTYPE x, y;
        for(int n=nleft; n<=nright; ++n){
            x = (n+delay)*dt;
            y = a*(*(data+n));

            if(y>1.0)       y = 1.0;
            else if(y<-1.0) y = -1.0;

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

        painter->drawLine(QLineF(double(snd->wav.size()-1)/fs, -1.0, double(snd->wav.size()-1)/fs, 1.0));
    }
    else {
//        COUTD << "draw_waveform: Plot only one line per pixel" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        double gain = snd->m_ampscale;
        if(snd->m_actionInvPolarity->isChecked())
            gain *= -1.0;

        QPen outlinePen(snd->color);
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);

        QRect pixrect = mapFromScene(rect).boundingRect();
//        COUTD << pixrect << endl;
        QRect fullpixrect = mapFromScene(viewrect).boundingRect();

        double s2p = -(fullpixrect.height()-1)/viewrect.height(); // Scene to pixel
        double p2n = fs*double(viewrect.width())/double(fullpixrect.width()-1); // Pixel to scene
        double yzero = fullpixrect.height()/2;

        WAVTYPE* yp = snd->wavtoplay->data();

        int winpixdelay = horizontalScrollBar()->value(); // - 1.0/p2n; // The magic value to make everything plot at the same place whatever the scroll

        int snddelay = snd->m_delay;

        FTSound::WavParameters reqparams(fullpixrect, viewrect, winpixdelay, snd);
        if(snd->wavtoplay==&(snd->wav) && reqparams==snd->m_wavparams){
//            COUTD << "Using existing buffer " << pixrect << endl;
            for(int i=pixrect.left(); i<=pixrect.right(); i++){
//            for(int i=0; i<=snd->m_wavpx_min.size(); i++)
                if(snd->m_wavpx_min[i]<1e10) // TODO Clean this dirty fix
                    painter->drawLine(QLineF(i, yzero+snd->m_wavpx_min[i], i, yzero+snd->m_wavpx_max[i]));
            }
        }
        else {
//            COUTD << "Re compute buffer and draw " << pixrect << endl;
            if(int(snd->m_wavpx_min.size())!=reqparams.fullpixrect.width()){
//                COUTD << "Resize" << endl;
                snd->m_wavpx_min.resize(reqparams.fullpixrect.width());
                snd->m_wavpx_max.resize(reqparams.fullpixrect.width());
                pixrect = reqparams.fullpixrect;
            }
//            COUTD << pixrect << endl;
            int ns = int((pixrect.left()+winpixdelay)*p2n)-snddelay;
            for(int i=pixrect.left(); i<=pixrect.right(); i++) {

                int ne = int((i+1+winpixdelay)*p2n)-snddelay;

                if(ns>=0 && ne<int(snd->wav.size())) {
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
                    ymin = gain*ymin;
                    if(ymin>1.0)       ymin = 1.0;
                    else if(ymin<-1.0) ymin = -1.0;
                    ymax = gain*ymax;
                    if(ymax>1.0)       ymax = 1.0;
                    else if(ymax<-1.0) ymax = -1.0;
                    ymin *= s2p;
                    ymax *= s2p;
                    ymin = int(ymin-2);
                    ymax = int(ymax-2);
                    snd->m_wavpx_min[i] = ymin;
                    snd->m_wavpx_max[i] = ymax;
                    painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
                }
                else {
                    snd->m_wavpx_min[i] = 1e20; // TODO Clean this dirty fix
                    snd->m_wavpx_max[i] = 1e20; // TODO Clean this dirty fix
                }

                ns = ne;
            }
        }
        snd->m_wavparams = reqparams;

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }
}

void QGVWaveform::draw_grid(QPainter* painter, const QRectF& rect){

//    std::cout << "QGVWaveform::draw_grid " << rect.left() << " " << rect.right() << endl;

    // Plot the grid

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    painter->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    QFontMetrics qfm(gMW->m_dlgSettings->ui->lblGridFontSample->font());

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
    for(double l=int(rect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(l, rect.top(), l, rect.bottom()));
        mn++;
    }

    // Write the absissa of the vertical lines
    painter->setPen(m_gridFontPen);
    for(double l=int(rect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->save();
        painter->translate(QPointF(l, viewrect.bottom()-(qfm.height()-2)/trans.m22()));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1s").arg(l)));
        painter->restore();
    }
}

void QGVWaveform::playCursorSet(double t, bool forwardsync){
    if(t==-1){
        if(m_selection.width()>0)
            playCursorSet(m_selection.left(), forwardsync);
        else
            m_giPlayCursor->setPos(QPointF(m_initialPlayPosition, 0));
    }
    else{
        m_giPlayCursor->setPos(QPointF(t, 0));
    }

    if(forwardsync && gMW->m_gvSpectrogram)
        gMW->m_gvSpectrogram->playCursorSet(t, false);
}

double QGVWaveform::getPlayCursorPosition(){
    return m_giPlayCursor->pos().x();
}
