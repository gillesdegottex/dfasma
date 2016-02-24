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

#include "ftsound.h"
#include "ftlabels.h"

#include "gvspectrumamplitude.h"
#include "gvspectrumamplitudewdialogsettings.h"
#include "ui_gvspectrumamplitudewdialogsettings.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "wgenerictimevalue.h"
#include "gvgenerictimevalue.h"

#include <iostream>
using namespace std;

#include <QtGlobal>
#include <QMenu>
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
#include "../external/audioengine/audioengine.h"

#include "qaehelpers.h"

GVWaveform::GVWaveform(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_ftlabel_current_index(-1)
//    , m_scrolledx(0)  // For #419 ?
    , m_ampzoom(1.0)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_aWaveformShowGrid = new QAction(tr("Show &grid"), this);
    m_aWaveformShowGrid->setObjectName("m_aWaveformShowGrid");
    m_aWaveformShowGrid->setStatusTip(tr("Show &grid"));
    m_aWaveformShowGrid->setCheckable(true);
    m_aWaveformShowGrid->setChecked(true);
    m_aWaveformShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aWaveformShowGrid);
    connect(m_aWaveformShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(update()));
    m_contextmenu.addAction(m_aWaveformShowGrid);
    m_giGrid = new QAEGIGrid(this, "s", "");
    m_giGrid->setMathYAxis(true);
    m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giGrid->setVisible(m_aWaveformShowGrid->isChecked());
    m_scene->addItem(m_giGrid);
    connect(m_aWaveformShowGrid, SIGNAL(toggled(bool)), this, SLOT(gridSetVisible(bool)));

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
    m_giMouseCursorTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giMouseCursorLine->setZValue(100);
    m_giMouseCursorTxt->setZValue(100);
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
    m_giSelection->setZValue(100);
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
    connect(m_aWaveformShowWindow, SIGNAL(toggled(bool)), m_scene, SLOT(update()));
    connect(m_aWaveformShowWindow, SIGNAL(toggled(bool)), this, SLOT(updateSelectionText()));
    m_contextmenu.addAction(m_aWaveformShowWindow);
    m_aWaveformShowSTFTWindowCenters = new QAction(tr("Show STFT's window centers"), this);
    m_aWaveformShowSTFTWindowCenters->setObjectName("m_aWaveformShowSTFTWindowCenters");
    m_aWaveformShowSTFTWindowCenters->setStatusTip(tr("Show STFT's window centers"));
    m_aWaveformShowSTFTWindowCenters->setCheckable(true);
    m_aWaveformShowSTFTWindowCenters->setChecked(false);
    gMW->m_settings.add(m_aWaveformShowSTFTWindowCenters);
    connect(m_aWaveformShowSTFTWindowCenters, SIGNAL(toggled(bool)), m_scene, SLOT(update()));
    m_contextmenu.addAction(m_aWaveformShowSTFTWindowCenters);

    m_aWaveformStickToSTFTWindows = new QAction(tr("Stick window to STFT windows' postion"), this);
    m_aWaveformStickToSTFTWindows->setObjectName("m_aWaveformStickToSTFTWindows");
    m_aWaveformStickToSTFTWindows->setStatusTip(tr("Set the window length and position according to the STFT's analysis instants"));
    m_aWaveformStickToSTFTWindows->setCheckable(true);
    m_aWaveformStickToSTFTWindows->setChecked(false);
    gMW->m_settings.add(m_aWaveformStickToSTFTWindows);
    m_contextmenu.addAction(m_aWaveformStickToSTFTWindows);

    // Play Cursor
    m_giPlayCursor = new QGraphicsLineItem(0.0, -1.0, 0.0, 1.0, NULL);
    QPen playCursorPen(QColor(255, 0, 0));
    playCursorPen.setCosmetic(true);
    playCursorPen.setWidth(2);
    m_giPlayCursor->setPen(playCursorPen);
    m_giPlayCursor->setZValue(100);
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

    m_aZoomXOnly = new QAction(tr("Zoom x-axis only"), this);
    m_aZoomXOnly->setObjectName("m_aZoomXOnly");
    m_aZoomXOnly->setStatusTip(tr("Zoom only on the x-axis, keep the y-axis unchanged."));
    m_aZoomXOnly->setCheckable(true);
    m_aZoomXOnly->setChecked(true);
    gMW->m_settings.add(m_aZoomXOnly);
    connect(m_aZoomXOnly, SIGNAL(toggled(bool)), this, SLOT(setZoomXOnly(bool)));
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
//    m_aUnZoom->setEnabled(false);
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
    m_contextmenu.addAction(m_aZoomXOnly);

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

void GVWaveform::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void GVWaveform::fitViewToSoundsAmplitude() {
    if(gFL->ftsnds.size()>0){
        WAVTYPE maxwavmaxamp = 0.0;
        for(unsigned int si=0; si<gFL->ftsnds.size(); si++)
            if(gFL->ftsnds[si]->isVisible())
                maxwavmaxamp = std::max(maxwavmaxamp, gFL->ftsnds[si]->m_giWavForWaveform->getMaxAbsoluteValue());
//                maxwavmaxamp = std::max(maxwavmaxamp, gFL->ftsnds[si]->m_giWaveform->gain()*gFL->ftsnds[si]->m_wavmaxamp);

        if(maxwavmaxamp==0.0)
            maxwavmaxamp = 1.0;

        // Add a small margin
        maxwavmaxamp *= 1.05;

        gMW->ui->sldWaveformAmplitude->setValue(100-(maxwavmaxamp*100.0));
    }
}

void GVWaveform::updateSceneRect() {
    m_scene->setSceneRect(-1.0/gFL->getFs(), -1.05*m_ampzoom, gFL->getMaxDuration()+1.0/gFL->getFs(), 2.1*m_ampzoom);
}

void GVWaveform::viewSet(QRectF viewrect, bool sync) {

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

//    COUTD << "GVWaveform::viewSet: viewrect=" << viewrect << endl;

    if(viewrect!=currentviewrect) {

        QPointF center = viewrect.center();

        if(viewrect.width()<10.0/gFL->getFs()){
           viewrect.setLeft(center.x()-5.0/gFL->getFs());
           viewrect.setRight(center.x()+5.0/gFL->getFs());
        }

        if(viewrect.top()<m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top()-0.02);
        if(viewrect.bottom()>m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom()+0.02);
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        fitInView(removeHiddenMargin(this, viewrect));

        updateTextsGeometry();
        m_giGrid->updateLines();

        if(sync){
            if(gMW->m_gvSpectrogram && gMW->ui->actionShowSpectrogram->isChecked()) {
//                DCOUT << gMW->m_gvSpectrogram->viewport()->rect() << endl;
                QRectF spectrorect = gMW->m_gvSpectrogram->mapToScene(gMW->m_gvSpectrogram->viewport()->rect()).boundingRect();
//                DCOUT << spectrorect << endl;
                spectrorect.setLeft(viewrect.left());
                spectrorect.setRight(viewrect.right());
                gMW->m_gvSpectrogram->viewSet(spectrorect, false);
            }

            for(int i=0; i<gMW->m_wGenericTimeValues.size(); ++i){
                if(gMW->m_wGenericTimeValues.at(i)) {
                    QRectF rect = gMW->m_wGenericTimeValues.at(i)->gview()->mapToScene(gMW->m_wGenericTimeValues.at(i)->gview()->viewport()->rect()).boundingRect();
                    rect.setLeft(viewrect.left());
                    rect.setRight(viewrect.right());
                    gMW->m_wGenericTimeValues.at(i)->gview()->viewSet(rect, false);
                }
            }
        }
    }
}

void GVWaveform::sldAmplitudeChanged(int value){

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
}

void GVWaveform::azoomin(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(viewrect.left()+viewrect.width()/4);
    viewrect.setRight(viewrect.right()-viewrect.width()/4);
    viewSet(viewrect);

    setMouseCursorPosition(-1, false);

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(true);
//    m_aUnZoom->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void GVWaveform::azoomout(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(viewrect.left()-viewrect.width()/4);
    viewrect.setRight(viewrect.right()+viewrect.width()/4);
    viewSet(viewrect);

    setMouseCursorPosition(-1, false);

    m_aZoomOut->setEnabled(true);
//    m_aUnZoom->setEnabled(true);
    m_aZoomIn->setEnabled(true);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void GVWaveform::aunzoom(){

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    viewrect.setLeft(m_scene->sceneRect().left());
    viewrect.setRight(m_scene->sceneRect().right());
    viewSet(viewrect);

    setMouseCursorPosition(-1, false);

    m_aZoomIn->setEnabled(true);
    m_aZoomOut->setEnabled(false);
//    m_aUnZoom->setEnabled(false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0);
}

void GVWaveform::setZoomXOnly(bool zoomxonly)
{
    if(zoomxonly){
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        viewrect.setBottom(m_scene->sceneRect().bottom());
        viewrect.setTop(m_scene->sceneRect().top());
        viewSet(viewrect);
    }
}

void GVWaveform::setMouseCursorPosition(double position, bool forwardsync) {
    if(position==-1){
        m_giMouseCursorLine->hide();
        m_giMouseCursorTxt->hide();
    }
    else {
        m_giMouseCursorLine->show();
        m_giMouseCursorTxt->show();

        m_giMouseCursorLine->setPos(position, 0.0);

        m_giMouseCursorTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        QString txt = QString("%1s").arg(position, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value());
        m_giMouseCursorTxt->setText(txt);

        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QRectF br = m_giMouseCursorTxt->boundingRect();
        position = min(position, double(viewrect.right()-br.width()/trans.m11()));
        m_giMouseCursorTxt->setPos(position+1/trans.m11(), viewrect.top()+0/trans.m22());
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
        m_giMouseCursorTxt->setTransform(txttrans);

        if(forwardsync){
            for(int i=0; i<gMW->m_wGenericTimeValues.size(); ++i)
                if(gMW->m_wGenericTimeValues.at(i))
                    gMW->m_wGenericTimeValues.at(i)->gview()->setMouseCursorPosition(QPointF(position, -1.0), false);
            if(gMW->m_gvSpectrogram)
                gMW->m_gvSpectrogram->setMouseCursorPosition(QPointF(position, 0.0), false);
        }
    }
}

void GVWaveform::resizeEvent(QResizeEvent* event){
//    COUTD << "GVWaveform::resizeEvent oldSize: " << event->oldSize().isEmpty() << endl;

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

void GVWaveform::scrollContentsBy(int dx, int dy){
//    COUTD << "GVWaveform::scrollContentsBy " << dx << "," << dy << endl;

//    m_scrolledx += dx;  // For #419 ?

    QGraphicsView::scrollContentsBy(dx, dy);

    m_giGrid->updateLines();
}

void GVWaveform::wheelEvent(QWheelEvent* event){

    QPoint numDegrees = event->angleDelta() / 8;

//    DCOUT << "GVWaveform::wheelEvent " << numDegrees.y() << endl;

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

//    cout << "GVWaveform::wheelEvent: " << viewrect << endl;

    if(event->modifiers().testFlag(Qt::ShiftModifier)){
        QScrollBar* sb = horizontalScrollBar();
        sb->setValue(sb->value()-3*numDegrees.y());
        m_giGrid->updateLines();
    }
    else if((viewrect.width()>10.0/gFL->getFs() && numDegrees.y()>0) || numDegrees.y()<0) {
        double gx = double(mapToScene(event->pos()).x()-viewrect.left())/viewrect.width();
        double gy = double(mapToScene(event->pos()).y()-viewrect.top())/viewrect.height();
        QRectF newrect = mapToScene(viewport()->rect()).boundingRect();
        newrect.setLeft(newrect.left()+gx*0.01*viewrect.width()*numDegrees.y());
        newrect.setRight(newrect.right()-(1-gx)*0.01*viewrect.width()*numDegrees.y());
        if(!m_aZoomXOnly->isChecked()){
            newrect.setTop(newrect.top()+gy*0.01*viewrect.height()*numDegrees.y());
            newrect.setBottom(newrect.bottom()-(1-gy)*0.01*viewrect.height()*numDegrees.y());
        }
        viewSet(newrect);

//        QPointF p = mapToScene(QPoint(event->x(),0));
        setMouseCursorPosition(-1, true);
        m_aZoomOnSelection->setEnabled(m_selection.width()>0);
        m_aZoomOut->setEnabled(true);
        m_aZoomIn->setEnabled(true);
//        m_aUnZoom->setEnabled(true);
    }

//    std::cout << "~GVWaveform::wheelEvent" << endl;
}

void GVWaveform::mousePressEvent(QMouseEvent* event){
    // std::cout << "GVWaveform::mousePressEvent" << endl;

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
                    m_selection_pressedp = p-QPointF(m_selection.left(), 0.0);
                    setCursor(Qt::SplitHCursor);
                }
                else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && abs(selview.right()-event->x())<5){
                    // Resize right boundary of the selection
                    m_currentAction = CAModifSelectionRight;
                    m_mouseSelection = m_selection;
                    m_selection_pressedp = p-QPointF(m_selection.right(), 0.0);
                    setCursor(Qt::SplitHCursor);
                }
                else if(m_selection.width()>0 && (event->modifiers().testFlag(Qt::ControlModifier) || (event->x()>=selview.left() && event->x()<=selview.right()))){
                    // cout << "Scrolling the selection" << endl;
                    m_currentAction = CAMovingSelection;
                    m_selection_pressedp = p;
                    m_mouseSelection = m_selection;
                    setCursor(Qt::ClosedHandCursor);
                }
                else if(!event->modifiers().testFlag(Qt::ControlModifier)){
                    // When selecting
                    m_currentAction = CASelecting;
                    m_selection_pressedp = p;
                    m_mouseSelection.setLeft(m_selection_pressedp.x());
                    m_mouseSelection.setRight(m_selection_pressedp.x());
                    selectionSet(m_mouseSelection);
                    m_giSelection->show();
                }
            }
            else if(gMW->ui->actionEditMode->isChecked()
                    && (gFL->currentFile() && gFL->currentFile()->isVisible())){

                // Look for a nearby marker to modify
                m_ca_pressed_index=-1;
                FTLabels* selectedlabels = gFL->getCurrentFTLabels();
                if(selectedlabels){
                    for(int lli=0; m_ca_pressed_index==-1 && lli<int(selectedlabels->starts.size()); lli++) {
                        QPoint slp = mapFromScene(QPointF(selectedlabels->starts[lli],0));
                        if(std::abs(slp.x()-event->x())<5) {
                            m_ca_pressed_index = lli;
                            m_currentAction = CALabelModifPosition;
                            gMW->setEditing(selectedlabels);
                        }
                    }
                    if(m_ca_pressed_index==-1) {
                        if(event->modifiers().testFlag(Qt::ControlModifier)){
                            m_selection_pressedp = p;
                            m_currentAction = CALabelAllModifPosition;
                            setCursor(Qt::SizeHorCursor);
                            gMW->setEditing(selectedlabels);
                        }
                    }

                    if(m_currentAction==CANothing)
                        playCursorSet(p.x(), true); // Put the play cursor
                }

                // Force this one so that there is no need to select a
                // file if there is just one waveform
                // (TODO could be replaced by selecting the first file autom.)
                FTSound* selectedsound = gFL->getCurrentFTSound();
                if(!selectedlabels && selectedsound) {
                    if(event->modifiers().testFlag(Qt::ShiftModifier)){
                    }
                    else if(event->modifiers().testFlag(Qt::ControlModifier)){
                        // cout << "Scaling the waveform" << endl;
                        m_currentAction = CAWaveformDelay;
                        m_selection_pressedp = p;
                        m_tmpdelay = selectedsound->m_giWavForWaveform->delay()/gFL->getFs();
                        setCursor(Qt::SizeHorCursor);
                        gMW->setEditing(selectedsound);
                    }
                    else{
                        // cout << "Scaling the waveform" << endl;
                        m_currentAction = CAWaveformScale;
                        m_selection_pressedp = p;
                        setCursor(Qt::SizeVerCursor);
                        gMW->setEditing(selectedsound);
                    }
                }
            }
        }
    }
    else if(event->buttons()&Qt::RightButton){
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_currentAction = CAZooming;
            m_selection_pressedp = p;
            m_pressed_mouseinviewport = mapFromScene(p);
            m_pressed_scenerect = mapToScene(viewport()->rect()).boundingRect();
            setCursor(Qt::CrossCursor);

            QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//            COUTD << viewrect << " " << m_scene->sceneRect() << std::endl;
            // If the mouse is close enough to a border, set to it
            // TODO There is still some crapy very very small differences and things
            //      unclear in the scene rect and the viewrect, which is supposely
            //      set to the scene rect. But it might not be possible to solve
            //      this issue because of bug 11945 (approx worked around by removeHiddenMargin(.))
//            COUTD << std::abs(viewrect.right()-1.0/gFL->getFs()-m_scene->sceneRect().right()) << " " << std::abs(m_pressed_mouseinviewport.x()-viewport()->rect().right()) << std::endl;
            if(std::abs(viewrect.left()-1.0/gFL->getFs()-m_scene->sceneRect().left())<std::numeric_limits<float>::epsilon()
               && std::abs(m_pressed_mouseinviewport.x()-viewport()->rect().left())<20)
                m_selection_pressedp.setX(m_scene->sceneRect().left());
            if(std::abs(viewrect.right()-1.0/gFL->getFs()-m_scene->sceneRect().right())<std::numeric_limits<float>::epsilon()
               && std::abs(m_pressed_mouseinviewport.x()-viewport()->rect().right())<20)
                m_selection_pressedp.setX(m_scene->sceneRect().right());
        }
        else if (event->modifiers().testFlag(Qt::ControlModifier) &&
                 m_selection.width()>0) {
            m_currentAction = CAStretchSelection;
            m_mouseSelection = m_selection;
            m_selection_pressedp = p;
            setCursor(Qt::SplitHCursor);
        }
    }

    QGraphicsView::mousePressEvent(event);
    // std::cout << "~GVWaveform::mousePressEvent " << p.x() << endl;
}

void GVWaveform::mouseMoveEvent(QMouseEvent* event){
//    COUTD << "GVWaveform::mouseMoveEvent" << m_selection.width() << endl;

    bool kshift = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p.x(), true);

    if(m_currentAction==CAMoving) {
        // When scrolling along the waveform
        setMouseCursorPosition(-1, false);
        m_giGrid->updateLines();
        QGraphicsView::mouseMoveEvent(event);
    }
    else if(m_currentAction==CAZooming) {
        double dx = -(event->pos()-m_pressed_mouseinviewport).x()/100.0;
        double dy = (event->pos()-m_pressed_mouseinviewport).y()/100.0;

        QRectF newrect = m_pressed_scenerect;
        newrect.setLeft(m_selection_pressedp.x()-(m_selection_pressedp.x()-m_pressed_scenerect.left())*exp(dx));
        newrect.setRight(m_selection_pressedp.x()+(m_pressed_scenerect.right()-m_selection_pressedp.x())*exp(dx));
        if(!m_aZoomXOnly->isChecked()){
            newrect.setTop(m_selection_pressedp.y()-(m_selection_pressedp.y()-m_pressed_scenerect.top())*exp(dy));
            newrect.setBottom(m_selection_pressedp.y()+(m_pressed_scenerect.bottom()-m_selection_pressedp.y())*exp(dy));
        }
        viewSet(newrect);

        setMouseCursorPosition(m_selection_pressedp.x(), true);

//        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedp.x(), 0, p.x()-m_selection_pressedp.x(), 0);
        selectionSet(m_mouseSelection);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setLeft(m_selection_pressedp.x());
        m_mouseSelection.setRight(p.x());
        selectionSet(m_mouseSelection);
    }
    else if(m_currentAction==CAStretchSelection) {
        // Stretch the selection from its center
        m_mouseSelection.setLeft(m_mouseSelection.left()-(p.x()-m_selection_pressedp.x()));
        m_mouseSelection.setRight(m_mouseSelection.right()+(p.x()-m_selection_pressedp.x()));
        selectionSet(m_mouseSelection);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CAWaveformScale){
        // Scale the selected waveform
        FTSound* currentftsound = gFL->getCurrentFTSound(true);
        if(currentftsound){
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                gMW->setEditing(NULL);
                m_currentAction = CANothing;
            }
            else {
                currentftsound->m_giWavForWaveform->setGain(currentftsound->m_giWavForWaveform->gain()*pow(10, -10*(p.y()-m_selection_pressedp.y())/20.0));
                m_selection_pressedp = p;

                if(currentftsound->m_giWavForWaveform->gain()>1e10)
                    currentftsound->m_giWavForWaveform->setGain(1e10);
                else if(currentftsound->m_giWavForWaveform->gain()<1e-10)
                    currentftsound->m_giWavForWaveform->setGain(1e-10);

                currentftsound->needDFTUpdate();
                currentftsound->setStatus();

                m_scene->update();
                gMW->m_gvSpectrumAmplitude->updateDFTs();
                gFL->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
                if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
                    gMW->m_gvSpectrogram->updateSTFTSettings();
            }
        }
    }
    else if(m_currentAction==CAWaveformDelay){
        FTSound* currentftsound = gFL->getCurrentFTSound(true);
        if(currentftsound){
            // Check first if the user is not trying to modify a hidden file
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                gMW->setEditing(NULL);
                m_currentAction = CANothing;
            }
            else {
                m_tmpdelay += p.x()-m_selection_pressedp.x();
                m_selection_pressedp = p;
                currentftsound->m_giWavForWaveform->setDelay(int(0.5+m_tmpdelay*gFL->getFs()));
                if(m_tmpdelay<0)
                    currentftsound->m_giWavForWaveform->setDelay(currentftsound->m_giWavForWaveform->delay()+1);

                currentftsound->needDFTUpdate();
                currentftsound->setStatus();

                m_scene->update();
                gMW->m_gvSpectrumAmplitude->updateDFTs();
                gFL->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
                if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
                    gMW->m_gvSpectrogram->updateSTFTSettings();
            }
        }
    }
    else if(m_currentAction==CALabelWritting){
        FTLabels* ftlabel = gFL->getCurrentFTLabels();
        if(ftlabel) {
            m_ftlabel_current_index = -1;
            m_currentAction = CANothing;
        }
    }
    else if(m_currentAction==CALabelModifPosition){
        FTLabels* ftlabel = gFL->getCurrentFTLabels();
        if(ftlabel) {
            ftlabel->moveLabel(m_ca_pressed_index, p.x());
            updateTextsGeometry();
        }
    }
    else if(m_currentAction==CALabelAllModifPosition){
//        COUTD << "CALabelAllModifPosition" << endl;
        FTLabels* ftlabel = gFL->getCurrentFTLabels();
        if(ftlabel){
            ftlabel->moveAllLabel(p.x()-m_selection_pressedp.x());
            m_selection_pressedp = p;
            m_scene->update();
            updateTextsGeometry();
        }
    }
    else{ // There is no action

        QRect selview = mapFromScene(m_giSelection->boundingRect()).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(kshift){
            }
            else if(kctrl){
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
            FTLabels* ftl = gFL->getCurrentFTLabels();
            if(ftl){
                for(int lli=0; !foundclosemarker && lli<ftl->getNbLabels(); lli++) {
                    // cout << ftl->labels[lli].toLatin1().data() << ": " << ftl->starts[lli] << endl;
                    QPoint slp = mapFromScene(QPointF(ftl->starts[lli],0));
                    foundclosemarker = std::abs(slp.x()-event->x())<5;
                }
            }
            if(foundclosemarker)
                setCursor(Qt::SplitHCursor);
            else
                setCursor(Qt::CrossCursor);

            FileType* ft = gFL->currentFile();
            if(kshift){
            }
            else if(kctrl){
                if(!foundclosemarker && ft && ft->is(FileType::FTSOUND))
                    setCursor(Qt::SizeHorCursor);
            }
            else{
                if(!foundclosemarker && ft && ft->is(FileType::FTSOUND))
                    setCursor(Qt::SizeVerCursor);
            }
        }

        QGraphicsView::mouseMoveEvent(event);
    }

//    std::cout << "~GVWaveform::mouseMoveEvent" << endl;
}

void GVWaveform::mouseReleaseEvent(QMouseEvent* event){
//    std::cout << "GVWaveform::mouseReleaseEvent " << m_selection.width() << endl;

    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);

    m_currentAction = CANothing;

    gMW->updateMouseCursorState(kshift, kctrl);

    // Order the mouse selection to avoid negative width
    if(m_mouseSelection.right()<m_mouseSelection.left()){
        float tmp = m_mouseSelection.left();
        m_mouseSelection.setLeft(m_mouseSelection.right());
        m_mouseSelection.setRight(tmp);
    }

    if(m_currentAction==CALabelModifPosition){
        FTLabels* ftlabel = gFL->getCurrentFTLabels();
        if(ftlabel)
            ftlabel->finishEditing();
    }

    if(gMW->ui->actionEditMode->isChecked()){
        gMW->setEditing(NULL);
    }

    if(abs(m_selection.width())==0)
        selectionClear();
    else{
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~GVWaveform::mouseReleaseEvent " << endl;
}

void GVWaveform::mouseDoubleClickEvent(QMouseEvent* event){

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

void GVWaveform::selectSegmentFindStartEnd(double x, FTLabels* ftl, double& start, double& end){
    start = -1;
    end = -1;
    if(ftl && ftl->getNbLabels()>0) {
        if(x<ftl->starts[0]){
            start = 0.0;
            end = ftl->starts[0];
        }
        else if(x>ftl->starts.back()){
            start = ftl->starts.back();
            end = gFL->getMaxLastSampleTime();
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

void GVWaveform::selectSegment(double x, bool add){
    // Check if a marker is close and show the horiz split cursor if true
    FTLabels* ftl = gFL->getCurrentFTLabels(true);
    if(ftl && ftl->getNbLabels()>0) {
        if(ftl->starts.size()>0){
            double start, end;
            selectSegmentFindStartEnd(x, ftl, start, end);
            if(add){
                start = std::min(m_selection.left(), start);
                end = std::max(m_selection.right(), end);
            }
            m_mouseSelection = QRectF(start, -1.0, end-start, 2.0);
            selectionSet(m_mouseSelection);
        }
    }
}

void GVWaveform::selectRemoveSegment(double x){
    FTLabels* ftl = gFL->getCurrentFTLabels(true);
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


void GVWaveform::keyPressEvent(QKeyEvent* event){

//    COUTD << "GVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Delete) {
        if(m_currentAction==CALabelModifPosition) {
            FTLabels* ftlabel = gFL->getCurrentFTLabels(false);
            if(ftlabel){
                ftlabel->removeLabel(m_ca_pressed_index);
                m_currentAction = CANothing;
            }
        }
    }
    else if(!gMW->ui->actionEditMode->isChecked() && event->key()==Qt::Key_S){
        selectionZoomOn();
    }
    else if(event->key()==Qt::Key_Escape) {
        if(!hasSelection()) {
            if(!gMW->m_gvSpectrogram->hasSelection()
                && !gMW->m_gvSpectrumAmplitude->hasSelection())
                playCursorSet(0.0, true);

            gMW->m_gvSpectrumAmplitude->selectionClear();
        }
        selectionClear(true);
    }
    else {
        if (gMW->ui->actionEditMode->isChecked()){
            FTLabels* ftlabel = gFL->getCurrentFTLabels(false);
            if(ftlabel && !FTGraphicsLabelItem::isEditing()){
                if(event->text().size()>0){
                    if(m_currentAction==CALabelWritting && m_ftlabel_current_index!=-1){
                        ftlabel->changeText(m_ftlabel_current_index, ftlabel->waveform_labels[m_ftlabel_current_index]->toPlainText()+event->text());
                    }
                    else{
                        m_currentAction = CALabelWritting;
                        ftlabel->addLabel(m_giMouseCursorLine->pos().x(), event->text());
                        m_ftlabel_current_index = ftlabel->getNbLabels()-1;
                        gFL->fileInfoUpdate();
                    }
                    updateTextsGeometry(); // TODO Could be avoided maybe
                }
            }
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void GVWaveform::selectionClear(bool forwardsync){
    m_giSelection->hide();
    m_selection = QRectF(0, -1, 0, 2);
    m_giSelection->setRect(m_selection.left(), -1, m_selection.width(), 2);
    gMW->ui->lblSelectionTxt->setText("No selection");
    gMW->m_gvSpectrumAmplitude->m_scene->update();
    gMW->m_gvSpectrumPhase->m_scene->update();
    gMW->m_gvSpectrumGroupDelay->m_scene->update();

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

void GVWaveform::fixTimeLimitsToSamples(QRectF& selection, const QRectF& mouseSelection, int action) {
    double fs = gFL->getFs();

    // Clip selection on exact sample times
    selection.setLeft((int(0.5+selection.left()*fs))/fs);
    if(selection.width()==0)
        selection.setRight(selection.left());
    else
        selection.setRight((int(0.5+selection.right()*fs))/fs);

    if(selection.left()<0) selection.setLeft(0.0);
//    if(selection.left()>gFL->getMaxLastSampleTime()-1.0/fs) selection.setLeft(gFL->getMaxLastSampleTime()-1.0/fs);
    if(selection.left()>gFL->getMaxLastSampleTime()) selection.setLeft(gFL->getMaxLastSampleTime());
    //    if(selection.right()<1.0/fs) selection.setRight(1.0/fs);
    if(selection.right()<0.0) selection.setRight(0.0);
    if(selection.right()>gFL->getMaxLastSampleTime()) selection.setRight(gFL->getMaxLastSampleTime());

    double twinend = selection.right();
    if(gMW->m_gvSpectrumAmplitude->m_dlgSettings->ui->cbAmplitudeSpectrumLimitWindowDuration->isChecked()
       && (twinend-selection.left())>gMW->m_gvSpectrumAmplitude->m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value())
        twinend = selection.left()+gMW->m_gvSpectrumAmplitude->m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value();

    // Adjust parity of the window size according to option
    int nl = std::max(0, int(0.5+selection.left()*fs));
    int nr = int(0.5+std::min(gFL->getMaxLastSampleTime(),twinend)*fs);
    int winlen = nr-nl+1;
    if(selection.width()>0 && winlen%2==0 && gMW->m_gvSpectrumAmplitude->m_dlgSettings->ui->cbAmplitudeSpectrumWindowSizeForcedOdd->isChecked()) {
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

    // Set window's center to STFT's analysis instants
    if(m_aWaveformStickToSTFTWindows->isChecked()){
        FTSound* cursnd = gFL->getCurrentFTSound(true);
        if(cursnd){
//            double wincenter = selection.left() + (winlen-1)/2.0/gMW->getFs();
//            int si = int((wincenter*gMW->getFs()-1 - (cursnd->m_stftparams.win.size()-1)/2.0) / cursnd->m_stftparams.stepsize + 0.5);
//            si = std::min(std::max(0, si), int(cursnd->m_stftts.size())-1);
//            double dt = (si*cursnd->m_stftparams.stepsize + (cursnd->m_stftparams.win.size()-1)/2.0)/gMW->getFs()  -  wincenter;
//            selection.setLeft(selection.left()+dt);
//            selection.setRight(selection.right()+dt);

            int si = int((selection.center().x()*gFL->getFs()-1 - (cursnd->m_stftparams.win.size()-1)/2.0) / cursnd->m_stftparams.stepsize + 0.5);
            gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.lock();
            si = std::min(std::max(0, si), int(cursnd->m_stftts.size())-1);
            gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.unlock();
            selection.setLeft((si*cursnd->m_stftparams.stepsize)/gFL->getFs());
            selection.setRight((si*cursnd->m_stftparams.stepsize + cursnd->m_stftparams.win.size()-1)/gFL->getFs());
        }
    }
}

GVWaveform::~GVWaveform(){
    delete m_toolBar;
}

void GVWaveform::contextMenuEvent(QContextMenuEvent *event){
    if (event->modifiers().testFlag(Qt::ShiftModifier)
        || event->modifiers().testFlag(Qt::ControlModifier))
        return;

    QPoint posglobal = mapToGlobal(event->pos()+QPoint(0,0));
    m_contextmenu.exec(posglobal);
}

void GVWaveform::selectionSet(QRectF selection, bool forwardsync){
//    COUTD << "GVWaveform::selectionSet " << selection << endl;

    double fs = gFL->getFs();

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

            if(gMW->m_gvSpectrumAmplitude && gMW->m_gvSpectrumAmplitude->m_giShownSelection->isVisible()){
                rect.setTop(-gMW->m_gvSpectrumAmplitude->m_selection.right());
                rect.setBottom(-gMW->m_gvSpectrumAmplitude->m_selection.left());
            }
            else{
                rect.setTop(gMW->m_gvSpectrogram->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrogram->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrogram->selectionSet(rect, false);
        }
    }

    // The selection's width can vary up to eps, even when it is only shifted

    // Spectra
    if(gMW->m_gvSpectrumAmplitude
       && (gMW->m_gvSpectrumAmplitude->isVisible()
           || gMW->m_gvSpectrumPhase->isVisible()
           || gMW->m_gvSpectrumGroupDelay->isVisible()))
        gMW->m_gvSpectrumAmplitude->setWindowRange(m_selection.left(), m_selection.right());

    updateSelectionText();

    m_giWindow->setPos(QPointF(m_selection.left(), 0));
    m_giWindow->show();
}

void GVWaveform::updateSelectionText(){
    // gMW->ui->lblSelectionTxt->setText(QString("[%1").arg(m_selection.left()).append(",%1] ").arg(m_selection.right()).append("%1 s").arg(m_selection.width())); // start, end and duration

    if(gMW->m_gvWaveform->m_aWaveformShowWindow->isChecked() && gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.win.size()>0)
        gMW->ui->lblSelectionTxt->setText(QString("%1s window centered at %2").arg(gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.win.size()/gFL->getFs(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.left()+((gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.win.size()-1)/2.0)/gFL->getFs(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and center
    else
        gMW->ui->lblSelectionTxt->setText(QString("[%1,%2]%3s").arg(m_selection.left(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.right(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(m_selection.width(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and start
//    gMW->ui->lblSelectionTxt->setText(QString("%1s selection ").arg(m_selection.width(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).append(" starting at %1s").arg(m_selection.left(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value())); // duration and start
}

void GVWaveform::selectionZoomOn(){
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
//        m_aUnZoom->setEnabled(true);
        m_aZoomOnSelection->setEnabled(false);

        setMouseCursorPosition(-1, false);
    }
}

void GVWaveform::updateTextsGeometry(){
    QTransform trans = transform();
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    m_giMouseCursorTxt->setPos(m_giMouseCursorTxt->pos().x(), viewrect.top()+0/trans.m22());
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(),1.0/trans.m22());
    m_giMouseCursorTxt->setTransform(txttrans);

    // Tell the labels to update their texts
    for(size_t fi=0; fi<gFL->ftlabels.size(); fi++)
        gFL->ftlabels[fi]->updateTextsGeometryWaveform();
}

void GVWaveform::drawBackground(QPainter* painter, const QRectF& rect){
    Q_UNUSED(rect)
    // COUTD << "GVWaveform::drawBackground rect:" << rect << endl;

    updateTextsGeometry(); // TODO Since called here, can be removed from many other places

    m_giWindow->setVisible(m_aWaveformShowWindow->isChecked() && m_selection.width()>0.0); // TODO Put elsewhere?

    // Plot STFT's window centers
    if(m_aWaveformShowSTFTWindowCenters->isChecked()){
        FTSound* cursnd = gFL->getCurrentFTSound(true);
        if(cursnd && cursnd->m_actionShow->isChecked()){
            QPen outlinePen(QColor(192, 192, 192)); // currentftsound->color
            outlinePen.setStyle(Qt::DashLine);
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);

            gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.lock();
            for(size_t wci=0; wci<cursnd->m_stftts.size(); wci++)
                painter->drawLine(QLineF(cursnd->m_stftts[wci], -1.0, cursnd->m_stftts[wci], 1.0));
            gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_changingstft.unlock();
        }
    }
}

void GVWaveform::playCursorSet(double t, bool forwardsync){
    if(t==-1){
        if(m_selection.width()>0)
            playCursorSet(m_selection.left(), forwardsync);
        else
            m_giPlayCursor->setPos(QPointF(m_initialPlayPosition, 0));

        // Put back the DFT window at selection times
        if(gMW->m_gvSpectrumAmplitude && gMW->m_gvSpectrumPhase
           && (gMW->m_gvSpectrumAmplitude->isVisible() || gMW->m_gvSpectrumPhase->isVisible()))
            gMW->m_gvSpectrumAmplitude->setWindowRange(m_selection.left(), m_selection.right());
    }
    else{
        m_giPlayCursor->setPos(QPointF(t, 0));

        // Move the DFT window according to play cursor
        if(gMW->m_gvSpectrumAmplitude && gMW->m_gvSpectrumPhase
            && gMW->m_gvSpectrumAmplitude->m_aFollowPlayCursor->isChecked()
            && gMW->m_audioengine->state()==QAudio::ActiveState // TODO Means that audio is necessary for this
            && gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.winlen>1
            && (gMW->m_gvSpectrumAmplitude->isVisible() || gMW->m_gvSpectrumPhase->isVisible())) {
            double halfwin = ((gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.winlen-1)/2.0)/gFL->getFs();
            gMW->m_gvSpectrumAmplitude->setWindowRange(t-halfwin, t+halfwin);
        }
    }

    if(forwardsync && gMW->m_gvSpectrogram)
        gMW->m_gvSpectrogram->playCursorSet(t, false);
}

double GVWaveform::getPlayCursorPosition() const{
    return m_giPlayCursor->pos().x();
}
