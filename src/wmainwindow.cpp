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

#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
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
#include "qthelper.h"

#ifdef SUPPORT_SDIF
    #include <easdif/easdif.h>
#endif

WMainWindow* gMW = NULL;

WMainWindow::WMainWindow(QStringList files, QWidget *parent)
    : QMainWindow(parent)
    , m_lastSelectedSound(NULL)
    , m_lastFilteredSound(NULL)
    , m_prgdlg(NULL)
    , m_dlgSettings(NULL)
    , ui(new Ui::WMainWindow)
    , m_gvWaveform(NULL)
    , m_gvAmplitudeSpectrum(NULL)
    , m_gvPhaseSpectrum(NULL)
    , m_gvSpectrogram(NULL)
    , m_audioengine(NULL)
    , m_playingftsound(NULL)
{ 
    gMW = this;

    m_loading = true;

    // Prepare the UI (everything which is independent of settings)
    ui->setupUi(this);
    ui->lblFileInfo->hide();
    ui->pbSpectrogramSTFTUpdate->hide();
    m_qxtSpectrogramSpanSlider = new QxtSpanSlider(Qt::Vertical, this);
    m_qxtSpectrogramSpanSlider->setMinimum(0);
    m_qxtSpectrogramSpanSlider->setMaximum(100);
    m_qxtSpectrogramSpanSlider->setUpperValue(90);
    m_qxtSpectrogramSpanSlider->setLowerValue(10);
    m_qxtSpectrogramSpanSlider->setMaximumWidth(ui->sldWaveformAmplitude->maximumWidth());
    m_qxtSpectrogramSpanSlider->setHandleMovementMode(QxtSpanSlider::NoOverlapping);
    ui->horizontalLayout_6->insertWidget(1, m_qxtSpectrogramSpanSlider);

    m_dlgSettings = new WDialogSettings(this);
    m_dlgSettings->ui->lblAudioOutputDeviceFormat->hide();
    m_dlgSettings->adjustSize();

    ui->mainToolBar->setIconSize(QSize(1.5*m_dlgSettings->ui->sbViewsToolBarSizes->value(),1.5*m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    connect(m_dlgSettings->ui->sbFileListItemSize, SIGNAL(valueChanged(int)), this, SLOT(changeFileListItemsSize()));
    changeFileListItemsSize();

    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));
    connect(ui->actionSelectedFilesReload, SIGNAL(triggered()), this, SLOT(selectedFilesReload()));
    connect(ui->actionSelectedFilesToggleShown, SIGNAL(triggered()), this, SLOT(selectedFilesToggleShown()));
    connect(ui->actionSelectedFilesClose, SIGNAL(triggered()), this, SLOT(selectedFilesClose()));
    connect(ui->actionSelectedFilesDuplicate, SIGNAL(triggered()), this, SLOT(selectedFilesDuplicate()));

    m_globalWaitingBarLabel = new QLabel(ui->statusBar);
    m_globalWaitingBarLabel->setAlignment(Qt::AlignRight);
    ui->statusBar->setStyleSheet("QStatusBar::item { border: 0px solid black }; ");
    m_globalWaitingBarLabel->setMaximumSize(500, 18);
    m_globalWaitingBarLabel->setText("<waiting bar info text>");
    ui->statusBar->addPermanentWidget(m_globalWaitingBarLabel);
    m_globalWaitingBar = new QProgressBar(ui->statusBar);
    m_globalWaitingBar->setAlignment(Qt::AlignRight);
    m_globalWaitingBar->setMaximumSize(100, 14);
    m_globalWaitingBar->setValue(50);
    ui->statusBar->addPermanentWidget(m_globalWaitingBar);
    m_globalWaitingBarLabel->hide();
    m_globalWaitingBar->hide();

    setAcceptDrops(true);
    ui->listSndFiles->setAcceptDrops(true);
    ui->listSndFiles->setSelectionRectVisible(false);
    ui->listSndFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listSndFiles, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showFileContextMenu(const QPoint&)));

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
    connect(ui->actionSettings, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    ui->actionSelectionMode->setChecked(true);
    connectModes();
    connect(ui->listSndFiles, SIGNAL(itemSelectionChanged()), this, SLOT(fileSelectionChanged()));
    addAction(ui->actionSelectedFilesToggleShown);
    addAction(ui->actionSelectedFilesReload);
    addAction(ui->actionSelectedFilesDuplicate);
    addAction(ui->actionPlayFiltered);

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
    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    // Spectrogram
    m_gvSpectrogram = new QGVSpectrogram(this);
    ui->lSpectrogramGraphicsView->addWidget(m_gvSpectrogram);
    m_settings.add(ui->actionShowSpectrogram);
    ui->wSpectrogram->setVisible(ui->actionShowSpectrogram->isChecked());
    connect(ui->pbSpectrogramSTFTUpdate, SIGNAL(clicked()), m_gvSpectrogram, SLOT(updateSTFTSettings()));

    // Link axis' views
    connect(m_gvAmplitudeSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvPhaseSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvPhaseSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvAmplitudeSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));

    // Set visible views
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumAmplitude, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->sldAmplitudeSpectrumMin, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumPhase, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->lblPhaseSpectrum, SLOT(setVisible(bool)));
    connect(ui->actionShowSpectrogram, SIGNAL(toggled(bool)), ui->wSpectrogram, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
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
        sizesMain.append(100);
        sizesMain.append(600);
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
            m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText("No audio device available.");
            m_dlgSettings->ui->lblAudioOutputDeviceFormat->show();
//            m_dlgSettings->ui->cbPlaybackAudioOutputDevices->hide();
        }
        selectAudioOutputDevice(m_settings.value("cbPlaybackAudioOutputDevices", "default").toString());
        connect(m_dlgSettings->ui->cbPlaybackAudioOutputDevices, SIGNAL(currentIndexChanged(int)), this, SLOT(selectAudioOutputDevice(int)));
        m_dlgSettings->adjustSize();
    }

    // This one seems able to open distant files because file paths arrive in gvfs format
    // in the main.
    addFiles(files);
    updateViewsAfterAddFile(true);

    if(files.size()>0)
        m_gvSpectrogram->updateSTFTSettings(); // This will update the window computation AND trigger the STFT computation

    connect(ui->actionFileOpen, SIGNAL(triggered()), this, SLOT(openFile())); // Alow this only at the end

    m_loading = false;
}

WMainWindow::~WMainWindow() {
//    COUTD << "WMainWindow::~WMainWindow" << endl;

    m_gvSpectrogram->m_stftcomputethread->cancelComputation(true);
    m_gvAmplitudeSpectrum->m_fftresizethread->cancelComputation(true);

    ui->listSndFiles->selectAll();
    selectedFilesClose();

    // The audio player
    if(m_audioengine){
        delete m_audioengine;
        m_audioengine=NULL;
    }

    // Delete views
    delete m_gvWaveform; m_gvWaveform=NULL;
    delete m_gvAmplitudeSpectrum; m_gvAmplitudeSpectrum=NULL;
    delete m_gvPhaseSpectrum; m_gvPhaseSpectrum=NULL;
    delete m_gvSpectrogram; m_gvSpectrogram=NULL;
    delete m_dlgSettings; m_dlgSettings=NULL;

    delete ui; ui=NULL; // The GUI
}

// Interface ===================================================================

void WMainWindow::changeFileListItemsSize() {
//    COUTD << m_dlgSettings->ui->sbFileListItemSize->value() << endl;

    QFont font = ui->listSndFiles->font();
    font.setPixelSize(m_dlgSettings->ui->sbFileListItemSize->value());
    QSize size = ui->listSndFiles->iconSize();
    size.setHeight(m_dlgSettings->ui->sbFileListItemSize->value()+2);
    size.setWidth(m_dlgSettings->ui->sbFileListItemSize->value()+2);
    ui->listSndFiles->setIconSize(size);
    ui->listSndFiles->setFont(font);
}

void WMainWindow::changeToolBarSizes(int size) {
    gMW->m_gvWaveform->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvAmplitudeSpectrum->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvSpectrogram->m_toolBar->setIconSize(QSize(size,size));
    ui->mainToolBar->setIconSize(QSize(1.5*size,1.5*size));
    m_pbVolume->setMaximumWidth(m_dlgSettings->ui->sbViewsToolBarSizes->value()/2);
    m_pbVolume->setMaximumHeight(1.5*size);
}

void WMainWindow::updateWindowTitle() {
    int count = ui->listSndFiles->count();
    if(count>0) setWindowTitle("DFasma ("+QString::number(count)+")");
    else        setWindowTitle("DFasma");
}

void WMainWindow::execAbout(){
    AboutBox box(this);
    box.exec();
}

// File management =======================================================

void WMainWindow::stopFileProgressDialog() {
//    COUTD << "WMainWindow::stopFileProgressDialog " << m_prgdlg << endl;
    if(m_prgdlg) m_prgdlg->setValue(m_prgdlg->maximum());// Stop the loading bar
}

void WMainWindow::newFile(){
    QMessageBox::StandardButton btn = QMessageBox::question(this, "Create a new file ...", "Do you want to create a new label file?", QMessageBox::Yes | QMessageBox::No);
    if(btn==QMessageBox::Yes){
        ui->listSndFiles->addItem(new FTLabels(this));
    }
}

void WMainWindow::openFile() {
//    COUTD << "WMainWindow::openFile" << endl;

    QString filters;
    filters += FileType::m_typestrings[FileType::FTUNSET];
    filters += ";;"+FileType::m_typestrings[FileType::FTSOUND];
    filters += ";;"+FileType::m_typestrings[FileType::FTFZERO];
    filters += ";;"+FileType::m_typestrings[FileType::FTLABELS];

    QString selectedFilter;
    QStringList files = QFileDialog::getOpenFileNames(this, "Open File(s)...", QString(), filters, &selectedFilter, QFileDialog::ReadOnly);

    // Use selectedFilter for pre-selecting the file type.
    FileType::FType type = FileType::FTUNSET;
    if(selectedFilter==FileType::m_typestrings[FileType::FTSOUND])
        type = FileType::FTSOUND;
    else if(selectedFilter==FileType::m_typestrings[FileType::FTFZERO])
        type = FileType::FTFZERO;
    else if(selectedFilter==FileType::m_typestrings[FileType::FTLABELS])
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
        bool isfirsts = ftsnds.size()==0;
        addFiles(files, type);
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

    bool isfirsts = ftsnds.size()==0;
    addFiles(files);
    updateViewsAfterAddFile(isfirsts);
}
void WMainWindow::dragEnterEvent(QDragEnterEvent *event){
    event->acceptProposedAction();
}

void WMainWindow::addFilesRecursive(const QStringList& files, FileType::FType type) {
    for(int fi=0; fi<files.size() && !m_prgdlg->wasCanceled(); fi++) {
        if(QFileInfo(files[fi]).isDir()) {
//            COUTD << "Add recursive" << endl;
            QDir fpd(files[fi]);

            // Recursive call on directories
            fpd.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
            if(fpd.count()>0){
                for(int fpdi=0; fpdi<int(fpd.count()) && !m_prgdlg->wasCanceled(); ++fpdi){
//                    COUTD << "Dir: " << fpd[fpdi].toLatin1().constData() << endl;
                    addFilesRecursive(QStringList(fpd.filePath(fpd[fpdi])));
                }
            }

            // Add the files of the current directory
            fpd.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            if(fpd.count()>0){
                m_prgdlg->setMaximum(fpd.count());
//                COUTD << "Setmax " << fpd.count() << endl;
                for(int fpdi=0; fpdi<int(fpd.count()) && !m_prgdlg->wasCanceled(); ++fpdi){
//                    COUTD << "File: " << fpd[fpdi].toLatin1().constData() << endl;
                    m_prgdlg->setValue(fpdi);
                    QCoreApplication::processEvents(); // To show the progress
                    addFile(fpd.filePath(fpd[fpdi]));
                }
            }
        }
        else {
            if(m_prgdlg) m_prgdlg->setValue(fi);
            QCoreApplication::processEvents(); // To show the progress
            addFile(files[fi], type);
        }
    }
}

void WMainWindow::addFiles(const QStringList& files, FileType::FType type) {
//    COUTD << "WMainWindow::addFiles " << files.size() << endl;

    // These progress dialogs HAVE to be built on the stack otherwise ghost dialogs appear.
    QProgressDialog prgdlg("Opening files...", "Abort", 0, files.size(), this);
    prgdlg.setMinimumDuration(500);
    m_prgdlg = &prgdlg;

    addFilesRecursive(files, type);

    stopFileProgressDialog();
    m_prgdlg = NULL;
}

void WMainWindow::addFile(const QString& filepath, FileType::FType type) {
//    cout << "INFO: Add file: " << filepath.toLocal8Bit().constData() << endl;

    if(QFileInfo(filepath).isDir()){
        throw QString("Shoudn't use WMainWindow::addFile for directories, use WMainWindow::addFiles instead (with an 's')");
    }
    else{
        try{
            bool isfirsts = ftsnds.size()==0;

            // Attention: There is the type of data stored in DFasma (FILETYPE) (ex. FileType::FTSOUND)
            //  and the format of the file (ex. FFSOUND)

            // This should be always "guessable"
            FileType::FileContainer container = FileType::guessContainer(filepath);

            // can be replaced by an autodetect or "forced mode" in the future

            // Then, guess the type of the data in the file, if no specified yet
            if(type==FileType::FTUNSET){
            // The format and the DFasma's type have to correspond
                if(container==FileType::FCANYSOUND)
                    type = FileType::FTSOUND; // These two have to match

                #ifdef SUPPORT_SDIF
                if(container==FileType::FCSDIF) {
                    if(FileType::SDIF_hasFrame(filepath, "1FQ0"))
                        type = FileType::FTFZERO;
                    else if (FileType::SDIF_hasFrame(filepath, "1MRK"))
                        type = FileType::FTLABELS;
                    else
                        throw QString("Unsupported SDIF data.");
                }
                #endif
                if(container==FileType::FCTEXT) {
                    // Distinguish between f0, labels and future VUF files (and futur others ...)
                    // Do a grammar check (But this won't help to diff F0 and VUF files)
                    ifstream fin(filepath.toLatin1().constData());
                    if(!fin.is_open())
                        throw QString("Cannot open this file");
                    double t;
                    string line, text;
                    // Check the first line only (Assuming it is enough)
                    // TODO May have to skip commented lines depending on the text format
                    if(!std::getline(fin, line))
                        throw QString("There is not a single line in this file");
                    // Check: <number> <number>
                    std::istringstream iss(line);
                    // Check if first two values are real numbers
                    // TODO This will NOT work to distinguish F0 or VUF !!
                    if((iss >> t >> t) && iss.eof())
                        type = FileType::FTFZERO;
                    else // Otherwise, assume this is a label
                        type = FileType::FTLABELS;
                }
            }

            if(type==FileType::FTUNSET)
                throw QString("Cannot find any data or audio channel in this file that is handled by this distribution of DFasma.");

            // Finally, load the data knowing the type and the container
            if(type==FileType::FTSOUND){
                int nchan = FTSound::getNumberOfChannels(filepath);
                if(nchan==1){
                    // If there is only one channel, just load it
                    ui->listSndFiles->addItem(new FTSound(filepath, this));
                }
                else{
                    // If more than one channel, ask what to do
                    stopFileProgressDialog();
                    WDialogSelectChannel dlg(filepath, nchan, this);
                    if(dlg.exec()) {
                        if(dlg.ui->rdbImportEachChannel->isChecked()){
                            for(int ci=1; ci<=nchan; ci++){
                                ui->listSndFiles->addItem(new FTSound(filepath, this, ci));
                            }
                        }
                        else if(dlg.ui->rdbImportOnlyOneChannel->isChecked()){
                            ui->listSndFiles->addItem(new FTSound(filepath, this, dlg.ui->sbChannelID->value()));
                        }
                        else if(dlg.ui->rdbMergeAllChannels->isChecked()){
                            ui->listSndFiles->addItem(new FTSound(filepath, this, -2));// -2 is a code for merging the channels
                        }
                    }
                }

                if(ftsnds.size()>0){
                    // The first sound determines the common sampling frequency for the audio output
                    if(isfirsts)
                        initializeSoundSystem(ftsnds[0]->fs);

                    m_gvWaveform->fitViewToSoundsAmplitude();
                }
            }
            else if(type == FileType::FTFZERO)
                ui->listSndFiles->addItem(new FTFZero(filepath, this, container));
            else if(type == FileType::FTLABELS)
                ui->listSndFiles->addItem(new FTLabels(filepath, this, container));
        }
        catch (QString err)
        {
            stopFileProgressDialog();
            QMessageBox::warning(this, "Failed to load file ...", "Data from the following file can't be loaded:\n"+filepath+"'\n\nReason:\n"+err);
        }
    }
}


// Views =======================================================================

void WMainWindow::updateViewsAfterAddFile(bool isfirsts) {
    if(ui->listSndFiles->count()==0)
        setInWaitingForFileState();
    else {
        ui->actionSelectedFilesClose->setEnabled(true);
        ui->actionSelectedFilesReload->setEnabled(true);
        ui->actionSelectedFilesToggleShown->setEnabled(true);
        ui->splitterViews->show();
        updateWindowTitle();
        m_gvWaveform->updateSceneRect();
        m_gvSpectrogram->updateSceneRect();
        m_gvAmplitudeSpectrum->updateAmplitudeExtent();
        m_gvPhaseSpectrum->updateSceneRect();
        if(isfirsts){
            m_gvWaveform->viewSet(m_gvWaveform->m_scene->sceneRect(), false);
            m_gvSpectrogram->viewSet(m_gvSpectrogram->m_scene->sceneRect(), false);
            m_gvAmplitudeSpectrum->viewSet(m_gvAmplitudeSpectrum->m_scene->sceneRect(), false);
            m_gvPhaseSpectrum->viewSet(m_gvPhaseSpectrum->m_scene->sceneRect(), false);
            ui->actionFileNew->setEnabled(true);
        }
        allSoundsChanged();
    }
}

void WMainWindow::viewsDisplayedChanged() {
    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    gMW->m_gvWaveform->m_aWaveformShowWindow->setChecked(gMW->m_gvWaveform->m_aWaveformShowWindow->isChecked() && (ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked()));
    gMW->m_gvWaveform->m_aWaveformShowWindow->setEnabled(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    if(ui->actionShowPhaseSpectrum->isChecked())
        m_gvAmplitudeSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else if(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked())
        m_gvAmplitudeSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    gMW->m_gvAmplitudeSpectrum->selectionSetTextInForm();
}

double WMainWindow::getFs(){
    if(ftsnds.size()>0)
        return ftsnds[0]->fs;
    else
        return 44100.0;   // Fake a ghost sound using 44.1kHz sampling rate
}
unsigned int WMainWindow::getMaxWavSize(){

    if(ftsnds.empty())
        return 44100;      // Fake a one second ghost sound of 44.1kHz sampling rate

    unsigned int s = 0;

    for(unsigned int fi=0; fi<ftsnds.size(); fi++)
        s = max(s, (unsigned int)(ftsnds[fi]->wav.size()));

    return s;
}
double WMainWindow::getMaxDuration(){

    double dur = getMaxWavSize()/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        dur = std::max(dur, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        dur = std::max(dur, ftlabels[fi]->getLastSampleTime());

    return dur;
}
double WMainWindow::getMaxLastSampleTime(){

    double lst = getMaxDuration()-1.0/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        lst = std::max(lst, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        lst = std::max(lst, ftlabels[fi]->getLastSampleTime());

    return lst;
}

void WMainWindow::keyPressEvent(QKeyEvent* event){
    bool kshift = event->modifiers().testFlag(Qt::ShiftModifier);
    bool kctrl = event->modifiers().testFlag(Qt::ControlModifier);
    if(event->key()==Qt::Key_Shift && !kctrl){
        m_gvWaveform->setDragMode(QGraphicsView::ScrollHandDrag);
        m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::ScrollHandDrag);
        m_gvPhaseSpectrum->setDragMode(QGraphicsView::ScrollHandDrag);
        m_gvSpectrogram->setDragMode(QGraphicsView::ScrollHandDrag);
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
            FTSound* currentftsound = getCurrentFTSound();
            if(currentftsound && currentftsound->isFiltered()) {
                currentftsound->resetFiltering();
                gMW->fileInfoUpdate();
            }
        }
    }
}

void WMainWindow::keyReleaseEvent(QKeyEvent* event){
    if(event->key()==Qt::Key_Shift){
        m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
        m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
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
    if(event->key()==Qt::Key_Control){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
            m_gvWaveform->setCursor(Qt::CrossCursor);
            m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvAmplitudeSpectrum->setCursor(Qt::CrossCursor);
            m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvPhaseSpectrum->setCursor(Qt::CrossCursor);
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

        m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
        m_gvAmplitudeSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvPhaseSpectrum->setDragMode(QGraphicsView::NoDrag);
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
    }
    else
        setSelectionMode(true);

    setLabelsEditable(checked);
}

void WMainWindow::setLabelsEditable(bool editable){
    for(size_t fi=0; fi<ftlabels.size(); fi++){
        for(size_t li=0; li<ftlabels[fi]->waveform_labels.size(); li++){
            if(editable)
                ftlabels[fi]->waveform_labels[li]->setTextInteractionFlags(Qt::TextEditorInteraction);
            else
                ftlabels[fi]->waveform_labels[li]->setTextInteractionFlags(Qt::NoTextInteraction);
        }
    }
}

// Check if a file has been modified on the disc
// TODO Check if this is a distant file and avoid checking if it is ?
void WMainWindow::checkFileModifications(){
//    cout << "GET FOCUS " << QDateTime::currentMSecsSinceEpoch() << endl;
    for(size_t fi=0; fi<ftsnds.size(); fi++)
        ftsnds[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftfzeros.size(); fi++)
        ftfzeros[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftlabels.size(); fi++)
        ftlabels[fi]->checkFileStatus();

    gMW->fileInfoUpdate();
}


FTSound* WMainWindow::getCurrentFTSound(bool forceselect) {

    if(ftsnds.empty())
        return NULL;

    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());

    if(currenItem && currenItem->type==FileType::FTSOUND)
        return (FTSound*)currenItem;

    if(forceselect){
        if(m_lastSelectedSound)
            return m_lastSelectedSound;
        else
            return ftsnds[0];
    }

    return NULL;
}

FTLabels* WMainWindow::getCurrentFTLabels(bool forceselect) {

    if(ftlabels.empty())
        return NULL;

    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());

    if(currenItem && currenItem->type==FileType::FTLABELS)
        return (FTLabels*)currenItem;

    if(forceselect)
        return ftlabels[0];

    return NULL;
}


void WMainWindow::showFileContextMenu(const QPoint& pos) {

    QMenu contextmenu(this);

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();
    if(l.size()==1) {
        FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());
        currenItem->fillContextMenu(contextmenu, this);
    }
    else {
        contextmenu.addAction(ui->actionSelectedFilesToggleShown);
        contextmenu.addAction(ui->actionSelectedFilesReload);
        contextmenu.addAction(ui->actionSelectedFilesClose);
    }

    int contextmenuheight = contextmenu.sizeHint().height();
    QPoint posglobal = mapToGlobal(pos)+QPoint(24,contextmenuheight/2);
    contextmenu.exec(posglobal);
}

void WMainWindow::fileSelectionChanged() {
//    COUTD << "WMainWindow::fileSelectionChanged" << endl;

    QList<QListWidgetItem*> list = ui->listSndFiles->selectedItems();
    int nb_snd_in_selection = 0;

    for(int i=0; i<list.size(); i++) {
        if(((FileType*)list.at(i))->type == FileType::FTSOUND){
            nb_snd_in_selection++;
            m_lastSelectedSound = (FTSound*)list.at(i);
        }
    }

    // Update the spectrogram to current selected signal
    if(nb_snd_in_selection>0){
        if(m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop){
            m_gvWaveform->m_scene->update();
            m_gvAmplitudeSpectrum->m_scene->update();
            m_gvPhaseSpectrum->m_scene->update();
        }
        m_gvSpectrogram->updateSTFTPlot();
        m_gvSpectrogram->m_scene->update();
    }

    fileInfoUpdate();

//    COUTD << "WMainWindow::~fileSelectionChanged" << endl;
}
void WMainWindow::fileInfoUpdate() {
    QList<QListWidgetItem*> list = ui->listSndFiles->selectedItems();

    // If only one file selected
    // Display Basic information of it
    if(list.size()==1) {
        ui->lblFileInfo->setText(((FileType*)list.at(0))->info());
        ui->lblFileInfo->show();
    }
    else
        ui->lblFileInfo->hide();
}

// FileType is not an qobject, thus, need to forward the message manually (i.e. without signal system).
void WMainWindow::colorSelected(const QColor& color) {
    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());
    if(currenItem)
        currenItem->setColor(color);
}
void WMainWindow::resetAmpScale(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_ampscale = 1.0;

        currentftsound->setStatus();

        allSoundsChanged();
    }
}
void WMainWindow::resetDelay(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_delay = 0.0;

        currentftsound->setStatus();

        allSoundsChanged();
    }
}

void WMainWindow::allSoundsChanged(){
//    COUTD << "WMainWindow::allSoundsChanged" << endl;
    m_gvWaveform->m_scene->update(); // Can be also very heavy if updating multiple files
    m_gvAmplitudeSpectrum->updateDFTs(); // Can be also very heavy if updating multiple files
    m_gvAmplitudeSpectrum->m_scene->update();
    m_gvPhaseSpectrum->m_scene->update();
    m_gvSpectrogram->m_dlgSettings->checkImageSize();
    // m_gvSpectrogram->soundsChanged(); // Too heavy to be here, call updateSTFTPlot(force) instead
//    COUTD << "WMainWindow::~allSoundsChanged" << endl;
}

void WMainWindow::selectedFilesToggleShown() {
    QList<QListWidgetItem*> list = ui->listSndFiles->selectedItems();
    for(int i=0; i<list.size(); i++){
        ((FileType*)list.at(i))->setVisible(!((FileType*)list.at(i))->m_actionShow->isChecked());
        if(((FileType*)list.at(i))==m_lastSelectedSound)
            m_gvSpectrogram->updateSTFTPlot();
    }
    m_gvWaveform->m_scene->update();
    m_gvSpectrogram->m_scene->update();
    m_gvAmplitudeSpectrum->updateDFTs();
    m_gvAmplitudeSpectrum->m_scene->update();
    m_gvPhaseSpectrum->m_scene->update();
}

void WMainWindow::selectedFilesClose() {
    m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();
    ui->listSndFiles->clearSelection();

    bool removeSelectedSound = false;

    for(int i=0; i<l.size(); i++){

        FileType* ft = (FileType*)l.at(i);

//        cout << "INFO: Closing file: \"" << ft->fileFullPath.toLocal8Bit().constData() << "\"" << endl;

        delete ft; // Remove it from the listview

        // Remove it from its own type-related list
        if(ft->type==FileType::FTSOUND){
            if(ft==m_lastSelectedSound){
                removeSelectedSound = true;
                m_lastSelectedSound = NULL;
            }
            ftsnds.erase(std::find(ftsnds.begin(), ftsnds.end(), (FTSound*)ft));
        }
        else if(ft->type==FileType::FTFZERO)
            ftfzeros.erase(std::find(ftfzeros.begin(), ftfzeros.end(), (FTFZero*)ft));
        else if(ft->type==FileType::FTLABELS)
            ftlabels.erase(std::find(ftlabels.begin(), ftlabels.end(), (FTLabels*)ft));
    }

    updateWindowTitle();

    if(m_gvWaveform->m_scene->sceneRect().right()>gMW->getMaxDuration()+1.0/gMW->getFs()) {
        m_gvWaveform->updateSceneRect();
        m_gvSpectrogram->updateSceneRect();
        m_gvWaveform->setMouseCursorPosition(-1, true);
        m_gvWaveform->viewSet(m_gvWaveform->m_scene->sceneRect(), true);
    }

    if(removeSelectedSound)
        m_gvSpectrogram->clearSTFTPlot();

    // If there is no more files, put the interface in a waiting-for-file state.
    if(ui->listSndFiles->count()==0)
        setInWaitingForFileState();
    else
        allSoundsChanged();
}

void WMainWindow::selectedFilesReload() {
//    COUTD << "WMainWindow::selectedFileReload" << endl;

    m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();

    bool reloadSelectedSound = false;
    bool didanysucceed = false;

    for(int i=0; i<l.size(); i++) {

        FileType* ft = (FileType*)l.at(i);

        if(ft->reload())
            didanysucceed = true;

        if(ft==m_lastSelectedSound)
            reloadSelectedSound = true;
    }

    fileInfoUpdate();

    if(didanysucceed && reloadSelectedSound) {
        m_gvWaveform->m_scene->update();
        m_gvAmplitudeSpectrum->updateDFTs();
        m_gvSpectrogram->updateSTFTPlot(true); // Force the STFT computation
    }

//    COUTD << "WMainWindow::~selectedFileReload" << endl;
}

void WMainWindow::selectedFilesDuplicate() {
    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        FileType* ft = currentfile->duplicate();
        if(ft){
            ui->listSndFiles->addItem(ft);
            m_gvWaveform->updateSceneRect();
            allSoundsChanged();
            ui->actionSelectedFilesClose->setEnabled(true);
            ui->actionSelectedFilesReload->setEnabled(true);
            ui->splitterViews->show();
            updateWindowTitle();
        }
    }
}

// Put the program into a waiting-for-sound-files state
// (initializeSoundSystem will wake up the necessary functions if a sound file arrived)
void WMainWindow::setInWaitingForFileState(){
    if(ui->listSndFiles->count()>0)
        return;

    ui->splitterViews->hide();
    FTSound::fs_common = 0;
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
        str += "<br/>";

        m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText(str);
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
        m_dlgSettings->ui->lblAudioOutputDeviceFormat->setText(heading+": "+detail);
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
            FTSound* currentftsound = getCurrentFTSound(true);
            if(currentftsound) {

                // Start by reseting any previously filtered sounds
                if(m_lastFilteredSound && m_lastFilteredSound->isFiltered()){
                    m_lastFilteredSound->resetFiltering();
                    m_lastFilteredSound->needDFTUpdate();
                }

                double tstart = m_gvWaveform->m_giPlayCursor->pos().x();
                double tstop = getMaxLastSampleTime();
                if(m_gvWaveform->m_selection.width()>0){
                    tstart = m_gvWaveform->m_selection.left();
                    tstop = m_gvWaveform->m_selection.right();
                }

                double fstart = 0.0;
                double fstop = getFs();
                if(QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) &&
                    m_gvAmplitudeSpectrum->m_selection.width()>0){
                    fstart = m_gvAmplitudeSpectrum->m_selection.left();
                    fstop = m_gvAmplitudeSpectrum->m_selection.right();
                }

                try {
                    if(fstart!=0.0 || fstop!=getFs()){
                        for(size_t fi=0; fi<ftsnds.size(); fi++)
                            ftsnds[fi]->setFiltered(false);
                        currentftsound->setFiltered(true);
                        m_lastFilteredSound = currentftsound;
                    }
                    else
                        currentftsound->setFiltered(false);

                    if(!currentftsound->m_actionShow->isChecked())
                        statusBar()->showMessage("WARNING: Playing a hidden waveform!");

                    m_gvWaveform->m_initialPlayPosition = tstart;
                    m_playingftsound = currentftsound;
                    m_audioengine->startPlayback(currentftsound, tstart, tstop, fstart, fstop);
                    fileInfoUpdate();

                    if(fstart!=0.0 || fstop!=getFs()) {
                        if(m_gvWaveform->m_giSelection->rect().width()>0)
                            m_gvWaveform->m_giFilteredSelection->setRect(m_gvWaveform->m_giSelection->rect());
                        else
                            m_gvWaveform->m_giFilteredSelection->setRect(-0.5/getFs(), -1.0, currentftsound->getLastSampleTime()+1.0/getFs(), 2.0);
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
