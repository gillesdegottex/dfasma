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

#include "gvgenerictimevalue.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "wgenerictimevalue.h"
#include "ui_wgenerictimevalue.h"

#include "ftgenerictimevalue.h"

#include "gvwaveform.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "ftsound.h"
#include "ftfzero.h"

#include <iostream>
#include <algorithm>
using namespace std;

#include <qnumeric.h>
#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>

#include "qaesigproc.h"
#include "qaehelpers.h"

GVGenericTimeValue::GVGenericTimeValue(WidgetGenericTimeValue *parent)
    : QGraphicsView(parent)
    , m_fgtv(parent)
{
    setStyleSheet("GVGenericTimeValue { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

//    m_dlgSettings = new GVAmplitudeSpectrumWDialogSettings(this);
//    gMW->m_settings.add(gMW->ui->sldAmplitudeSpectrumMin);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the spectrum view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_aShowGrid = new QAction(tr("Show &grid"), this);
    m_aShowGrid->setObjectName("m_aGenericTimeValueShowGrid");
    m_aShowGrid->setStatusTip(tr("Show or hide the grid"));
    m_aShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aShowGrid->setCheckable(true);
    m_aShowGrid->setChecked(true);
    gMW->m_settings.add(m_aShowGrid);
    m_giGrid = new QAEGIGrid(this, "s", "");
    m_giGrid->setMathYAxis(true);
    m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giGrid->setVisible(m_aShowGrid->isChecked());
    m_scene->addItem(m_giGrid);
    connect(m_aShowGrid, SIGNAL(toggled(bool)), this, SLOT(gridSetVisible(bool)));
//    connect(gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop, SIGNAL(triggered()), m_scene, SLOT(update()));

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -1000, 0, 1000);
    m_giCursorHoriz->hide();
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursorHoriz->setPen(cursorPen);
    m_giCursorHoriz->setZValue(100);
    m_scene->addItem(m_giCursorHoriz);
    m_giCursorVert = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0); // TODO
    m_giCursorVert->hide();
    m_giCursorVert->setPen(cursorPen);
    m_giCursorVert->setZValue(100);
    m_scene->addItem(m_giCursorVert);
    m_giCursorPositionXTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionXTxt->hide();
    m_giCursorPositionXTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionXTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giCursorPositionXTxt->setZValue(100);
    m_scene->addItem(m_giCursorPositionXTxt);
    m_giCursorPositionYTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionYTxt->hide();
    m_giCursorPositionYTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionYTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giCursorPositionYTxt->setZValue(100);
    m_scene->addItem(m_giCursorPositionYTxt);

    // Selection
    m_currentAction = CANothing;
    m_giShownSelection = new QGraphicsRectItem();
    m_giShownSelection->hide();
    m_giShownSelection->setZValue(100);
    m_scene->addItem(m_giShownSelection);
    m_giSelectionTxt = new QGraphicsSimpleTextItem();
    m_giSelectionTxt->hide();
    m_giSelectionTxt->setBrush(QColor(64, 64, 64));
    m_giSelectionTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_scene->addItem(m_giSelectionTxt);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giShownSelection->setPen(selectionPen);
    m_giShownSelection->setBrush(selectionBrush);
    m_giShownSelection->setOpacity(0.5);
    gMW->ui->lblSpectrumSelectionTxt->setText("No selection");

    showScrollBars(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked());
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), this, SLOT(showScrollBars(bool)));
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

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
    m_aUnZoom->setIcon(QIcon(":/icons/unzoomxy.svg"));
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(true);
    //m_aZoomOnSelection->setShortcut(Qt::Key_S); // This one creates "ambiguous" shortcuts
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(true);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));

    m_fgtv->ui->lblSelectionTxt->setText("No selection");

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aAutoUpdateDFT);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->addSeparator();
    m_toolBar->addAction(widget()->m_aRemoveView);
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(), gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    m_fgtv->ui->lToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aShowGrid);
//    m_contextmenu.addSeparator();
//    m_contextmenu.addAction(m_aShowProperties);
//    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(show()));
//    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(settingsModified()));

    connect(gMW->m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), horizontalScrollBar(), SLOT(setValue(int)));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), gMW->m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));
    connect(gMW->m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), horizontalScrollBar(), SLOT(setValue(int)));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), gMW->m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int)));

    for(int gvi=0; gvi<gMW->m_wGenericTimeValues.size(); ++gvi){
        connect(gMW->m_wGenericTimeValues[gvi]->gview()->horizontalScrollBar(), SIGNAL(valueChanged(int)), horizontalScrollBar(), SLOT(setValue(int)));
        connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), gMW->m_wGenericTimeValues[gvi]->gview()->horizontalScrollBar(), SLOT(setValue(int)));
    }
}

void GVGenericTimeValue::gridSetVisible(bool visible){m_giGrid->setVisible(visible);}

//void GVGenericTimeValue::settingsModified(){
//}

void GVGenericTimeValue::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

//void GVGenericTimeValue::updateAmplitudeExtent(){
//    COUTD << "QGVAmplitudeSpectrum::updateAmplitudeExtent" << endl;

//    if(gFL->ftsnds.size()>0){
//        gMW->ui->sldAmplitudeSpectrumMin->setMaximum(0);
//        gMW->ui->sldAmplitudeSpectrumMin->setMinimum(-3*gFL->getMaxSQNR()); // to give a margin

//        updateSceneRect();
//    }
//}

void GVGenericTimeValue::updateSceneRect() {
    double values_min = +std::numeric_limits<double>::infinity();
    double values_max = -std::numeric_limits<double>::infinity();
    for(int i=0; i<m_ftgenerictimevalues.size(); ++i){
        if(!std::isinf(m_ftgenerictimevalues.at(i)->m_values_min))
            values_min = std::min(values_min, m_ftgenerictimevalues.at(i)->m_values_min);
        if(!std::isinf(m_ftgenerictimevalues.at(i)->m_values_max))
            values_max = std::max(values_max, m_ftgenerictimevalues.at(i)->m_values_max);
    }
    values_min -= 0.05*(values_max-values_min);
    values_max += 0.05*(values_max-values_min);
    if(std::isinf(values_min))
        values_min = -1000.0;// This happens only if none of the data has a valid range
    if(std::isinf(values_max))
        values_max = 1000.0; // This happens only if none of the data has a valid range
    m_scene->setSceneRect(-1.0/gFL->getFs(), -values_max, gFL->getMaxDuration()+1.0/gFL->getFs(), (values_max-values_min));
}

void GVGenericTimeValue::viewSet(QRectF viewrect, bool sync) {
//    cout << "QGVAmplitudeSpectrum::viewSet" << endl;

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

        QPointF center = viewrect.center();

        if(viewrect.width()<10.0/gFL->getFs()){
           viewrect.setLeft(center.x()-5.0/gFL->getFs());
           viewrect.setRight(center.x()+5.0/gFL->getFs());
        }

        double vminimum = 1e-16;
        if(viewrect.height()<vminimum){
            viewrect.setTop(center.y()-0.5*vminimum);
            viewrect.setBottom(center.y()+0.5*vminimum);
        }

        if(viewrect.top()<=m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top());
        if(viewrect.bottom()>=m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom());
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        // This is not perfect and might never be because:
        // 1) The workaround removeHiddenMargin is apparently simplistic
        // 2) This position in real coordinates involves also the position of the
        //    scrollbars which fit integers only. Thus the final position is always
        //    aproximative.
        // A solution would be to subclass QSplitter, catch the view when the
        // splitter's handle is clicked, and repeat this view until button released.
        fitInView(removeHiddenMargin(this, viewrect));

        m_giGrid->updateLines();

        if(sync){
            if(gMW->m_gvWaveform && !gMW->m_gvWaveform->viewport()->size().isEmpty()) { // && gMW->ui->actionShowWaveform->isChecked()
                QRectF currect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
                currect.setLeft(viewrect.left());
                currect.setRight(viewrect.right());
                gMW->m_gvWaveform->viewSet(currect, false);
            }

            if(gMW->m_gvSpectrogram && gMW->ui->actionShowSpectrogram->isChecked()) {
                QRectF spectrorect = gMW->m_gvSpectrogram->mapToScene(gMW->m_gvSpectrogram->viewport()->rect()).boundingRect();
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

//    cout << "QGVAmplitudeSpectrum::~viewSet" << endl;
}

void GVGenericTimeValue::contextMenuEvent(QContextMenuEvent *event){
    if (event->modifiers().testFlag(Qt::ShiftModifier)
        || event->modifiers().testFlag(Qt::ControlModifier))
        return;

    QPoint posglobal = mapToGlobal(event->pos()+QPoint(0,0));
    m_contextmenu.exec(posglobal);
}

void GVGenericTimeValue::resizeEvent(QResizeEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::resizeEvent" << endl;

    // Note: Resized is called for all views so better to not forward modifications
    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {

        updateSceneRect();

        if(gMW->m_gvWaveform->viewport()->rect().width()*gMW->m_gvWaveform->viewport()->rect().height()>0){
            QRectF srcrect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(srcrect.left());
            viewrect.setRight(srcrect.right());
            viewrect.setTop(sceneRect().top());
            viewrect.setBottom(sceneRect().bottom());
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);

    }
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    viewUpdateTexts();
    setMouseCursorPosition(QPointF(-1,0), false);
}

void GVGenericTimeValue::scrollContentsBy(int dx, int dy) {
//    cout << "QGVAmplitudeSpectrum::scrollContentsBy" << endl;

    setMouseCursorPosition(QPointF(-1,0), false);
    viewUpdateTexts();

    QGraphicsView::scrollContentsBy(dx, dy);

    m_giGrid->updateLines();
}

void GVGenericTimeValue::wheelEvent(QWheelEvent* event) {

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

    m_aZoomOut->setEnabled(true);
    m_aZoomIn->setEnabled(true);
}

void GVGenericTimeValue::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(gMW->ui->actionSelectionMode->isChecked()){
            if(kshift) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setMouseCursorPosition(QPointF(-1,0), false);
            }
            else if(!kctrl && m_selection.width()>0 && m_selection.height()>0 && abs(selview.left()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()) {
                // Resize left boundary of the selection
                m_currentAction = CAModifSelectionLeft;
                m_selection_pressedp = QPointF(p.x()-m_selection.left(), 0);
            }
            else if(!kctrl && m_selection.width()>0 && m_selection.height()>0 && abs(selview.right()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()){
                // Resize right boundary of the selection
                m_currentAction = CAModifSelectionRight;
                m_selection_pressedp = QPointF(p.x()-m_selection.right(), 0);
            }
            else if(!kctrl && m_selection.width()>0 && m_selection.height()>0 && abs(selview.top()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
                // Resize top boundary of the selection
                m_currentAction = CAModifSelectionTop;
                m_selection_pressedp = QPointF(0, p.y()-m_selection.top());
            }
            else if(!kctrl && m_selection.width()>0 && m_selection.height()>0 && abs(selview.bottom()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
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
                selectionSet(m_mouseSelection, true);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()
                && (gFL->currentFile() && gFL->currentFile()->isVisible())){
        }
    }
    else if(event->buttons()&Qt::RightButton) {
        if (kshift) {
            setCursor(Qt::CrossCursor);
            m_currentAction = CAZooming;
            m_selection_pressedp = p;
            m_pressed_mouseinviewport = mapFromScene(p);
            m_pressed_viewrect = mapToScene(viewport()->rect()).boundingRect();
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void GVGenericTimeValue::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVAmplitudeSpectrum::mouseMoveEvent" << endl;

    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p, true);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
        setMouseCursorPosition(QPointF(-1,0), false);
        m_giGrid->updateLines();
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
        setMouseCursorPosition(m_selection_pressedp, true);
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
    else{
        QRect selview = mapFromScene(m_selection).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(!kshift && !kctrl){
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
        }
    }

//    std::cout << "~GVGenericTimeValue::mouseMoveEvent" << endl;
    QGraphicsView::mouseMoveEvent(event);
}

void GVGenericTimeValue::mouseReleaseEvent(QMouseEvent* event) {
//    std::cout << "QGVAmplitudeSpectrum::mouseReleaseEvent " << endl;

    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);

    QPointF p = mapToScene(event->pos());

    m_currentAction = CANothing;

    gMW->updateMouseCursorState(kshift, kctrl);

    if(gMW->ui->actionSelectionMode->isChecked()) {
        if(abs(m_selection.width())<=0 || abs(m_selection.height())<=0)
            selectionClear();

        if (!kshift && !kctrl){
            if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                setCursor(Qt::OpenHandCursor);
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void GVGenericTimeValue::keyPressEvent(QKeyEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape) {
        if(!hasSelection()) {
//            if(!gMW->m_gvSpectrogram->hasSelection()
//                && !gMW->m_gvWaveform->hasSelection())
//                gMW->m_gvWaveform->playCursorSet(0.0, true);

            gMW->m_gvWaveform->selectionClear();
        }
        selectionClear();
    }
    if(event->key()==Qt::Key_S)
        selectionZoomOn();

    QGraphicsView::keyPressEvent(event);
}

void GVGenericTimeValue::selectionClear(bool forwardsync) {
    Q_UNUSED(forwardsync)

    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    setCursor(Qt::CrossCursor);

    if(gMW->m_gvSpectrumPhase)
        gMW->m_gvSpectrumPhase->selectionClear();
    if(gMW->m_gvSpectrumGroupDelay)
        gMW->m_gvSpectrumGroupDelay->selectionClear();

    if(forwardsync){
        // TODO
    }

    selectionSetTextInForm();

    m_scene->update();
}

void GVGenericTimeValue::selectionSetTextInForm() {

    QString str;

//    cout << "QGVAmplitudeSpectrum::selectionSetText: " << m_selection.height() << " " << gMW->m_gvPhaseSpectrum->m_selection.height() << endl;

    if (m_selection.height()==0 && gMW->m_gvSpectrumPhase->m_selection.height()==0) {
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
            left = gMW->m_gvSpectrumPhase->m_selection.left();
            right = gMW->m_gvSpectrumPhase->m_selection.right();
        }
        // TODO The line below cannot be avoided exept by reversing the y coordinate of the
        //      whole seen of the spectrogram, and I don't know how to do this :(
//        if(std::abs(left)<1e-10)
//            left=0.0;

        str += QString("[%1,%2]%3s [%4,%5]%6").arg(left).arg(right).arg(right-left).arg(-m_selection.top()).arg(-m_selection.bottom()).arg(m_selection.height());
    }

    m_fgtv->ui->lblSelectionTxt->setText(str);
}

void GVGenericTimeValue::selectionSet(QRectF selection, bool forwardsync) {

    // Order the selection to avoid negative width and negative height
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }
//    COUTD << selection << " " << selection.width() << "x" << selection.height() << std::endl;
    if(selection.top()>selection.bottom()){
        float tmp = selection.top();
        selection.setTop(selection.bottom());
        selection.setBottom(tmp);
    }
//    COUTD << selection << " " << selection.width() << "x" << selection.height() << std::endl;

    m_selection = m_mouseSelection = selection;

//    if(m_selection.left()<0) m_selection.setLeft(0);
//    if(m_selection.right()>gFL->getFs()/2.0) m_selection.setRight(gFL->getFs()/2.0);
//    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
//    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    m_giShownSelection->setRect(m_selection);
    m_giShownSelection->show();

    m_giSelectionTxt->setText(QString("%1s,%2").arg(m_selection.width()).arg(m_selection.height()));
    viewUpdateTexts();

    selectionSetTextInForm();

    if(forwardsync) {
        // TODO
    }

    selectionSetTextInForm();
}

void GVGenericTimeValue::viewUpdateTexts() {
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

void GVGenericTimeValue::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
            zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
            zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
            zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        }
        viewSet(zoomonrect);

        viewUpdateTexts();

        if(gMW->m_gvSpectrumPhase)
            gMW->m_gvSpectrumPhase->viewUpdateTexts();
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->viewUpdateTexts();

        setMouseCursorPosition(QPointF(-1,0), false);
    }
}

void GVGenericTimeValue::azoomin(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
}
void GVGenericTimeValue::azoomout(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
}
void GVGenericTimeValue::aunzoom(){
    viewSet(m_scene->sceneRect(), true);
}

void GVGenericTimeValue::setMouseCursorPosition(QPointF p, bool forwardsync) {
    QFontMetrics qfm(gMW->m_dlgSettings->ui->lblGridFontSample->font());

    QLineF line;
    line.setP1(QPointF(p.x(), m_giCursorVert->line().y1()));
    line.setP2(QPointF(p.x(), m_giCursorVert->line().y2()));
    m_giCursorVert->setLine(line);
    line.setP1(QPointF(m_giCursorHoriz->line().x1(), p.y()));
    line.setP2(QPointF(m_giCursorHoriz->line().x2(), p.y()));
    m_giCursorHoriz->setLine(line);

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
        m_giCursorHoriz->setLine(viewrect.right()-50/trans.m11(), m_giCursorHoriz->line().y1(), gFL->getFs()/2.0, m_giCursorHoriz->line().y1());
        m_giCursorVert->show();
        m_giCursorVert->setLine(m_giCursorVert->line().x1(), viewrect.top(), m_giCursorVert->line().x1(), viewrect.top()+(qfm.height())/trans.m22());
        m_giCursorPositionXTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionYTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionXTxt->show();
        m_giCursorPositionYTxt->show();

        m_giCursorPositionXTxt->setTransform(txttrans);
        m_giCursorPositionYTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionXTxt->boundingRect();
        qreal x = m_giCursorVert->line().x1()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());

        QString freqstr = QString("%1s").arg(m_giCursorVert->line().x1());
        m_giCursorPositionXTxt->setText(freqstr);
        m_giCursorPositionXTxt->setPos(x, viewrect.top()-2/trans.m22());

        m_giCursorPositionYTxt->setText(QString("%1").arg(-m_giCursorHoriz->line().y1()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), m_giCursorHoriz->line().y1()-br.height()/trans.m22());
    }

    if(forwardsync){
        // TODO
    }
}

void GVGenericTimeValue::drawBackground(QPainter* painter, const QRectF& rect){
//    COUTD << "QGVAmplitudeSpectrum::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

}

GVGenericTimeValue::~GVGenericTimeValue(){
//    delete m_dlgSettings;
    delete m_toolBar;
}
