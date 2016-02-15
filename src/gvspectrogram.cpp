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
#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"
#include "stftcomputethread.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "ftsound.h"
#include "ftlabels.h"
#include "ftfzero.h"

#include "gvwaveform.h"
#include "gvspectrumamplitude.h"
#include "gvspectrumamplitudewdialogsettings.h"
#include "ui_gvspectrumamplitudewdialogsettings.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "wgenerictimevalue.h"
#include "gvgenerictimevalue.h"

#include <iostream>
#include <algorithm>
#include <cmath>
using namespace std;

#include <QMessageBox>
#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QToolTip>
#include <QScrollBar>
#include "../external/libqxt/qxtspanslider.h"

#include "qaesigproc.h"
#include "qaehelpers.h"
#include "qaegisampledsignal.h"

GVSpectrogram::GVSpectrogram(WMainWindow* parent)
    : QGraphicsView(parent)
    , m_editing_fzero(NULL)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
//    QTransform trans;
//    trans.scale(1.0, -1.0); // Reverse the y-axis
//    setTransform(trans);

    m_dlgSettings = new GVSpectrogramWDialogSettings(this);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the Spectrogram view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_aSpectrogramShowGrid = new QAction(tr("Show &grid"), this);
    m_aSpectrogramShowGrid->setObjectName("m_aSpectrogramShowGrid"); // For auto settings
    m_aSpectrogramShowGrid->setStatusTip(tr("Show &grid"));
    m_aSpectrogramShowGrid->setCheckable(true);
    m_aSpectrogramShowGrid->setChecked(false);
    m_aSpectrogramShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aSpectrogramShowGrid);
    m_giGrid = new QAEGIGrid(this, "s", "Hz");
    m_giGrid->setMathYAxis(true);
    m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    m_giGrid->setVisible(m_aSpectrogramShowGrid->isChecked());
    m_giGrid->setHighContrast(true);
    m_scene->addItem(m_giGrid);
    connect(m_aSpectrogramShowGrid, SIGNAL(toggled(bool)), this, SLOT(gridSetVisible(bool)));

    m_aSpectrogramShowHarmonics = new QAction(tr("Show F0 &harmonic"), this);
    m_aSpectrogramShowHarmonics->setObjectName("m_aSpectrogramShowHarmonics"); // For auto settings
    m_aSpectrogramShowHarmonics->setStatusTip(tr("Show the harmonics of the fundamental frequency curves"));
    m_aSpectrogramShowHarmonics->setCheckable(true);
    m_aSpectrogramShowHarmonics->setChecked(false);
//    m_aSpectrogramShowHarmonics->setIcon(QIcon(":/icons/grid.svg"));
    gMW->m_settings.add(m_aSpectrogramShowHarmonics);
    connect(m_aSpectrogramShowHarmonics, SIGNAL(toggled(bool)), this, SLOT(showHarmonics(bool)));

    m_aAutoUpdate = new QAction(tr("Auto-Update STFT"), this);
    m_aAutoUpdate->setStatusTip(tr("Auto-Update the STFT view when the waveform is modified"));
    m_aAutoUpdate->setCheckable(true);
    m_aAutoUpdate->setChecked(false);
    m_aAutoUpdate->setIcon(QIcon(":/icons/autoupdate.svg"));
//    connect(m_aAutoUpdateDFT, SIGNAL(toggled(bool)), this, SLOT(settingsModified()));

    m_stftcomputethread = new STFTComputeThread(this);

    // Cursor
    m_giMouseCursorLineTimeBack = new QGraphicsLineItem(0, 0, 1, 1);
    QPen cursorPen(QColor(0,0,0));
    cursorPen.setWidth(0);
    m_giMouseCursorLineTimeBack->setPen(cursorPen);
    m_giMouseCursorLineTimeBack->hide();
    m_scene->addItem(m_giMouseCursorLineTimeBack);
    m_giMouseCursorLineFreqBack = new QGraphicsLineItem(0, 0, 1, 1);
    m_giMouseCursorLineFreqBack->setPen(cursorPen);
    m_giMouseCursorLineFreqBack->hide();
    m_scene->addItem(m_giMouseCursorLineFreqBack);
    QColor cursorcolor(255, 255, 255); //QColor(64, 64, 64)
    cursorPen = QPen(cursorcolor);
    cursorPen.setWidth(0);
    m_giMouseCursorLineTime = new QGraphicsLineItem(0, 0, 1, 1);
    m_giMouseCursorLineTime->setPen(cursorPen);
    m_giMouseCursorLineTime->hide();
    m_scene->addItem(m_giMouseCursorLineTime);
    m_giMouseCursorLineFreq = new QGraphicsLineItem(0, 0, 1, 1);
    m_giMouseCursorLineFreq->setPen(cursorPen);
    m_giMouseCursorLineFreq->hide();
    m_scene->addItem(m_giMouseCursorLineFreq);
    m_giMouseCursorTxtTimeBack = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtTimeBack->setBrush(QColor(0,0,0));
    m_giMouseCursorTxtTimeBack->hide();
    m_scene->addItem(m_giMouseCursorTxtTimeBack);
    m_giMouseCursorTxtFreqBack = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtFreqBack->setBrush(QColor(0,0,0));
    m_giMouseCursorTxtFreqBack->hide();
    m_scene->addItem(m_giMouseCursorTxtFreqBack);
    m_giMouseCursorTxtTime = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtTime->setBrush(cursorcolor);
    m_giMouseCursorTxtTime->hide();
    m_scene->addItem(m_giMouseCursorTxtTime);
    m_giMouseCursorTxtFreq = new QGraphicsSimpleTextItem();
    m_giMouseCursorTxtFreq->setBrush(cursorcolor);
    m_giMouseCursorTxtFreq->hide();
    m_scene->addItem(m_giMouseCursorTxtFreq);

    // Play Cursor
    m_giPlayCursor = new QGraphicsLineItem(0.0, 0.0, 0.0, -100000, NULL);
    QPen playCursorPen(QColor(255, 0, 0));
    playCursorPen.setCosmetic(true);
    playCursorPen.setWidth(2);
    m_giPlayCursor->setPen(playCursorPen);
    playCursorSet(0.0, false);
    m_scene->addItem(m_giPlayCursor);

    // Selection
    m_currentAction = CANothing;
    m_giShownSelection = new QGraphicsRectItem();
    m_giShownSelection->hide();
    m_scene->addItem(m_giShownSelection);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giShownSelection->setPen(selectionPen);
    m_giShownSelection->setBrush(selectionBrush);
    m_giShownSelection->setOpacity(0.5);
    gMW->ui->lblSpectrogramSelectionTxt->setText("No selection");

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
    m_aZoomOnSelection->setEnabled(false);
    //m_aZoomOnSelection->setShortcut(Qt::Key_S); // This one creates "ambiguous" shortcuts
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));

    gMW->ui->lblSpectrogramSelectionTxt->setText("No selection");

    showScrollBars(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked());
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), this, SLOT(showScrollBars(bool)));
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    gMW->ui->pbSTFTComputingCancel->hide();
    gMW->ui->pgbSpectrogramSTFTCompute->hide();
    gMW->ui->pgbSpectrogramSTFTCompute->setMaximum(100);
    gMW->ui->lblSpectrogramInfoTxt->setText("");

    connect(m_stftcomputethread, SIGNAL(stftComputingStateChanged(int)), this, SLOT(stftComputingStateChanged(int)));
    connect(m_stftcomputethread, SIGNAL(stftProgressing(int)), gMW->ui->pgbSpectrogramSTFTCompute, SLOT(setValue(int)));

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aShowProperties);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(),gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lSpectrogramToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aSpectrogramShowGrid);
    m_contextmenu.addAction(m_aSpectrogramShowHarmonics);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aAutoUpdate);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(show()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(updateSTFTSettings()));
    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), m_dlgSettings, SLOT(close()));

    connect(m_dlgSettings->ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(updateSTFTSettings()));

    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(sliderPressed()), this, SLOT(amplitudeExtentSlidersChanged()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(sliderReleased()), this, SLOT(amplitudeExtentSlidersChangesEnded()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(spanChanged(int,int)), SLOT(amplitudeExtentSlidersChanged()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(lowerValueChanged(int)), this, SLOT(updateSTFTPlot()));
    connect(gMW->m_qxtSpectrogramSpanSlider, SIGNAL(upperValueChanged(int)), this, SLOT(updateSTFTPlot()));

    connect(m_aAutoUpdate, SIGNAL(toggled(bool)), this, SLOT(autoUpdate(bool)));

    updateSTFTSettings(); // Prepare a window from loaded settings
}

void GVSpectrogram::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void GVSpectrogram::gridSetVisible(bool visible){
    m_giGrid->setVisible(visible);
}

void GVSpectrogram::showHarmonics(bool show){
    for(size_t fi=0; fi<gFL->ftfzeros.size(); ++fi) // TODO Could do only the prev selected one
        gFL->ftfzeros[fi]->m_giHarmonicForSpectrogram->setVisible(show);
    m_scene->update();
}

void GVSpectrogram::amplitudeExtentSlidersChanged(){
    if(!gMW->isLoading()) {
        QString tt;
        if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==0)
            tt = QString("[%1,%2]\% of amplitude range").arg(gMW->m_qxtSpectrogramSpanSlider->lowerValue()).arg(gMW->m_qxtSpectrogramSpanSlider->upperValue());
        else if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==1)
            tt = QString("[%1,%2]dB of amplitude").arg(gMW->m_qxtSpectrogramSpanSlider->lowerValue()).arg(gMW->m_qxtSpectrogramSpanSlider->upperValue());
        QToolTip::showText(QCursor::pos(), tt, this);
        gMW->m_qxtSpectrogramSpanSlider->setToolTip(tt);

        FTSound* csnd = gFL->getCurrentFTSound(true);
        if(csnd) {
            FFTTYPE ymin = 0.0; // Init shouldn't be used
            FFTTYPE ymax = 1.0; // Init shouldn't be used
            if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==0){
                ymin = csnd->m_stft_min+(csnd->m_stft_max-csnd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0; // Min of color range [dB]
                ymax = csnd->m_stft_min+(csnd->m_stft_max-csnd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0; // Max of color range [dB]
            }
            else if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==1){
                ymin = gMW->m_qxtSpectrogramSpanSlider->lowerValue();
                ymax = gMW->m_qxtSpectrogramSpanSlider->upperValue();
            }

            gMW->m_gvSpectrumAmplitude->m_giSpectrogramMin->setPos(0, -ymin);
            gMW->m_gvSpectrumAmplitude->m_giSpectrogramMin->show();
            gMW->m_gvSpectrumAmplitude->m_giSpectrogramMax->setPos(0, -ymax);
            gMW->m_gvSpectrumAmplitude->m_giSpectrogramMax->show();
        }
    }

    updateSTFTPlot();
}
void GVSpectrogram::amplitudeExtentSlidersChangesEnded() {
    gMW->m_gvSpectrumAmplitude->m_giSpectrogramMin->hide();
    gMW->m_gvSpectrumAmplitude->m_giSpectrogramMax->hide();
}


void GVSpectrogram::updateSTFTSettings(){
//    DCOUT << "GVSpectrogram::updateSTFTSettings" << endl;

    gMW->ui->pbSpectrogramSTFTUpdate->hide();
    m_dlgSettings->checkImageSize();

    int winlen = std::floor(0.5+gFL->getFs()*m_dlgSettings->ui->sbSpectrogramWindowSize->value());
    //    cout << "GVSpectrogram::updateSTFTSettings winlen=" << winlen << endl;

    if(winlen%2==0 && m_dlgSettings->ui->cbSpectrogramWindowSizeForcedOdd->isChecked())
        winlen++;

    // Create the window
    int wintype = m_dlgSettings->ui->cbSpectrogramWindowType->currentIndex();
    if(wintype==0)
        m_win = qae::rectangular(winlen);
    else if(wintype==1)
        m_win = qae::hamming(winlen);
    else if(wintype==2)
        m_win = qae::hann(winlen);
    else if(wintype==3)
        m_win = qae::blackman(winlen);
    else if(wintype==4)
        m_win = qae::blackmannutall(winlen);
    else if(wintype==5)
        m_win = qae::blackmanharris(winlen);
    else if(wintype==6)
        m_win = qae::nutall(winlen);
    else if(wintype==7)
        m_win = qae::flattop(winlen);
    else if(wintype==8)
        m_win = qae::normwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowNormSigma->value());
    else if(wintype==9)
        m_win = qae::expwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowExpDecay->value());
    else if(wintype==10)
        m_win = qae::gennormwindow(winlen, m_dlgSettings->ui->spSpectrogramWindowNormSigma->value(), m_dlgSettings->ui->spSpectrogramWindowNormPower->value());
    else
        throw QString("No window selected");

//    assert(m_win.size()==winlen);

    // Normalize the window energy to sum=1
    double winsum = 0.0;
    for(size_t n=0; n<m_win.size(); ++n)
        winsum += m_win[n];
    for(size_t n=0; n<m_win.size(); ++n)
        m_win[n] /= winsum;

    updateSTFTPlot();

//    cout << "GVSpectrogram::~updateSTFTSettings" << endl;
}


//template<typename streamtype>
//streamtype& operator<<(streamtype& stream, const GVSpectrogram::ImageParameters& imgparams){
//    stream << imgparams.stftparams.snd << " win.size=" << imgparams.stftparams.win.size() << " stepsize=" << imgparams.stftparams.stepsize << " dftlen=" << imgparams.stftparams.dftlen << " amp=[" << imgparams.amplitudeMin << ", " << imgparams.amplitudeMax << "]";
//}


void GVSpectrogram::stftComputingStateChanged(int state){
//    DCOUT << "GVSpectrogram::stftComputingStateChanged " << state << std::endl;
    if(state==STFTComputeThread::SCSDFT){
//        COUTD << "SCSDFT" << endl;
        gMW->ui->pgbSpectrogramSTFTCompute->setValue(0);
        gMW->ui->pgbSpectrogramSTFTCompute->show();
        gMW->ui->pbSTFTComputingCancel->show();
        gMW->ui->pbSpectrogramSTFTUpdate->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("Computing STFT"));
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_progresswidgets_lastup = QTime::currentTime();
        QTimer::singleShot(125, this, SLOT(showProgressWidgets()));
    }
    else if(state==STFTComputeThread::SCSIMG){
//        COUTD << "SCSIMG" << endl;
        gMW->ui->pgbSpectrogramSTFTCompute->setValue(0);
        gMW->ui->pgbSpectrogramSTFTCompute->show();
        gMW->ui->pbSTFTComputingCancel->show();
        gMW->ui->pbSpectrogramSTFTUpdate->hide();
        gMW->ui->lblSpectrogramInfoTxt->setText(QString("Updating Image"));
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_progresswidgets_lastup = QTime::currentTime();
        QTimer::singleShot(125, this, SLOT(showProgressWidgets()));
    }
    else if(state==STFTComputeThread::SCSFinished){
//        COUTD << "SCSFinished" << endl;
        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->hide();
        gMW->ui->pgbSpectrogramSTFTCompute->hide();
        gMW->ui->wSpectrogramProgressWidgets->hide();
        m_stftcomputethread->m_params_last.stftparams.snd->m_imgSTFTParams = m_stftcomputethread->m_params_last;
//        COUTD << m_imgSTFTParams.stftparams.dftlen << endl;
//        gMW->ui->lblSpectrogramInfoTxt->setText(" ");
//        gMW->ui->lblSpectrogramInfoTxt->setText(QString("STFT: size %1, %2s step").arg(m_imgSTFTParams.stftparams.dftlen).arg(m_imgSTFTParams.stftparams.stepsize/gMW->getFs()));
        m_scene->update();
        if(gMW->m_gvWaveform->m_aWaveformShowSTFTWindowCenters->isChecked())
            gMW->m_gvWaveform->update();

        QString tt;
        if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==0)
            tt = QString("[%1,%2]\% of amplitude range").arg(gMW->m_qxtSpectrogramSpanSlider->lowerValue()).arg(gMW->m_qxtSpectrogramSpanSlider->upperValue());
        else if(m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==1)
            tt = QString("[%1,%2]dB of amplitude").arg(gMW->m_qxtSpectrogramSpanSlider->lowerValue()).arg(gMW->m_qxtSpectrogramSpanSlider->upperValue());
        gMW->m_qxtSpectrogramSpanSlider->setToolTip(tt);
    }
    else if(state==STFTComputeThread::SCSCanceled){
//        COUTD << "SCSCanceled" << endl;
        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->hide();
        gMW->ui->pgbSpectrogramSTFTCompute->hide();
        // Use the visibility of lblSpectrogramInfoTxt to know if the canceled message has to be shown or not
        // TODO ... very dirty
        if(gMW->ui->lblSpectrogramInfoTxt->isVisible()){
            gMW->ui->lblSpectrogramInfoTxt->setText(QString("STFT Canceled"));
            gMW->ui->pbSpectrogramSTFTUpdate->show();
            gMW->ui->wSpectrogramProgressWidgets->show();
        }
        gMW->m_gvSpectrogram->m_scene->update();
    }
    else if(state==STFTComputeThread::SCSMemoryFull){
//        COUTD << "SCSMemoryFull" << endl;
        QMessageBox::critical(NULL, "Memory full!", "There is not enough free memory for computing this STFT");
    }
//    COUTD << "GVSpectrogram::~stftComputingStateChanged" << endl;
}

void GVSpectrogram::showProgressWidgets() {
//    COUTD << "GVSpectrogram::showProgressWidgets " << QTime::currentTime().msecsSinceStartOfDay() << " " << m_progresswidgets_lastup.msecsSinceStartOfDay() << " " << QTime::currentTime().msecsSinceStartOfDay()-m_progresswidgets_lastup.msecsSinceStartOfDay() << endl;
    if(m_progresswidgets_lastup.msecsTo(QTime::currentTime())>=125 && m_stftcomputethread->isComputing())
        gMW->ui->wSpectrogramProgressWidgets->show();
}

void GVSpectrogram::autoUpdate(bool autoupdate){
    if(autoupdate && gMW->ui->pbSpectrogramSTFTUpdate->isVisible())
        updateSTFTSettings();
}

void GVSpectrogram::updateSTFTPlot(bool force){
//    DCOUT << "GVSpectrogram::updateSTFTPlot" << endl;

    if(!gMW->ui->actionShowSpectrogram->isChecked())
        return;

    // Fix limits between min and max sliders
    FTSound* csnd = gFL->getCurrentFTSound(true);
    if(csnd){
        if(csnd->m_actionShow->isChecked()) {
    //        cout << "GVSpectrogram::updateSTFTPlot " << csnd->fileFullPath.toLatin1().constData() << endl;

            if(force)
                csnd->m_imgSTFTParams.clear();

            int stepsize = std::floor(0.5+gFL->getFs()*m_dlgSettings->ui->sbSpectrogramStepSize->value());//[samples]
            int dftlen = -1;
            if(m_dlgSettings->ui->cbSpectrogramDFTSizeType->currentIndex()==0)
                dftlen = m_dlgSettings->ui->sbSpectrogramDFTSize->value();
            else if(m_dlgSettings->ui->cbSpectrogramDFTSizeType->currentIndex()==1)
                dftlen = std::pow(2.0, std::ceil(log2(float(m_win.size())))+m_dlgSettings->ui->sbSpectrogramOversamplingFactor->value());//[samples]
            int cepliftorder = -1;//[samples]
            if(gMW->m_gvSpectrogram->m_dlgSettings->ui->gbSpectrogramCepstralLiftering->isChecked())
                cepliftorder = gMW->m_gvSpectrogram->m_dlgSettings->ui->sbSpectrogramCepstralLifteringOrder->value();
            bool cepliftpresdc = gMW->m_gvSpectrogram->m_dlgSettings->ui->cbSpectrogramCepstralLifteringPreserveDC->isChecked();

            STFTComputeThread::STFTParameters reqSTFTParams(csnd, m_win, stepsize, dftlen, cepliftorder, cepliftpresdc);
            STFTComputeThread::ImageParameters reqImgSTFTParams(reqSTFTParams, &(csnd->m_imgSTFT), m_dlgSettings->ui->cbSpectrogramColorMaps->currentIndex(), m_dlgSettings->ui->cbSpectrogramColorMapReversed->isChecked(), gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0, gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0, m_dlgSettings->ui->cbSpectrogramLoudnessWeighting->isChecked(), m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex());

            if(csnd->m_imgSTFTParams.isEmpty() || reqImgSTFTParams!=csnd->m_imgSTFTParams) {
                gMW->ui->pbSpectrogramSTFTUpdate->hide();
                m_stftcomputethread->compute(reqImgSTFTParams);
            }
        }
        // m_scene->update(); // Should not be called here, otherwise creates intermediate black background
    }

//    COUTD << "GVSpectrogram::~updateSTFTPlot" << endl;
}


void GVSpectrogram::updateSceneRect() {
    m_scene->setSceneRect(-1.0/gFL->getFs(), -gFL->getFs()/2.0, gFL->getMaxDuration()+1.0/gFL->getFs(), gFL->getFs()/2.0);
    m_scene->update();
}

void GVSpectrogram::allSoundsChanged(){
    if(gFL->ftsnds.size()>0)
        updateSTFTPlot();
}

void GVSpectrogram::viewSet(QRectF viewrect, bool forwardsync) {

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

//        DCOUT << m_scene->sceneRect() << endl;

        if(viewrect.top()<=m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top());
        if(viewrect.bottom()>=m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom());
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        fitInView(removeHiddenMargin(this, viewrect));

        updateTextsGeometry();
        m_giGrid->updateLines();

        if(forwardsync){
            if(gMW->m_gvWaveform && !gMW->m_gvWaveform->viewport()->size().isEmpty()) { // && gMW->ui->actionShowWaveform->isChecked()
                QRectF currect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
                currect.setLeft(viewrect.left());
                currect.setRight(viewrect.right());
                gMW->m_gvWaveform->viewSet(currect, false);
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

void GVSpectrogram::resizeEvent(QResizeEvent* event){
    // Note: Resized is called for all views so better to not forward modifications
//    cout << "GVSpectrogram::resizeEvent" << endl;

    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {

        updateSceneRect();

        if(gMW->m_gvWaveform->viewport()->rect().width()*gMW->m_gvWaveform->viewport()->rect().height()>0){
            QRectF spectrorect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(spectrorect.left());
            viewrect.setRight(spectrorect.right());
            viewrect.setTop(-gFL->getFs()/2);
            viewrect.setBottom(0.0);
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);
    }
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    updateTextsGeometry();
    setMouseCursorPosition(QPointF(-1,0), false);
//    cout << "GVSpectrogram::~resizeEvent" << endl;
}

void GVSpectrogram::scrollContentsBy(int dx, int dy) {
//    DCOUT << "GVSpectrogram::scrollContentsBy " << dx << " " << dy << endl;

    setMouseCursorPosition(QPointF(-1,0), false); // TODO Not sure it's safe to call this here. E.g. updateGeometry is not welcomed here.

    QGraphicsView::scrollContentsBy(dx, dy);

    m_giGrid->updateLines();
}

void GVSpectrogram::wheelEvent(QWheelEvent* event) {

    qreal numDegrees = (event->angleDelta() / 8).y();
    // Clip to avoid flipping (workaround of a Qt bug ?)
    if(numDegrees>90) numDegrees = 90;
    if(numDegrees<-90) numDegrees = -90;

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

//    cout << "GVSpectrogram::wheelEvent: " << viewrect << endl;

    if(event->modifiers().testFlag(Qt::ShiftModifier)){
        QScrollBar* sb = horizontalScrollBar();
        sb->setValue(sb->value()-3*numDegrees);
    }
    else if((numDegrees>0 && viewrect.width()>10.0/gFL->getFs()) || numDegrees<0) {
        double gx = double(mapToScene(event->pos()).x()-viewrect.left())/viewrect.width();
        double gy = double(mapToScene(event->pos()).y()-viewrect.top())/viewrect.height();
        QRectF newrect = mapToScene(viewport()->rect()).boundingRect();
        newrect.setLeft(newrect.left()+gx*0.01*viewrect.width()*numDegrees);
        newrect.setRight(newrect.right()-(1-gx)*0.01*viewrect.width()*numDegrees);
        newrect.setTop(newrect.top()+gy*0.01*viewrect.height()*numDegrees);
        newrect.setBottom(newrect.bottom()-(1-gy)*0.01*viewrect.height()*numDegrees);
        if(newrect.width()<10.0/gFL->getFs()){
           newrect.setLeft(newrect.center().x()-5.0/gFL->getFs());
           newrect.setRight(newrect.center().x()+5.0/gFL->getFs());
        }

        viewSet(newrect);
    }

    QPointF p = mapToScene(event->pos());
    setMouseCursorPosition(p, true);
}

void GVSpectrogram::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_giShownSelection->boundingRect()).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setMouseCursorPosition(QPointF(-1,0), false);
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
            else if((m_selection.width()>0 && m_selection.height()>0) && (event->modifiers().testFlag(Qt::ControlModifier) || (event->x()>=selview.left() && event->x()<=selview.right() && event->y()>=selview.top() && event->y()<=selview.bottom()))){
                // When scroling the selection
                m_currentAction = CAMovingSelection;
//                COUTD << m_mouseSelection << " " << m_mouseSelection.width() << "x" << m_mouseSelection.height() << endl;
                m_topismax = m_mouseSelection.top()<=-gFL->getFs()/2.0;
                m_bottomismin = m_mouseSelection.bottom()>=0.0;
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
                selectionSet(m_mouseSelection);
            }
        }
        if(gMW->ui->actionEditMode->isChecked()){
            if(!event->modifiers().testFlag(Qt::ControlModifier) && !event->modifiers().testFlag(Qt::ShiftModifier)){
                FTFZero* current_fzero = gFL->getCurrentFTFZero(false);
                if(current_fzero){
                    m_currentAction = CAEditFZero;
                    m_editing_fzero = current_fzero;
                    m_selection_pressedp = p;
                    m_editing_fzero->edit(p.x(), -p.y());
                    m_scene->update();
                    gMW->m_gvSpectrumAmplitude->update();
                    current_fzero->setEditing(true);
                }
                else
                    playCursorSet(p.x(), true); // Place the play cursor
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

            QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//            COUTD << viewrect << std::endl;
            // If the mouse is close enough to a border, set to it
//            COUTD << std::abs(viewrect.top()-m_scene->sceneRect().top()) << " " << std::abs(m_pressed_mouseinviewport.y()-viewport()->rect().top()) << std::endl;
//            COUTD << std::numeric_limits<float>::epsilon() << std::endl;
            if(std::abs(viewrect.bottom()-m_scene->sceneRect().bottom())<std::numeric_limits<float>::epsilon()
               && std::abs(m_pressed_mouseinviewport.y()-viewport()->rect().bottom())<20)
                m_selection_pressedp.setY(m_scene->sceneRect().bottom());
            if(std::abs(viewrect.top()-m_scene->sceneRect().top())<std::numeric_limits<float>::epsilon()
               && std::abs(m_pressed_mouseinviewport.y()-viewport()->rect().top())<20)
                m_selection_pressedp.setY(m_scene->sceneRect().top());
            if(std::abs(viewrect.left()-m_scene->sceneRect().left())==0
               && std::abs(m_pressed_mouseinviewport.x()-viewport()->rect().left())<20)
                m_selection_pressedp.setX(m_scene->sceneRect().left());
            if(std::abs(viewrect.right()-m_scene->sceneRect().right())==0
               && std::abs(m_pressed_mouseinviewport.x()-viewport()->rect().right())<20)
                m_selection_pressedp.setX(m_scene->sceneRect().right());
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void GVSpectrogram::mouseMoveEvent(QMouseEvent* event){
//    COUTD << "GVSpectrogram::mouseMoveEvent" << endl;

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p, true);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
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

        updateTextsGeometry();

        setMouseCursorPosition(m_selection_pressedp, true);

        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
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
        if(m_topismax) m_mouseSelection.setTop(-gFL->getFs()/2.0);
        if(m_bottomismin) m_mouseSelection.setBottom(0.0);
        selectionSet(m_mouseSelection, true);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionSet(m_mouseSelection, true);
    }
    else if (m_currentAction==CAEditFZero){
        // Editing an F0 curve
        if(p!=m_selection_pressedp){
            m_editing_fzero->edit(p.x(), -p.y());
            m_selection_pressedp = p;
            m_scene->update();
            gMW->m_gvSpectrumAmplitude->update();
        }
    }
    else{
        QRect selview = mapFromScene(m_giShownSelection->boundingRect()).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.left()-event->x())<5
                        && event->y()>=selview.top() && event->y()<=selview.bottom())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.right()-event->x())<5
                        && selview.top()<=event->y() && selview.bottom()>=event->y())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.top()-event->y())<5
                        && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(m_selection.width()>0 && m_selection.height()>0
                        && abs(selview.bottom()-event->y())<5
                        && event->x()>=selview.left() && event->x()<=selview.right()) // If inside the time interv
                    setCursor(Qt::SplitVCursor);
                else if(event->x()>=selview.left() && event->x()<=selview.right() && event->y()>=selview.top() && event->y()<=selview.bottom())
                    setCursor(Qt::OpenHandCursor);
                else
                    setCursor(Qt::CrossCursor);

                FTFZero* curfzero = gFL->getCurrentFTFZero();
                if(curfzero && gMW->m_gvSpectrogram->m_aSpectrogramShowHarmonics->isChecked()){
                    // Get the clostest harmonic
                    double ct = p.x();
//                    double cf0 = qae::nearest<double>(curfzero->ts, curfzero->f0s, ct);
                    double cf0 = qae::interp_stepatzeros<double>(curfzero->ts, curfzero->f0s, ct);
                    if(cf0>0){
                        int h = int(-p.y()/cf0 +0.5);
                        curfzero->m_giHarmonicForSpectrogram->setGain(h);
                        curfzero->m_giHarmonicForSpectrogram->show();
                        m_scene->update(); // Can make it lighter ?
                    }
                }
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                FTFZero* curf0 = gFL->getCurrentFTFZero(false);
                if(curf0){
//                    setCursor(Qt::Pen); // TODO Set a pen cursor
                }
            }
        }
    }

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
    QGraphicsView::mouseMoveEvent(event);
}

void GVSpectrogram::mouseReleaseEvent(QMouseEvent* event) {
//    COUTD << "GVSpectrogram::mouseReleaseEvent" << endl;

    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);

    if(gMW->ui->actionEditMode->isChecked())
        if(m_currentAction==CAEditFZero)
            m_editing_fzero->setEditing(false);

    m_currentAction = CANothing;

    gMW->updateMouseCursorState(kshift, kctrl);

    QPointF p = mapToScene(event->pos());
    if(gMW->ui->actionSelectionMode->isChecked()) {
        if(!kshift && !kctrl){
            if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                setCursor(Qt::OpenHandCursor);
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

void GVSpectrogram::keyPressEvent(QKeyEvent* event){

//    cout << "GVSpectrogram::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape) {
        if(!gMW->m_gvWaveform->hasSelection()
            && !gMW->m_gvWaveform->hasSelection())
            gMW->m_gvWaveform->playCursorSet(0.0, true);
        selectionClear(true);
    }
    if(event->key()==Qt::Key_S)
        selectionZoomOn();

    QGraphicsView::keyPressEvent(event);
}

void GVSpectrogram::selectionClear(bool forwardsync) {
    m_giShownSelection->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    selectionSetTextInForm();

    if(forwardsync){
        if(gMW->m_gvWaveform)
            gMW->m_gvWaveform->selectionClear(false);
        if(gMW->m_gvSpectrumAmplitude)
            gMW->m_gvSpectrumAmplitude->selectionClear(false);
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->selectionClear(false);
    }
}

void GVSpectrogram::selectionSetTextInForm() {

    QString str;

//    cout << "GVSpectrogram::selectionSetText: " << m_selection.height() << " " << gMW->m_gvPhaseSpectrum->m_selection.height() << endl;

//    if (m_selection.height()==0 && gMW->m_gvPhaseSpectrum->m_selection.height()==0) {
    if (m_selection.height()==0 && m_selection.width()==0) {
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
        str += QString("[%1,%2]%3s").arg(left, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(right, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()).arg(right-left, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value());

        if (m_selection.height()>0) {
            // TODO The two lines below cannot be avoided exept by reversing the y coordinate of the
            //      whole seen, and I don't know how to do this :(
//            double lower = gFL->getFs()/2-m_selection.bottom();
//            if(std::abs(lower)<1e-10) lower=0.0;
//            str += QString(" x [%4,%5]%6Hz").arg(lower).arg(gFL->getFs()/2-m_selection.top()).arg(m_selection.height());
            str += QString(" x [%4,%5]%6Hz").arg(-m_selection.bottom()).arg(-m_selection.top()).arg(m_selection.height());
        }
    }

    gMW->ui->lblSpectrogramSelectionTxt->setText(str);
}

void GVSpectrogram::selectionSet(QRectF selection, bool forwardsync) {
    double fs = gFL->getFs();

    // Order the selection to avoid negative width and negative height
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }
    if(selection.top()>selection.bottom()){
        float tmp = selection.top();
        selection.setTop(selection.bottom());
        selection.setBottom(tmp);
    }

    m_selection = m_mouseSelection = selection;

    if(m_selection.left()<0) m_selection.setLeft(0);
    if(m_selection.right()>gFL->getFs()/2.0) m_selection.setRight(gFL->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

    gMW->m_gvWaveform->fixTimeLimitsToSamples(m_selection, m_mouseSelection, m_currentAction);

    m_giShownSelection->setRect(m_selection.left()-0.5/fs, m_selection.top(), m_selection.width()+1.0/fs, m_selection.height());
    m_giShownSelection->show();

    updateTextsGeometry();

    selectionSetTextInForm();

    playCursorSet(m_selection.left(), true); // Put the play cursor

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());

    if(forwardsync){
        if(gMW->m_gvWaveform) {
            if(gMW->m_gvWaveform->m_selection.left()!=m_selection.left()
               || gMW->m_gvWaveform->m_selection.right()!=m_selection.right()) {
                gMW->m_gvWaveform->selectionSet(m_selection, false);
            }
        }

        if(gMW->m_gvSpectrumAmplitude){
            if(m_selection.height()>=gFL->getFs()/2){
                gMW->m_gvSpectrumAmplitude->selectionClear();
            }
            else{
                QRectF rect = gMW->m_gvSpectrumAmplitude->m_mouseSelection;
                rect.setLeft(-m_selection.bottom());
                rect.setRight(-m_selection.top());
                if(rect.height()==0){
                    rect.setTop(gMW->m_gvSpectrumAmplitude->m_scene->sceneRect().top());
                    rect.setBottom(gMW->m_gvSpectrumAmplitude->m_scene->sceneRect().bottom());
                }
                gMW->m_gvSpectrumAmplitude->selectionSet(rect, false);
            }
        }
        if(gMW->m_gvSpectrumPhase){
            if(m_selection.height()>=gFL->getFs()/2){
                gMW->m_gvSpectrumPhase->selectionClear();
            }
            else{
                QRectF rect = gMW->m_gvSpectrumPhase->m_mouseSelection;
                rect.setLeft(-m_selection.bottom());
                rect.setRight(-m_selection.top());
                if(rect.height()==0){
                    rect.setTop(gMW->m_gvSpectrumPhase->m_scene->sceneRect().top());
                    rect.setBottom(gMW->m_gvSpectrumPhase->m_scene->sceneRect().bottom());
                }
                gMW->m_gvSpectrumPhase->selectionSet(rect, false);
            }
        }
        if(gMW->m_gvSpectrumGroupDelay){
            if(m_selection.height()>=gFL->getFs()/2){
                gMW->m_gvSpectrumGroupDelay->selectionClear();
            }
            else{
                QRectF rect = gMW->m_gvSpectrumGroupDelay->m_mouseSelection;
                rect.setLeft(-m_selection.bottom());
                rect.setRight(-m_selection.top());
                if(rect.height()==0){
                    rect.setTop(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().top());
                    rect.setBottom(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().bottom());
                }
                gMW->m_gvSpectrumGroupDelay->selectionSet(rect, false);
            }
        }
    }

    selectionSetTextInForm();
}

void GVSpectrogram::updateTextsGeometry() {
    QTransform trans = transform();
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(), 1.0/trans.m22());

    // Mouse Cursor
    m_giMouseCursorTxtTime->setTransform(txttrans);
    m_giMouseCursorTxtFreq->setTransform(txttrans);

    // Labels
    for(size_t fi=0; fi<gFL->ftlabels.size(); fi++){
        if(!gFL->ftlabels[fi]->m_actionShow->isChecked())
            continue;

        gFL->ftlabels[fi]->updateTextsGeometrySpectrogram();
    }
}

void GVSpectrogram::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
            zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
            zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
            zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        }
        viewSet(zoomonrect);

        updateTextsGeometry();
//        if(gMW->m_gvPhaseSpectrum)
//            gMW->m_gvPhaseSpectrum->viewUpdateTexts();
//        if(gMW->m_gvAmplitudeSpectrum)
//            gMW->m_gvAmplitudeSpectrum->viewUpdateTexts();

        setMouseCursorPosition(QPointF(-1,0), false);
//        m_aZoomOnSelection->setEnabled(false);
    }
}

void GVSpectrogram::azoomin(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void GVSpectrogram::azoomout(){
    DFLAG
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void GVSpectrogram::aunzoom(){
    viewSet(m_scene->sceneRect(), true);
}

void GVSpectrogram::setMouseCursorPosition(QPointF p, bool forwardsync) {
    if(p.x()==-1){
        m_giMouseCursorLineFreqBack->hide();
        m_giMouseCursorLineTimeBack->hide();
        m_giMouseCursorLineFreq->hide();
        m_giMouseCursorLineTime->hide();
        m_giMouseCursorTxtTimeBack->hide();
        m_giMouseCursorTxtFreqBack->hide();
        m_giMouseCursorTxtTime->hide();
        m_giMouseCursorTxtFreq->hide();
    }
    else {

        m_giMouseCursorLineTimeBack->setPos(p.x(), 0.0);
        m_giMouseCursorLineFreqBack->setPos(0.0, p.y());
        m_giMouseCursorLineTime->setPos(p.x(), 0.0);
        m_giMouseCursorLineFreq->setPos(0.0, p.y());

        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());

        m_giMouseCursorLineTimeBack->setLine(1.0/trans.m11(), viewrect.top(), 1.0/trans.m11(), viewrect.top()+18/trans.m22());
        m_giMouseCursorLineTimeBack->show();
        m_giMouseCursorLineFreqBack->setLine(viewrect.right()-50/trans.m11(), 1.0/trans.m22(), gFL->getFs()/2.0, 1.0/trans.m22());
        m_giMouseCursorLineFreqBack->show();
        m_giMouseCursorLineTime->setLine(0.0, viewrect.top(), 0.0, viewrect.top()+18/trans.m22());
        m_giMouseCursorLineTime->show();
        m_giMouseCursorLineFreq->setLine(viewrect.right()-50/trans.m11(), 0.0, gFL->getFs()/2.0, 0.0);
        m_giMouseCursorLineFreq->show();

        QFont darkfont = gMW->m_dlgSettings->ui->lblGridFontSample->font();
        darkfont.setBold(true);

        QRectF br = m_giMouseCursorTxtTime->boundingRect();
        qreal x = p.x()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());
        QString txt = QString("%1s").arg(p.x(), 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value());
        m_giMouseCursorTxtTimeBack->setText(txt);
        m_giMouseCursorTxtTimeBack->setPos(x+1.0/trans.m11(), viewrect.top()-(3-1.0)/trans.m22());
        m_giMouseCursorTxtTimeBack->setTransform(txttrans);
        m_giMouseCursorTxtTimeBack->setFont(darkfont);
        m_giMouseCursorTxtTimeBack->show();
        m_giMouseCursorTxtTime->setText(txt);
        m_giMouseCursorTxtTime->setPos(x, viewrect.top()-3/trans.m22());
        m_giMouseCursorTxtTime->setTransform(txttrans);
        m_giMouseCursorTxtTime->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giMouseCursorTxtTime->show();

//        txt = QString("%1Hz").arg(0.5*gFL->getFs()-p.y());
        txt = QString("%1Hz").arg(-p.y());
        m_giMouseCursorTxtFreqBack->setText(txt);
        br = m_giMouseCursorTxtFreqBack->boundingRect();
        m_giMouseCursorTxtFreqBack->setPos(viewrect.right()-(br.width()-2.0)/trans.m11(), p.y()-(br.height()-1.0)/trans.m22());
        m_giMouseCursorTxtFreqBack->setTransform(txttrans);
        m_giMouseCursorTxtFreqBack->setFont(darkfont);
        m_giMouseCursorTxtFreqBack->show();
        m_giMouseCursorTxtFreq->setText(txt);
        br = m_giMouseCursorTxtFreq->boundingRect();
        m_giMouseCursorTxtFreq->setPos(viewrect.right()-br.width()/trans.m11(), p.y()-br.height()/trans.m22());
        m_giMouseCursorTxtFreq->setTransform(txttrans);
        m_giMouseCursorTxtFreq->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giMouseCursorTxtFreq->show();

        if(forwardsync){
            if(gMW->m_gvWaveform)
                gMW->m_gvWaveform->setMouseCursorPosition(p.x(), false);
            if(gMW->m_gvSpectrumAmplitude)
                gMW->m_gvSpectrumAmplitude->setMouseCursorPosition(QPointF(-p.y(), 0.0), false);
            if(gMW->m_gvSpectrumPhase)
                gMW->m_gvSpectrumPhase->setMouseCursorPosition(QPointF(-p.y(), 0.0), false);
            if(gMW->m_gvSpectrumGroupDelay)
                gMW->m_gvSpectrumGroupDelay->setMouseCursorPosition(QPointF(-p.y(), 0.0), false);
        }
    }
}

void GVSpectrogram::playCursorSet(double t, bool forwardsync){
    if(t==-1){
        if(m_selection.width()>0)
            playCursorSet(m_selection.left(), forwardsync);
        else
            m_giPlayCursor->setPos(QPointF(gMW->m_gvWaveform->m_initialPlayPosition, 0));
    }
    else{
        m_giPlayCursor->setPos(QPointF(t, 0));
    }

    if(gMW->m_gvWaveform && forwardsync)
        gMW->m_gvWaveform->playCursorSet(t, false);
}

void GVSpectrogram::drawBackground(QPainter* painter, const QRectF& rect){
    Q_UNUSED(rect)
//    cout << QTime::currentTime().toString("hh:mm:ss.zzz").toLocal8Bit().constData() << ": GVSpectrogram::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    updateTextsGeometry(); // TODO Since called here, can be removed from many other places

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    // Draw the sound's spectrogram
    if(QAEColorMap::getAt(m_dlgSettings->ui->cbSpectrogramColorMaps->currentIndex()).isTransparent()){
        QPainter::CompositionMode compmode = painter->compositionMode();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->fillRect(viewrect, Qt::transparent);
//        painter->fillRect(viewrect, Qt::black);
        painter->setCompositionMode(QPainter::CompositionMode_Plus);
        // If the color mapping is partly transparent, draw the spectro of all sounds
        for(size_t fi=0; fi<gFL->ftsnds.size(); ++fi)
            draw_spectrogram(painter, rect, viewrect, gFL->ftsnds[fi]);
        painter->setCompositionMode(QPainter::CompositionMode_DestinationOver);
        painter->fillRect(viewrect, Qt::white);
        painter->setCompositionMode(compmode);
    }
    else{
        // If the color mapping is opaque, draw only the spectro of the current sound
        FTSound* csnd = gFL->getCurrentFTSound(true);
        if(csnd && csnd->m_actionShow->isChecked())
            draw_spectrogram(painter, rect, viewrect, csnd);
    }

//    cout << "GVSpectrogram::~drawBackground" << endl;
}

void GVSpectrogram::draw_spectrogram(QPainter* painter, const QRectF& rect, const QRectF& viewrect, FTSound* snd){
    Q_UNUSED(rect)

    gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_stftts.lock();
    if(snd==NULL
      || !snd->m_actionShow->isChecked()
      || snd->m_imgSTFT.isNull()
      || snd->m_stftts.empty()) // First need to be computed
    {
        gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_stftts.unlock();
        return;
    }

    double stftwidth = snd->m_stftts.back()-snd->m_stftts.front();

    QRect imgrect = snd->m_imgSTFT.rect();

    // Build the piece of STFT which will be drawn in the view
    QRectF srcrect;
    srcrect.setLeft(0.5+(imgrect.width()-1)*(viewrect.left()-snd->m_stftts.front())/stftwidth);
    srcrect.setRight(0.5+(imgrect.width()-1)*(viewrect.right()-snd->m_stftts.front())/stftwidth);
//        COUTD << "viewrect=" << viewrect << endl;
//        COUTD << "IMG: " << m_imgSTFT.rect() << endl;
    // This one is vertically super sync,
    // but the cursor falls always on the top of the line, not in the middle of it.
//        srcrect.setTop((m_imgSTFT.rect().height()-1)*viewrect.top()/m_scene->sceneRect().height());
//        srcrect.setBottom((m_imgSTFT.rect().height()-1)*viewrect.bottom()/m_scene->sceneRect().height());
    srcrect.setTop((imgrect.height()-1)*-(m_scene->sceneRect().top()-viewrect.top())/m_scene->sceneRect().height());
    srcrect.setBottom((imgrect.height()-1)*-(m_scene->sceneRect().top()-viewrect.bottom())/m_scene->sceneRect().height());
    srcrect.setTop(srcrect.top()+0.5);
    srcrect.setBottom(srcrect.bottom()+0.5);
    QRectF trgrect = viewrect;

    // This one is the basic synchronized version,
    // but it creates flickering when zooming
    //QRectF srcrect = m_imgSTFT.rect();
    //QRectF trgrect = m_scene->sceneRect();
    //trgrect.setLeft(csnd->m_stftts.front()-0.5*csnd->m_stftparams.stepsize/csnd->fs);
    //trgrect.setRight(csnd->m_stftts.back()+0.5*csnd->m_stftparams.stepsize/csnd->fs);
    //double bin2hz = fs*1/csnd->m_stftparams.dftlen;
    //trgrect.setTop(-bin2hz/2);// Hard to verify because of the flickering
    //trgrect.setBottom(fs/2+bin2hz/2);// Hard to verify because of the flickering

    //COUTD << "Scene: " << m_scene->sceneRect() << " " << m_scene->sceneRect().width() << "x" << m_scene->sceneRect().height() << endl;
    //COUTD << "SRC: " << srcrect << " " << srcrect.width() << "x" << srcrect.height() << endl;
    //COUTD << "TRG: " << trgrect << " " << trgrect.width() << "x" << trgrect.height() << endl;

    gMW->m_gvSpectrogram->m_stftcomputethread->m_mutex_stftts.unlock();

    if(m_stftcomputethread->m_mutex_imageallocation.tryLock()) {
        painter->drawImage(trgrect, snd->m_imgSTFT, srcrect);
        m_stftcomputethread->m_mutex_imageallocation.unlock();
    }
}

void GVSpectrogram::contextMenuEvent(QContextMenuEvent *event){
    if (event->modifiers().testFlag(Qt::ShiftModifier)
        || event->modifiers().testFlag(Qt::ControlModifier))
        return;

    QPoint posglobal = mapToGlobal(event->pos()+QPoint(0,0));
    m_contextmenu.exec(posglobal);
}

GVSpectrogram::~GVSpectrogram(){
    m_stftcomputethread->m_mutex_computing.lock();
    m_stftcomputethread->m_mutex_computing.unlock();
    m_stftcomputethread->m_mutex_changingparams.lock();
    m_stftcomputethread->m_mutex_changingparams.unlock();
    m_stftcomputethread->wait();
    delete m_stftcomputethread;
    delete m_dlgSettings;

    delete m_aAutoUpdate;
    delete m_aSpectrogramShowHarmonics;
    delete m_aSpectrogramShowGrid;
    delete m_aShowProperties;
}
