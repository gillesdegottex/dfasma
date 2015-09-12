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

#include "wmainwindow.h"
#include "ui_wmainwindow.h"

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "wdialogselectchannel.h"
#include "ui_wdialogselectchannel.h"

#include "../external/libqxt/qxtspanslider.h"

#include "fileslistwidget.h"
#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "ftlabels.h"
#include "../external/audioengine/audioengine.h"
#include "aboutbox.h"

#include <math.h>
#include <iostream>
#include <algorithm>
#include <limits>
#include <fstream>
using namespace std;

#include <QGraphicsWidget>
#include <QToolBar>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include <QSplitter>
#include <QDebug>
#include <QGLWidget>
#include <QAudioDeviceInfo>
#include <QStaticText>
#include <QTextEdit>
#include <QIcon>
#include <QTime>
#include <QtMultimedia/QSound>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QProgressDialog>
#include "qaehelpers.h"

#ifdef SUPPORT_SDIF
    #include <easdif/easdif.h>
#endif

WMainWindow* gMW = NULL;

WMainWindow::WMainWindow(QStringList files, QWidget *parent)
    : QMainWindow(parent)
    , m_last_file_editing(NULL)
    , m_lastFilteredSound(NULL)
    , m_dlgSettings(NULL)
    , ui(new Ui::WMainWindow)
    , m_gvWaveform(NULL)
    , m_gvAmplitudeSpectrum(NULL)
    , m_gvPhaseSpectrum(NULL)
    , m_gvSpectrumGroupDelay(NULL)
    , m_gvSpectrogram(NULL)
    , m_audioengine(NULL)
    , m_playingftsound(NULL)
{ 
    gMW = this;

    m_loading = true;

    // Prepare the UI (everything which is independent of settings)
    ui->setupUi(this);
    ui->pbSpectrogramSTFTUpdate->hide();
    m_qxtSpectrogramSpanSlider = new QxtSpanSlider(Qt::Vertical, this);
    m_qxtSpectrogramSpanSlider->setMinimum(0);
    m_qxtSpectrogramSpanSlider->setMaximum(100);
    m_qxtSpectrogramSpanSlider->setUpperValue(90);
    m_qxtSpectrogramSpanSlider->setLowerValue(30);
    m_qxtSpectrogramSpanSlider->setMinimumWidth(18);
    m_qxtSpectrogramSpanSlider->setMaximumWidth(18);
    m_qxtSpectrogramSpanSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_qxtSpectrogramSpanSlider->setMaximumWidth(ui->sldWaveformAmplitude->maximumWidth());
    m_qxtSpectrogramSpanSlider->setHandleMovementMode(QxtSpanSlider::NoOverlapping);
    ui->lSpectrogramSliders->insertWidget(1, m_qxtSpectrogramSpanSlider);

    m_dlgSettings = new WDialogSettings(this);

    m_fileslist = new FilesListWidget(this);
    ui->vlFilesList->addWidget(m_fileslist);
    ui->lblFileInfo->hide();

    ui->mainToolBar->setIconSize(QSize(1.5*m_dlgSettings->ui->sbViewsToolBarSizes->value(),1.5*m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    connect(m_dlgSettings->ui->sbFileListItemSize, SIGNAL(valueChanged(int)), gFL, SLOT(changeFileListItemsSize()));
    m_fileslist->changeFileListItemsSize();

    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));
    connect(ui->actionSelectedFilesReload, SIGNAL(triggered()), gFL, SLOT(selectedFilesReload()));
    connect(ui->actionSelectedFilesToggleShown, SIGNAL(triggered()), gFL, SLOT(selectedFilesToggleShown()));
    connect(ui->actionSelectedFilesClose, SIGNAL(triggered()), gFL, SLOT(selectedFilesClose()));
    connect(ui->actionSelectedFilesDuplicate, SIGNAL(triggered()), gFL, SLOT(selectedFilesDuplicate()));
    connect(ui->actionSelectedFilesSave, SIGNAL(triggered()), gFL, SLOT(selectedFilesSave()));
    connect(ui->actionEstimationF0, SIGNAL(triggered()), gFL, SLOT(selectedFilesEstimateF0()));
    connect(ui->actionEstimationF0Forced, SIGNAL(triggered()), gFL, SLOT(selectedFilesEstimateF0()));

    ui->statusBar->setStyleSheet("QStatusBar::item { border: 0px solid black }; ");
    m_globalWaitingBar = new QProgressBar(ui->statusBar);
    m_globalWaitingBar->setAlignment(Qt::AlignRight);
    m_globalWaitingBar->setMaximumSize(100, 14);
    m_globalWaitingBar->setValue(50);
    ui->statusBar->addPermanentWidget(m_globalWaitingBar);
    m_globalWaitingBar->hide();

    setAcceptDrops(true);
    m_fileslist->setAcceptDrops(true);
    m_fileslist->setSelectionRectVisible(false);
    m_fileslist->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileslist, SIGNAL(customContextMenuRequested(const QPoint&)), gFL, SLOT(showFileContextMenu(const QPoint&)));

    ui->actionAbout->setIcon(QIcon(":/icons/about.svg"));
    ui->actionSettings->setIcon(QIcon(":/icons/settings.svg"));
    ui->actionFileOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionFileNew->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(ui->actionFileNew, SIGNAL(triggered()), this, SLOT(newFile()));
    ui->actionSelectedFilesClose->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionSelectedFilesClose->setEnabled(false);
    ui->actionSelectedFilesReload->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->actionSelectedFilesReload->setEnabled(false);
    ui->actionSelectedFilesToggleShown->setEnabled(false);
    ui->actionSelectedFilesSave->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->actionSelectedFilesReload->setEnabled(false);
    connect(ui->actionSettings, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    ui->actionSelectionMode->setChecked(true);
    connectModes();
    connect(m_fileslist, SIGNAL(itemSelectionChanged()), gFL, SLOT(fileSelectionChanged()));
    addAction(ui->actionSelectedFilesToggleShown);
    addAction(ui->actionSelectedFilesReload);
    addAction(ui->actionSelectedFilesDuplicate);
    addAction(ui->actionSelectedFilesSave);
    addAction(ui->actionPlayFiltered);
    addAction(ui->actionEstimationF0);
    addAction(ui->actionEstimationF0Forced);

    // Audio engine for playing the selections
    ui->actionPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->actionPlay->setEnabled(false);
    connect(ui->actionPlay, SIGNAL(triggered()), this, SLOT(play()));
    connect(ui->actionPlayFiltered, SIGNAL(triggered()), this, SLOT(play()));
    m_pbVolume = new QProgressBar(this);
    m_pbVolume->setOrientation(Qt::Vertical);
    m_pbVolume->setTextVisible(false);
//    m_pbVolume->setMaximumWidth(75);
    m_pbVolume->setMaximumWidth(m_dlgSettings->ui->sbViewsToolBarSizes->value()/2);
    m_pbVolume->setMaximum(0);
//    m_pbVolume->setMaximumHeight(ui->mainToolBar->height()/2);
    m_pbVolume->setMaximumHeight(ui->mainToolBar->height());
    m_pbVolume->setMinimum(-50); // Quite arbitrary
    m_pbVolume->setValue(-50);   // Quite arbitrary
    m_pbVolume->setEnabled(false);
    ui->mainToolBar->insertWidget(ui->actionSettings, m_pbVolume);
    ui->mainToolBar->insertSeparator(ui->actionSettings);

    m_gvWaveform = new QGVWaveform(this);
    ui->lWaveformGraphicsView->addWidget(m_gvWaveform);

    // Spectra
    m_gvAmplitudeSpectrum = new QGVAmplitudeSpectrum(this);
    ui->lSpectrumAmplitudeGraphicsView->addWidget(m_gvAmplitudeSpectrum);
    m_settings.add(ui->actionShowAmplitudeSpectrum);
    ui->wSpectrumAmplitude->setVisible(ui->actionShowAmplitudeSpectrum->isChecked());

    m_gvPhaseSpectrum = new QGVPhaseSpectrum(this);
    ui->lSpectrumPhaseGraphicsView->addWidget(m_gvPhaseSpectrum);
    m_settings.add(ui->actionShowPhaseSpectrum);
    ui->wSpectrumPhase->setVisible(ui->actionShowPhaseSpectrum->isChecked());

    m_gvSpectrumGroupDelay = new QGVSpectrumGroupDelay(this);
    ui->lSpectrumGroupDelayGraphicsView->addWidget(m_gvSpectrumGroupDelay);
    m_settings.add(ui->actionShowGroupDelaySpectrum);
    ui->wSpectrumGroupDelay->setVisible(ui->actionShowGroupDelaySpectrum->isChecked());

    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked()
                             || ui->actionShowPhaseSpectrum->isChecked()
                             || ui->actionShowGroupDelaySpectrum->isChecked());

    // Spectrogram
    m_gvSpectrogram = new QGVSpectrogram(this);
    ui->lSpectrogramGraphicsView->addWidget(m_gvSpectrogram);
    m_settings.add(ui->actionShowSpectrogram);
    ui->wSpectrogram->setVisible(ui->actionShowSpectrogram->isChecked());
    connect(ui->pbSpectrogramSTFTUpdate, SIGNAL(clicked()), m_gvSpectrogram, SLOT(updateSTFTSettings()));

    m_gvAmplitudeSpectrum->updateScrollBars();
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), m_gvAmplitudeSpectrum, SLOT(updateScrollBars()));


    // Link axis' views
    connect(m_gvAmplitudeSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvPhaseSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvAmplitudeSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrumGroupDelay->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvPhaseSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvAmplitudeSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvPhaseSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrumGroupDelay->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvSpectrumGroupDelay->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvAmplitudeSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvSpectrumGroupDelay->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvPhaseSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));

    // Set visible views
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumAmplitude, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->sldAmplitudeSpectrumMin, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumPhase, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->lblSpectrumPhase, SLOT(setVisible(bool)));
    connect(ui->actionShowGroupDelaySpectrum, SIGNAL(toggled(bool)), ui->wSpectrumGroupDelay, SLOT(setVisible(bool)));
    connect(ui->actionShowGroupDelaySpectrum, SIGNAL(toggled(bool)), ui->lblSpectrumGroupDelay, SLOT(setVisible(bool)));
    connect(ui->actionShowSpectrogram, SIGNAL(toggled(bool)), ui->wSpectrogram, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowGroupDelaySpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(m_dlgSettings->ui->sbViewsToolBarSizes, SIGNAL(valueChanged(int)), this, SLOT(changeToolBarSizes(int)));
    viewsDisplayedChanged();

    // Load saved sizes of the views
    int onesize;
    QTextStream strMain(m_settings.value("splitterMain").toByteArray());
    QList<int> sizesMain;
    while(!strMain.atEnd()) {
        strMain >> onesize;
        sizesMain.append(onesize);
    }
    if(sizesMain.count()==0) {
        sizesMain.append(200);
        sizesMain.append(500);
    }
    ui->splitterMain->setSizes(sizesMain);

    QTextStream strViews(m_settings.value("splitterViews").toByteArray());
    QList<int> sizesViews;
    while(!strViews.atEnd()) {
        strViews >> onesize;
        sizesViews.append(onesize);
    }
    if(sizesViews.count()==0) {
        sizesViews.append(100);
        sizesViews.append(100);
        sizesViews.append(100);
    }
    ui->splitterViews->setSizes(sizesViews);

    QTextStream strSpectra(m_settings.value("splitterSpectra").toByteArray());
    QList<int> sizesSpectra;
    while(!strSpectra.atEnd()) {
        strSpectra >> onesize;
        sizesSpectra.append(onesize);
    }
//    for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++)
//        std::cout << *it << " ";
//    std::cout << std::endl;
    if(sizesSpectra.count()==0) {
        sizesSpectra.append(100);
        sizesSpectra.append(100);
        sizesSpectra.append(100);
    }
    ui->splitterSpectra->setSizes(sizesSpectra);

    // Start in open file mode
    // and show the panels only if a file has been loaded
    ui->splitterViews->hide();

    m_audioengine = new AudioEngine(this);
    if(m_audioengine) {
        connect(m_audioengine, SIGNAL(errorMessage(const QString &, const QString &)), this, SLOT(audioEngineError(const QString &, const QString &)));
        connect(m_audioengine, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioStateChanged(QAudio::State)));
        connect(m_audioengine, SIGNAL(formatChanged(const QAudioFormat&)), this, SLOT(audioOutputFormatChanged(const QAudioFormat&)));
        connect(m_audioengine, SIGNAL(playPositionChanged(double)), m_gvWaveform, SLOT(playCursorSet(double)));
        connect(m_audioengine, SIGNAL(localEnergyChanged(double)), this, SLOT(localEnergyChanged(double)));
        // List the audio devices and select the first one
        m_dlgSettings->ui->cbPlaybackAudioOutputDevices->clear();
        QList<QAudioDeviceInfo> audioDevices = m_audioengine->availableAudioOutputDevices();
        for(int di=0; di<audioDevices.size(); di++)
            m_dlgSettings->ui->cbPlaybackAudioOutputDevices->addItem(audioDevices[di].deviceName());
        if(m_dlgSettings->ui->cbPlaybackAudioOutputDevices->count()==0){
            m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText("<small>No audio device available.</small>");
            m_dlgSettings->ui->lblAudioOutputDeviceFormat->show();
//            m_dlgSettings->ui->cbPlaybackAudioOutputDevices->hide();
        }
        selectAudioOutputDevice(m_settings.value("cbPlaybackAudioOutputDevices", "default").toString());
        connect(m_dlgSettings->ui->cbPlaybackAudioOutputDevices, SIGNAL(currentIndexChanged(int)), this, SLOT(selectAudioOutputDevice(int)));
        m_dlgSettings->adjustSize();
    }

    // This one seems able to open distant files because file paths arrive in gvfs format
    // in the main.
    // Doesn't work any more (at least with sftp). The gvfs "miracle" might not be very reliable.
    m_fileslist->addExistingFiles(files);
    updateViewsAfterAddFile(true);

    if(files.size()>0)
        m_gvSpectrogram->updateSTFTSettings(); // This will update the window computation AND trigger the STFT computation
    gMW->ui->actionSelectedFilesClose->setEnabled(gFL->selectedItems().size()>0);

    connect(ui->actionFileOpen, SIGNAL(triggered()), this, SLOT(openFile())); // Alow this only at the end

    m_loading = false;
}

WMainWindow::~WMainWindow() {
//    COUTD << "WMainWindow::~WMainWindow" << endl;

    m_gvSpectrogram->m_stftcomputethread->cancelComputation(true);
    m_gvAmplitudeSpectrum->m_fftresizethread->cancelComputation(true);

    m_fileslist->selectAll();
    m_fileslist->selectedFilesClose();

    // The audio player
    if(m_audioengine){
        delete m_audioengine;
        m_audioengine=NULL;
    }

    // Delete views
    delete m_gvWaveform; m_gvWaveform=NULL;
    delete m_gvAmplitudeSpectrum; m_gvAmplitudeSpectrum=NULL;
    delete m_gvPhaseSpectrum; m_gvPhaseSpectrum=NULL;
    delete m_gvSpectrumGroupDelay; m_gvSpectrumGroupDelay=NULL;
    delete m_gvSpectrogram; m_gvSpectrogram=NULL;
    delete m_dlgSettings; m_dlgSettings=NULL;

    delete ui; ui=NULL; // The GUI
}

// Interface ===================================================================

void WMainWindow::changeToolBarSizes(int size) {
    gMW->m_gvWaveform->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvAmplitudeSpectrum->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvSpectrogram->m_toolBar->setIconSize(QSize(size,size));
    ui->mainToolBar->setIconSize(QSize(1.5*size,1.5*size));
    m_pbVolume->setMaximumWidth(m_dlgSettings->ui->sbViewsToolBarSizes->value()/2);
    m_pbVolume->setMaximumHeight(1.5*size);
}

void WMainWindow::updateWindowTitle() {
    int count = m_fileslist->count();
    if(count>0) setWindowTitle("DFasma ("+QString::number(count)+")");
    else        setWindowTitle("DFasma");
}

void WMainWindow::execAbout(){
    AboutBox box(this);
    box.exec();
}

void WMainWindow::globalWaitingBarMessage(const QString& statusmessage, int max){
    statusBar()->showMessage(statusmessage);
    m_globalWaitingBar->setMinimum(0);
    m_globalWaitingBar->setMaximum(max);
    m_globalWaitingBar->show();
    statusBar()->update();
    QCoreApplication::processEvents(); // To show the progress
}
void WMainWindow::globalWaitingBarSetMaximum(int max){
    m_globalWaitingBar->setMaximum(max);
}
void WMainWindow::globalWaitingBarSetValue(int value){
    m_globalWaitingBar->setValue(value);
}
void WMainWindow::globalWaitingBarDone(){
    m_globalWaitingBar->setMaximum(m_globalWaitingBar->maximum());
    statusBar()->showMessage(gMW->statusBar()->currentMessage()+" done", 3000);
    m_globalWaitingBar->hide();
    QCoreApplication::processEvents(); // To show the progress
}
void WMainWindow::globalWaitingBarClear(){
    statusBar()->clearMessage();
    m_globalWaitingBar->hide();
    QCoreApplication::processEvents(); // To show the progress
}

// File management =======================================================

void WMainWindow::newFile(){
    QMessageBox::StandardButton btn = QMessageBox::question(this, "Create a new file ...", "Do you want to create an empty label file?", QMessageBox::Yes | QMessageBox::No);
    if(btn==QMessageBox::Yes){
        m_fileslist->addItem(new FTLabels(this));
    }
}

void WMainWindow::openFile() {
//    COUTD << "WMainWindow::openFile" << endl;

    QString filters;
    filters += FileType::getTypeNameAndExtensions(FileType::FTUNSET);
    filters += ";;"+FileType::getTypeNameAndExtensions(FileType::FTSOUND);
    filters += ";;"+FileType::getTypeNameAndExtensions(FileType::FTFZERO);
    filters += ";;"+FileType::getTypeNameAndExtensions(FileType::FTLABELS);

    QString selectedFilter;
    QStringList files = QFileDialog::getOpenFileNames(this, "Open File(s)...", QString(), filters, &selectedFilter, QFileDialog::ReadOnly);

    // Use selectedFilter for pre-selecting the file type.
    FileType::FType type = FileType::FTUNSET;
    if(selectedFilter==FileType::getTypeNameAndExtensions(FileType::FTSOUND))
        type = FileType::FTSOUND;
    else if(selectedFilter==FileType::getTypeNameAndExtensions(FileType::FTFZERO))
        type = FileType::FTFZERO;
    else if(selectedFilter==FileType::getTypeNameAndExtensions(FileType::FTLABELS))
        type = FileType::FTLABELS;

//    COUTD << selectedFilter.toLatin1().constData() << ": " << type << endl;

    // Cancel button doesn't work the same way with the code below
//    QFileDialog dlg(this, "Open File(s)...", QString(), filter);
//    dlg.setFileMode(QFileDialog::DirectoryOnly);
//    dlg.setOption(QFileDialog::DontUseNativeDialog,true);
////    dlg.setFileMode(QFileDialog::Directory);
//    QListView *lv = dlg.findChild<QListView*>("listView");
//    if(lv)
//        lv->setSelectionMode(QAbstractItemView::MultiSelection);
//    QTreeView *t = dlg.findChild<QTreeView*>();
//    if(t)
//        t->setSelectionMode(QAbstractItemView::MultiSelection);
//    dlg.exec();
//    QStringList l = dlg.selectedFiles();

    if(files.size()>0) {
        bool isfirsts = m_fileslist->ftsnds.size()==0;
        m_fileslist->addExistingFiles(files, type);
        updateViewsAfterAddFile(isfirsts);
    }
}

void WMainWindow::dropEvent(QDropEvent *event){
//    COUTD << "Contents: " << event->mimeData()->text().toLatin1().data() << endl;

    QList<QUrl>	lurl = event->mimeData()->urls();

    // Need to call with Local file name, otherwise the path contains "file://"
    // However, remote file paths are not in an apropriate format for opening.
    QStringList files;
    for(int lurli=0; lurli<lurl.size(); lurli++)
        files.append(lurl[lurli].toLocalFile());
//        files.append(lurl[lurli].url());

    bool isfirsts = m_fileslist->ftsnds.size()==0;
    m_fileslist->addExistingFiles(files);
    updateViewsAfterAddFile(isfirsts);
}
void WMainWindow::dragEnterEvent(QDragEnterEvent *event){
    event->acceptProposedAction();
}

// Views =======================================================================

void WMainWindow::updateViewsAfterAddFile(bool isfirsts) {
    if(m_fileslist->count()==0)
        setInWaitingForFileState();
    else {
        ui->actionSelectedFilesClose->setEnabled(true);
        ui->actionSelectedFilesReload->setEnabled(true);
        ui->actionSelectedFilesToggleShown->setEnabled(true);
        gMW->ui->actionSelectedFilesSave->setEnabled(gFL->m_nb_labels_in_selection>0 || gFL->m_nb_fzeros_in_selection>0);
        ui->splitterViews->show();
        updateWindowTitle();
        m_gvWaveform->updateSceneRect();
        m_gvSpectrogram->updateSceneRect();
        m_gvAmplitudeSpectrum->updateAmplitudeExtent();
        m_gvPhaseSpectrum->updateSceneRect();
        m_gvSpectrumGroupDelay->updateSceneRect();
        if(isfirsts){
            m_gvWaveform->viewSet(m_gvWaveform->m_scene->sceneRect(), false);
            m_gvSpectrogram->viewSet(m_gvSpectrogram->m_scene->sceneRect(), false);
            m_gvAmplitudeSpectrum->viewSet(m_gvAmplitudeSpectrum->m_scene->sceneRect(), false);
            m_gvPhaseSpectrum->viewSet(m_gvPhaseSpectrum->m_scene->sceneRect(), false);
            m_gvSpectrumGroupDelay->viewSet(m_gvSpectrumGroupDelay->m_scene->sceneRect(), false);
            ui->actionFileNew->setEnabled(true);
        }
        allSoundsChanged();
        m_gvSpectrogram->updateSTFTSettings(); // This will update the window computation AND trigger the STFT computation
    }
}

void WMainWindow::viewsDisplayedChanged() {
    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked() || ui->actionShowGroupDelaySpectrum->isChecked());

    gMW->m_gvWaveform->m_aWaveformShowWindow->setChecked(gMW->m_gvWaveform->m_aWaveformShowWindow->isChecked() && (ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked() || ui->actionShowGroupDelaySpectrum->isChecked()));
    gMW->m_gvWaveform->m_aWaveformShowWindow->setEnabled(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked() || ui->actionShowGroupDelaySpectrum->isChecked());

    // Set the horizontal scroll bars of the spectra
    m_gvAmplitudeSpectrum->updateScrollBars();

    gMW->m_gvAmplitudeSpectrum->selectionSetTextInForm();
}

void WMainWindow::keyPressEvent(QKeyEvent* event){
    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);
    if(event->key()==Qt::Key_Shift && !kctrl){
        enterScrollHandDragMode();
    }
    else if(event->key()==Qt::Key_Control && !kshift){
        if(ui->actionSelectionMode->isChecked()){
            if(m_gvWaveform->m_selection.width()>0){
                m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
                m_gvWaveform->setCursor(Qt::OpenHandCursor);
            }
            if(m_gvAmplitudeSpectrum->m_selection.width()*m_gvAmplitudeSpectrum->m_selection.height()>0){
                m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
                m_gvAmplitudeSpectrum->setCursor(Qt::OpenHandCursor);
            }
            if(m_gvPhaseSpectrum->m_selection.width()*m_gvPhaseSpectrum->m_selection.height()>0){
                m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
                m_gvPhaseSpectrum->setCursor(Qt::OpenHandCursor);
            }
            if(m_gvSpectrumGroupDelay->m_selection.width()*m_gvSpectrumGroupDelay->m_selection.height()>0){
                m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
                m_gvSpectrumGroupDelay->setCursor(Qt::OpenHandCursor);
            }
            if(m_gvSpectrogram->m_selection.width()*m_gvSpectrogram->m_selection.height()>0){
                m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
                m_gvSpectrogram->setCursor(Qt::OpenHandCursor);
            }
        }
        else if(ui->actionEditMode->isChecked()){
            m_gvWaveform->setCursor(Qt::SizeHorCursor);
        }
    }
    else if(event->key()==Qt::Key_Control && kshift){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
            m_gvWaveform->setCursor(Qt::CrossCursor);
            m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvAmplitudeSpectrum->setCursor(Qt::CrossCursor);
            m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvPhaseSpectrum->setCursor(Qt::OpenHandCursor); // For the window's pos control
            m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrumGroupDelay->setCursor(Qt::OpenHandCursor); // For the window's pos control
            m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrogram->setCursor(Qt::CrossCursor);
        }
        else if(ui->actionEditMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
            m_gvWaveform->setCursor(Qt::CrossCursor);
        }
    }
    else{
        if(event->key()==Qt::Key_Escape){
            FTSound* currentftsound = m_fileslist->getCurrentFTSound();
            if(currentftsound && currentftsound->isFiltered()) {
                currentftsound->resetFiltering();
                gFL->fileInfoUpdate();
            }
        }
    }
}

void WMainWindow::keyReleaseEvent(QKeyEvent* event){
    if(event->key()==Qt::Key_Shift){
        leaveScrollHandDragMode();
    }
    if(event->key()==Qt::Key_Control){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
            m_gvWaveform->setCursor(Qt::CrossCursor);
            m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvAmplitudeSpectrum->setCursor(Qt::CrossCursor);
            m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvPhaseSpectrum->setCursor(Qt::CrossCursor);
            m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrumGroupDelay->setCursor(Qt::CrossCursor);
            m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrogram->setCursor(Qt::CrossCursor);
        }
        else {
            m_gvWaveform->setCursor(Qt::SizeVerCursor);
        }
    }
}

void WMainWindow::connectModes(){
    connect(ui->actionSelectionMode, SIGNAL(toggled(bool)), this, SLOT(setSelectionMode(bool)));
    connect(ui->actionEditMode, SIGNAL(toggled(bool)), this, SLOT(setEditMode(bool)));
}
void WMainWindow::disconnectModes(){
    disconnect(ui->actionSelectionMode, SIGNAL(toggled(bool)), this, SLOT(setSelectionMode(bool)));
    disconnect(ui->actionEditMode, SIGNAL(toggled(bool)), this, SLOT(setEditMode(bool)));
}

void WMainWindow::setSelectionMode(bool checked){
    if(checked){
        disconnectModes();
        if(!ui->actionSelectionMode->isChecked()) ui->actionSelectionMode->setChecked(true);
        if(ui->actionEditMode->isChecked()) ui->actionEditMode->setChecked(false);
        connectModes();

        // Clear the icons from the edit mode
        QList<QListWidgetItem*> list = m_fileslist->selectedItems();
        for(int i=0; i<list.size(); i++)
            ((FileType*)list[i])->setEditing(false);

        m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
        m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
        m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);

        QPoint cp = QCursor::pos();

        // Change waveform's cursor
        QPointF p = m_gvWaveform->mapToScene(m_gvWaveform->mapFromGlobal(cp));
        if(p.x()>=m_gvWaveform->m_selection.left() && p.x()<=m_gvWaveform->m_selection.right())
            m_gvWaveform->setCursor(Qt::OpenHandCursor);
        else
            m_gvWaveform->setCursor(Qt::CrossCursor);

        // Change spectrum's cursor
        p = m_gvAmplitudeSpectrum->mapToScene(m_gvAmplitudeSpectrum->mapFromGlobal(cp));
        if(p.x()>=m_gvAmplitudeSpectrum->m_selection.left() && p.x()<=m_gvAmplitudeSpectrum->m_selection.right() && p.y()>=m_gvAmplitudeSpectrum->m_selection.top() && p.y()<=m_gvAmplitudeSpectrum->m_selection.bottom())
            m_gvAmplitudeSpectrum->setCursor(Qt::OpenHandCursor);
        else
            m_gvAmplitudeSpectrum->setCursor(Qt::CrossCursor);
    }
    else
        setSelectionMode(true);
}
void WMainWindow::setEditMode(bool checked){
    if(checked){
        disconnectModes();
        if(ui->actionSelectionMode->isChecked()) ui->actionSelectionMode->setChecked(false);
        if(!ui->actionEditMode->isChecked()) ui->actionEditMode->setChecked(true);
        connectModes();

        m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
        m_gvWaveform->setCursor(Qt::SizeVerCursor);
        m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvAmplitudeSpectrum->setCursor(Qt::SizeVerCursor);
        m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
    }
    else
        setSelectionMode(true);

    gFL->setLabelsEditable(checked);
}

void WMainWindow::setEditing(FileType *ft){
    if(ft)
        ft->setEditing(true);
    else if(m_last_file_editing)
        m_last_file_editing->setEditing(false);

    m_last_file_editing = ft;
}

void WMainWindow::enterScrollHandDragMode(){
    m_gvWaveform->setDragMode(QGraphicsView::ScrollHandDrag);
    m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::ScrollHandDrag);
    m_gvPhaseSpectrum->setDragMode(QGraphicsView::ScrollHandDrag);
    m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::ScrollHandDrag);
    m_gvSpectrogram->setDragMode(QGraphicsView::ScrollHandDrag);
}

void WMainWindow::leaveScrollHandDragMode(){
    m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
    m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
    m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
    m_gvSpectrumGroupDelay->setDragMode(QGraphicsView::NoDrag);
    m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
    if(ui->actionSelectionMode->isChecked()){
        m_gvWaveform->setCursor(Qt::CrossCursor);
        m_gvAmplitudeSpectrum->setCursor(Qt::CrossCursor);
        m_gvSpectrogram->setCursor(Qt::CrossCursor);
    }
    else if(ui->actionEditMode->isChecked()){
        m_gvWaveform->setCursor(Qt::SizeVerCursor);
    }
}

void WMainWindow::focusWindowChanged(QWindow* win){
    Q_UNUSED(win)

    if(QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)
        && !QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
        enterScrollHandDragMode();
    else
        leaveScrollHandDragMode();

    m_fileslist->checkFileModifications();
}


void WMainWindow::allSoundsChanged(){
//    COUTD << "WMainWindow::allSoundsChanged" << endl;
    m_gvWaveform->m_scene->update(); // Can be also very heavy if updating multiple files
    m_gvAmplitudeSpectrum->updateDFTs(); // Can be also very heavy if updating multiple files
    m_gvAmplitudeSpectrum->m_scene->update();
    m_gvPhaseSpectrum->m_scene->update();
    m_gvSpectrumGroupDelay->m_scene->update();
    m_gvSpectrogram->m_dlgSettings->checkImageSize();
    // m_gvSpectrogram->soundsChanged(); // Too heavy to be here, call updateSTFTPlot(force) instead
//    COUTD << "WMainWindow::~allSoundsChanged" << endl;
}

// Put the program into a waiting-for-sound-files state
// (initializeSoundSystem will wake up the necessary functions if a sound file arrived)
void WMainWindow::setInWaitingForFileState(){
    if(m_fileslist->count()>0)
        return;

    ui->splitterViews->hide();
    FTSound::s_fs_common = 0;
    ui->actionSelectedFilesClose->setEnabled(false);
    ui->actionSelectedFilesReload->setEnabled(false);
    ui->actionSelectedFilesToggleShown->setEnabled(false);
    ui->actionPlay->setEnabled(false);
    m_pbVolume->setEnabled(false);
    ui->actionFileNew->setEnabled(false);
}

// Audio management ============================================================

void WMainWindow::initializeSoundSystem(double fs) {

    m_audioengine->initialize(fs);
    if(m_audioengine->isInitialized()) {
        ui->actionPlay->setEnabled(true);
        m_pbVolume->setEnabled(true);
    }
}

void WMainWindow::selectAudioOutputDevice(int di) {
    QList<QAudioDeviceInfo> audioDevices = m_audioengine->availableAudioOutputDevices();
    m_audioengine->setAudioOutputDevice(audioDevices[di]);
}

void WMainWindow::selectAudioOutputDevice(const QString& devicename) {
    if(devicename=="default") {
        m_audioengine->setAudioOutputDevice(QAudioDeviceInfo::defaultOutputDevice());
    }
    else {
        QList<QAudioDeviceInfo> audioDevices = m_audioengine->availableAudioOutputDevices();
        for(int di=0; di<audioDevices.size(); di++) {
            if(audioDevices[di].deviceName()==devicename) {
                m_audioengine->setAudioOutputDevice(audioDevices[di]);
                m_dlgSettings->ui->cbPlaybackAudioOutputDevices->setCurrentIndex(di);
                return;
            }
        }

        m_audioengine->setAudioOutputDevice(QAudioDeviceInfo::defaultOutputDevice());
    }
}

void WMainWindow::audioOutputFormatChanged(const QAudioFormat &format) {
//    cout << "WMainWindow::audioOutputFormatChanged" << endl;
    if(format.sampleRate()==-1) {
        m_dlgSettings->ui->lblAudioOutputDeviceFormat->hide();
    }
    else {
        QAudioDeviceInfo adinfo = m_audioengine->audioOutputDevice();
        cout << "INFO: Audio output format changed: " << adinfo.deviceName().toLocal8Bit().constData() << " fs=" << format.sampleRate() << "Hz" << endl;

        // Display some information
        QString str = "";
        str += "Codec: "+QString::number(format.channelCount())+" channel "+m_audioengine->format().codec()+"<br/>";
        str += "Sampling frequency: "+QString::number(format.sampleRate())+"Hz<br/>";
        str += "Sample type: "+QString::number(format.sampleSize())+"b ";
        QAudioFormat::SampleType sampletype = format.sampleType();
        if(sampletype==QAudioFormat::Unknown)
            str += "(unknown type)";
        else if(sampletype==QAudioFormat::SignedInt)
            str += "signed integer";
        else if(sampletype==QAudioFormat::UnSignedInt)
            str += "unsigned interger";
        else if(sampletype==QAudioFormat::Float)
            str += "float";
        QAudioFormat::Endian byteOrder = format.byteOrder();
        if(byteOrder==QAudioFormat::BigEndian)
            str += " big endian";
        else if(byteOrder==QAudioFormat::LittleEndian)
            str += " little endian";
//        str += "<br/>";

        m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText("<small>"+str+"</small>");
        m_dlgSettings->ui->lblAudioOutputDeviceFormat->show();
        ui->actionPlay->setEnabled(true);
        m_pbVolume->setEnabled(true);
    }
//    cout << "WMainWindow::~audioOutputFormatChanged" << endl;
}

void WMainWindow::audioEngineError(const QString &heading, const QString &detail) {
    Q_UNUSED(heading)
    Q_UNUSED(detail)
    if(!m_audioengine->isInitialized()) {
        m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText("<small>"+heading+": "+detail+"</small>");
        ui->actionPlay->setEnabled(false);
        m_pbVolume->setEnabled(false);
    }
}

void WMainWindow::play()
{
    if(m_audioengine && m_audioengine->isInitialized()){

        if(m_audioengine->state()==QAudio::IdleState || m_audioengine->state()==QAudio::StoppedState){
        // DEBUGSTRING << "MainWindow::play QAudio::IdleState || QAudio::StoppedState" << endl;

            // If stopped, play the whole signal or its selection
            FTSound* currentftsound = m_fileslist->getCurrentFTSound(true);
            if(currentftsound) {

                // Start by reseting any previously filtered sounds
                if(m_lastFilteredSound && m_lastFilteredSound->isFiltered()){
                    m_lastFilteredSound->resetFiltering();
                    m_lastFilteredSound->needDFTUpdate();
                }

                double tstart = m_gvWaveform->m_giPlayCursor->pos().x();
                double tstop = gFL->getMaxLastSampleTime();
                if(m_gvWaveform->m_selection.width()>0){
                    tstart = m_gvWaveform->m_selection.left();
                    tstop = m_gvWaveform->m_selection.right();
                }

                double fstart = 0.0;
                double fstop = gFL->getFs();
                if(QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) &&
                    m_gvAmplitudeSpectrum->m_selection.width()>0){
                    fstart = m_gvAmplitudeSpectrum->m_selection.left();
                    fstop = m_gvAmplitudeSpectrum->m_selection.right();
                }

                try {
                    if(fstart!=0.0 || fstop!=gFL->getFs()){
                        for(size_t fi=0; fi<m_fileslist->ftsnds.size(); fi++)
                            m_fileslist->ftsnds[fi]->setFiltered(false);
                        currentftsound->setFiltered(true);
                        m_lastFilteredSound = currentftsound;
                    }
                    else
                        currentftsound->setFiltered(false);

                    if(!currentftsound->m_actionShow->isChecked())
                        statusBar()->showMessage("WARNING: Playing a hidden waveform!", 3000);
                    else
                        statusBar()->clearMessage();

                    m_gvWaveform->m_initialPlayPosition = tstart;
                    m_playingftsound = currentftsound;
                    m_audioengine->startPlayback(currentftsound, tstart, tstop, fstart, fstop);
                    gFL->fileInfoUpdate();

                    if(fstart!=0.0 || fstop!=gFL->getFs()) {
                        if(m_gvWaveform->m_giSelection->rect().width()>0)
                            m_gvWaveform->m_giFilteredSelection->setRect(m_gvWaveform->m_giSelection->rect());
                        else
                            m_gvWaveform->m_giFilteredSelection->setRect(-0.5/gFL->getFs(), -1.0, currentftsound->getLastSampleTime()+1.0/gFL->getFs(), 2.0);
                        m_gvWaveform->m_giFilteredSelection->show();
                    }
                    else {
                        m_gvWaveform->m_giFilteredSelection->hide();
                    }

                    ui->actionPlay->setEnabled(false);
                    // Delay the stop and re-play,
                    // to avoid the audio engine to go hysterical and crash.
                    QTimer::singleShot(250, this, SLOT(enablePlay()));
                }
                catch(QString err) {
                    statusBar()->showMessage("Error during playback: "+err);
                }
            }
        }
        else if(m_audioengine->state()==QAudio::ActiveState){
            // If playing, just stop it
            m_audioengine->stopPlayback();
//            m_gvWaveform->m_giPlayCursor->hide();
        }
    }
    else
        statusBar()->showMessage("The engine is not ready for playing. Missing available sound device ?");
}

void WMainWindow::enablePlay(){
    ui->actionPlay->setEnabled(true); // Re-enable the play/stop button once the timer m_playreenabler timed out.
}

void WMainWindow::audioStateChanged(QAudio::State state){
//    DEBUGSTRING << "MainWindow::stateChanged " << state << endl;

    if(state==QAudio::ActiveState){
        // Started playing
        ui->actionPlay->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    }
    else if(state==QAudio::StoppedState){
        // Stopped playing
        ui->actionPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        if(m_playingftsound)
            m_playingftsound->stopPlay();
    }

//    DEBUGSTRING << "~MainWindow::stateChanged " << state << endl;
}

void WMainWindow::localEnergyChanged(double e){

//    cout << 20*log10(e) << " " << flush;

    if(e==0) m_pbVolume->setValue(m_pbVolume->minimum());
    else     m_pbVolume->setValue(20*log10(e)); // In dB

    m_pbVolume->repaint();
}
