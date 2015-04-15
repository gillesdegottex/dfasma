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

#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "gvspectrogram.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "ftlabels.h"
#include "../external/audioengine/audioengine.h"

#include "qtextedit.h"

#include <math.h>
#include <iostream>
#include <algorithm>
#include <limits>
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

WMainWindow::WMainWindow(QStringList sndfiles, QWidget *parent)
    : QMainWindow(parent)
    , m_dlgProgress(NULL)
    , m_lastSelectedSound(NULL)
    , m_lastFilteredSound(NULL)
    , ui(new Ui::WMainWindow)
    , m_dlgSettings(NULL)
    , m_gvWaveform(NULL)
    , m_gvSpectrum(NULL)
    , m_gvPhaseSpectrum(NULL)
    , m_gvSpectrogram(NULL)
    , m_audioengine(NULL)
{
    m_loading = true;

    ui->setupUi(this);
    ui->lblFileInfo->hide();
    ui->pbSpectrogramSTFTUpdate->hide();

    gMW = this;

    m_dlgSettings = new WDialogSettings(this);
    m_dlgSettings->ui->lblLibraryAudioFileReading->setText(FTSound::getAudioFileReadingDescription());
    m_dlgSettings->ui->lblAudioOutputDeviceFormat->hide();
    m_dlgSettings->adjustSize();

    ui->mainToolBar->setIconSize(QSize(1.5*m_dlgSettings->ui->sbToolBarSizes->value(),1.5*m_dlgSettings->ui->sbToolBarSizes->value()));

    QString fftinfostr = "";
    fftinfostr += "<br/><i>For the Fast Fourier Transform (FFT):</i> "+sigproc::FFTwrapper::getLibraryInfo();
    fftinfostr += " ("+QString::number(sizeof(FFTTYPE)*8)+"bits; smallest: "+QString::number(20*log10(std::numeric_limits<FFTTYPE>::min()))+"dB)";
    fftinfostr += "</p>";
    m_dlgSettings->ui->vlLibraries->addWidget(new QLabel(fftinfostr));

    QString sdifinfostr = "";
    #ifdef SUPPORT_SDIF
        sdifinfostr = "<br/><i>For SDIF file format:</i> <a href=\"http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/\">Easdif</a> version "+QString(EASDIF_VERSION_STRING);
    #else
        sdifinfostr = "<br/><i>No support for SDIF file format</i>";
    #endif
    m_dlgSettings->ui->vlLibraries->addWidget(new QLabel(sdifinfostr));

    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));
    connect(ui->actionSelectedFilesReload, SIGNAL(triggered()), this, SLOT(selectedFilesReload()));
    connect(ui->actionSelectedFilesToggleShown, SIGNAL(triggered()), this, SLOT(selectedFilesToggleShown()));
    connect(ui->actionSelectedFilesClose, SIGNAL(triggered()), this, SLOT(selectedFilesClose()));

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
    ui->actionFileNew->setVisible(false);
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
    m_pbVolume->setMaximumWidth(m_dlgSettings->ui->sbToolBarSizes->value()/2);
    m_pbVolume->setMaximum(0);
//    m_pbVolume->setMaximumHeight(ui->mainToolBar->height()/2);
    m_pbVolume->setMaximumHeight(ui->mainToolBar->height());
    m_pbVolume->setMinimum(-50); // Quite arbitrary
    m_pbVolume->setValue(-50);   // Quite arbitrary
    ui->mainToolBar->insertWidget(ui->actionSettings, m_pbVolume);
    ui->mainToolBar->insertSeparator(ui->actionSettings);

    m_gvWaveform = new QGVWaveform(this);
    ui->lWaveformGraphicsView->addWidget(m_gvWaveform);

    m_gvSpectrum = new QGVAmplitudeSpectrum(this);
    ui->lSpectrumAmplitudeGraphicsView->addWidget(m_gvSpectrum);

    m_gvPhaseSpectrum = new QGVPhaseSpectrum(this);
    ui->lSpectrumPhaseGraphicsView->addWidget(m_gvPhaseSpectrum);

    m_gvSpectrogram = new QGVSpectrogram(this);
    ui->lSpectrogramGraphicsView->addWidget(m_gvSpectrogram);
    ui->wSpectrogram->hide();

    ui->splitterMain->setStretchFactor(1, 1);
    ui->splitterViews->setStretchFactor(0, 0);
    ui->splitterViews->setStretchFactor(1, 1);
    ui->splitterViews->setStretchFactor(2, 1);
    ui->splitterSpectra->setStretchFactor(0, 2);
    ui->splitterSpectra->setStretchFactor(1, 1);

    // Link axis' views
    connect(m_gvSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvPhaseSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvPhaseSpectrum->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrum->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvWaveform->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvSpectrogram->horizontalScrollBar(), SLOT(setValue(int)));
    connect(m_gvSpectrogram->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_gvWaveform->horizontalScrollBar(), SLOT(setValue(int)));

    // TODO Link spectra with spectrogram ?

    // Set visible views
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumAmplitude, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->sldSpectrumAmplitudeMin, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->wSpectrumPhase, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->lblPhaseSpectrum, SLOT(setVisible(bool)));
    connect(ui->actionShowSpectrogram, SIGNAL(toggled(bool)), ui->wSpectrogram, SLOT(setVisible(bool)));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    QSettings settings;
    ui->actionShowAmplitudeSpectrum->setChecked(settings.value("ShowAmplitudeSpectrum", true).toBool());
    ui->actionShowPhaseSpectrum->setChecked(settings.value("ShowPhaseSpectrum", true).toBool());
    ui->actionShowSpectrogram->setChecked(settings.value("ShowSpectrogram", false).toBool());

    // Start in open file mode
    // and show the panels only if a file has been loaded
    ui->splitterViews->hide();

    m_audioengine = new AudioEngine(this);
    if(m_audioengine) {
        connect(m_audioengine, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioStateChanged(QAudio::State)));
        connect(m_audioengine, SIGNAL(formatChanged(const QAudioFormat&)), this, SLOT(audioOutputFormatChanged(const QAudioFormat&)));
        connect(m_audioengine, SIGNAL(playPositionChanged(double)), m_gvWaveform, SLOT(playCursorSet(double)));
        connect(m_audioengine, SIGNAL(localEnergyChanged(double)), this, SLOT(localEnergyChanged(double)));
        // List the audio devices and select the first one
        m_dlgSettings->ui->cbAudioOutputDevices->clear();
        QList<QAudioDeviceInfo> audioDevices = m_audioengine->availableAudioOutputDevices();
        for(int di=0; di<audioDevices.size(); di++)
            m_dlgSettings->ui->cbAudioOutputDevices->addItem(audioDevices[di].deviceName());
        QSettings settings;
        selectAudioOutputDevice(settings.value("playback/AudioOutputDeviceName", "default").toString());
        connect(m_dlgSettings->ui->cbAudioOutputDevices, SIGNAL(currentIndexChanged(int)), this, SLOT(selectAudioOutputDevice(int)));
    }

    connect(m_dlgSettings->ui->sbToolBarSizes, SIGNAL(valueChanged(int)), this, SLOT(changeToolBarSizes(int)));

    m_dlgProgress = new QProgressDialog("Opening files...", "Abort", 0, sndfiles.size(), this);
    m_dlgProgress->setWindowModality(Qt::WindowModal);
    m_dlgProgress->setMinimumDuration(1000);
    m_dlgProgress->setMaximum(sndfiles.size());
    for(int f=0; f<sndfiles.size() && !m_dlgProgress->wasCanceled(); f++){
        m_dlgProgress->setValue(f);
        addFile(sndfiles[f]);
    }
    m_dlgProgress->setValue(sndfiles.size());
    updateViewsAfterAddFile(true);

    if(sndfiles.size()>0)
        m_gvSpectrogram->updateDFTSettings(); // This will update the window computation AND trigger the STFT computation

    connect(ui->pbSpectrogramSTFTUpdate, SIGNAL(clicked()), m_gvSpectrogram, SLOT(updateDFTSettings()));

    connect(ui->actionFileOpen, SIGNAL(triggered()), this, SLOT(openFile()));

    m_loading = false;
}

void WMainWindow::changeToolBarSizes(int size) {
    gMW->m_gvWaveform->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvSpectrum->m_toolBar->setIconSize(QSize(size,size));
    gMW->m_gvSpectrogram->m_toolBar->setIconSize(QSize(size,size));
    ui->mainToolBar->setIconSize(QSize(1.5*size,1.5*size));
    m_pbVolume->setMaximumWidth(m_dlgSettings->ui->sbToolBarSizes->value()/2);
    m_pbVolume->setMaximumHeight(1.5*size);
}

void WMainWindow::viewsDisplayedChanged() {
    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    gMW->m_gvWaveform->m_aShowWindow->setChecked(gMW->m_gvWaveform->m_aShowWindow->isChecked() && (ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked()));
    gMW->m_gvWaveform->m_aShowWindow->setEnabled(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    if(ui->actionShowPhaseSpectrum->isChecked())
        m_gvSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else
        m_gvSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    gMW->m_gvSpectrum->selectionSetTextInForm();
}

void WMainWindow::newFile(){
    QMessageBox::StandardButton btn = QMessageBox::question(this, "Create a new file ...", "Do you want to create a new label file?", QMessageBox::Yes | QMessageBox::No);
    if(btn==QMessageBox::Yes){
        ui->listSndFiles->addItem(new FTLabels(this));
    }
}

void WMainWindow::settingsSave() {

    QSettings settings;
    settings.setValue("ShowAmplitudeSpectrum", ui->actionShowAmplitudeSpectrum->isChecked());
    settings.setValue("ShowPhaseSpectrum", ui->actionShowPhaseSpectrum->isChecked());
    settings.setValue("ShowSpectrogram", ui->actionShowSpectrogram->isChecked());

    m_gvWaveform->settingsSave();
    m_gvSpectrum->settingsSave();
    m_gvSpectrogram->settingsSave();
}

void WMainWindow::execAbout(){

    QString dfasmaversiongit = STR(DFASMAVERSIONGIT);

    QString	dfasmaversion;
    if(dfasmaversiongit.length()>0) {
        dfasmaversion = QString("Version ") + dfasmaversiongit;
    }
    else {
        QFile readmefile(":/README.txt");
        readmefile.open(QFile::ReadOnly | QFile::Text);
        QTextStream readmefilestream(&readmefile);
        readmefilestream.readLine();
        readmefilestream.readLine();
        dfasmaversion = readmefilestream.readLine();
    }
    QString txt = QString("\
    <h1>DFasma</h1>\
    ")+dfasmaversion;

    txt += " (compiled by "+QString(COMPILER)+" for ";
    #ifdef Q_PROCESSOR_X86_32
      txt += "32bits";
    #endif
    #ifdef Q_PROCESSOR_X86_64
      txt += "64bits";
    #endif
    txt += " on "+QString(__DATE__)+")";

    txt += "<h4>Purpose</h4>";
    txt += "<i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains. "; //  Even though there are a few scaling functionalities, DFasma is basically not an audio editor
    txt += "Its design is inspired by the <i>Xspect</i> software which was developed at <a href='http://www.ircam.fr'>Ircam</a>.</p>";
    // <a href='http://recherche.ircam.fr/equipes/analyse-synthese/DOCUMENTATIONS/xspect/xsintro1.2.html'>Xspect software</a>

    txt += "<p>To suggest a new functionality or report a bug, do not hesitate to <a href='https://github.com/gillesdegottex/dfasma/issues'>raise an issue on GitHub.</a></p>";

    txt += "<h4>Legal</h4>\
            Copyright &copy; 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a><br/>\
            <i>DFasma</i> is coded in C++/<a href='http://qt-project.org'>Qt</a> under the <a href='http://www.gnu.org/licenses/gpl.html'>GPL (v3) License</a>.\
            The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.";

    txt += "<h4>Disclaimer</h4>\
            ALL THE FUNCTIONALITIES OF <I>DFASMA</I> AND ITS CODE ARE PROVIDED WITHOUT ANY WARRANTY \
            (E.G. THERE IS NO WARRANTY OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE). \
            ALSO, THE COPYRIGHT HOLDERS AND CONTRIBUTORS DO NOT TAKE ANY LEGAL RESPONSIBILITY \
            REGARDING THE IMPLEMENTATIONS OF THE PROCESSING TECHNIQUES OR ALGORITHMS \
            (E.G. CONSEQUENCES OF BUGS OR ERRONEOUS IMPLEMENTATIONS). \
            Please see the README.txt file for additional information.";

    txt += "<h4>Credits</h4>\
            Most open-source softwares are infeasible without indirect contributions provided through libraries. \
            Thus, thanks to the following geeks: \
            A. J. Fisher (Butterworth\'s filter design); \
            Laurent de Soras (FFTReal); \
            FFTW3\'s team; \
            Erik de Castro Lopo (libsndfile); \
            SOX\'s team (libsox); \
            Ircam\'s team (SDIF format); \
            Qt\'s team.\
            ";

    txt += "<p>Any contribution of any sort is very welcome and will be rewarded by your name in this about box, in addition to a pint of your favorite beer during the next signal processing <a href='http://www.obsessedwithsports.com/wp-content/uploads/2013/03/revenge-of-the-nerds-sloan-conference.png'>conference</a>!</p>";

    QMessageBox::about(this, "About this software                                                                                                                   ", txt);

//    QMessageBox::aboutQt(this, "About this software");
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

//    cout << "QGVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Shift){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::ScrollHandDrag);
            m_gvSpectrum->setDragMode(QGraphicsView::ScrollHandDrag);
            m_gvSpectrogram->setDragMode(QGraphicsView::ScrollHandDrag);
        }
        else if(ui->actionEditMode->isChecked()){
            m_gvWaveform->setCursor(Qt::SizeHorCursor);
        }
    }
    else if(event->key()==Qt::Key_Control){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setCursor(Qt::OpenHandCursor);
            m_gvSpectrum->setCursor(Qt::OpenHandCursor);
            m_gvSpectrogram->setCursor(Qt::OpenHandCursor);
        }
    }
    else{
        if(event->key()==Qt::Key_Escape){
            FTSound* currentftsound = getCurrentFTSound();
            if(currentftsound && currentftsound->isFiltered())
                currentftsound->resetFiltering();
        }
    }
}

void WMainWindow::keyReleaseEvent(QKeyEvent* event){
    Q_UNUSED(event);

    if(event->key()==Qt::Key_Shift){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setDragMode(QGraphicsView::NoDrag);
            m_gvWaveform->setCursor(Qt::CrossCursor);
            m_gvSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrum->setCursor(Qt::CrossCursor);
            m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
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
            m_gvSpectrum->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrum->setCursor(Qt::CrossCursor);
            m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);
            m_gvSpectrogram->setCursor(Qt::CrossCursor);
        }
        else {

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
        m_gvSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvSpectrogram->setDragMode(QGraphicsView::NoDrag);

        QPoint cp = QCursor::pos();

        // Change waveform's cursor
        QPointF p = m_gvWaveform->mapToScene(m_gvWaveform->mapFromGlobal(cp));
        if(p.x()>=m_gvWaveform->m_selection.left() && p.x()<=m_gvWaveform->m_selection.right())
            m_gvWaveform->setCursor(Qt::OpenHandCursor);
        else
            m_gvWaveform->setCursor(Qt::CrossCursor);

        // Change spectrum's cursor
        p = m_gvSpectrum->mapToScene(m_gvSpectrum->mapFromGlobal(cp));
        if(p.x()>=m_gvSpectrum->m_selection.left() && p.x()<=m_gvSpectrum->m_selection.right() && p.y()>=m_gvSpectrum->m_selection.top() && p.y()<=m_gvSpectrum->m_selection.bottom())
            m_gvSpectrum->setCursor(Qt::OpenHandCursor);
        else
            m_gvSpectrum->setCursor(Qt::CrossCursor);

        ui->actionFileNew->setVisible(false);
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
        m_gvSpectrum->setDragMode(QGraphicsView::NoDrag);
        m_gvWaveform->setCursor(Qt::SizeVerCursor);
        m_gvSpectrum->setCursor(Qt::SizeVerCursor);
        ui->actionFileNew->setVisible(true);
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

WMainWindow::~WMainWindow() {
    delete ui;
}


// File management =======================================================

void WMainWindow::openFile() {

    bool isfirsts = ftsnds.size()==0;

//      const QString &caption = QString(),
//      const QString &dir = QString(),
//      const QString &filter = QString(),
//      QString *selectedFilter = 0,
//      Options options = 0);

    QStringList l = QFileDialog::getOpenFileNames(this, "Open File(s) ...", QString(), QString(), 0, QFileDialog::ReadOnly);

    if(l.size()>0){
        m_dlgProgress->setMaximum(l.size());
        for(int f=0; f<l.size() && !m_dlgProgress->wasCanceled(); f++){
            m_dlgProgress->setValue(f);
            addFile(l.at(f));
        }
        m_dlgProgress->setValue(l.size());
        updateViewsAfterAddFile(isfirsts);
    }
}

void WMainWindow::addFile(const QString& filepath) {
//    cout << "INFO: Add file: " << filepath.toLocal8Bit().constData() << endl;

    try
    {
        bool isfirsts = ftsnds.size()==0;

        FileType* ft = NULL;

        int nchan = FTSound::getNumberOfChannels(filepath);
        if(nchan>0){
            if(nchan==1){
                // If there is only one channel, just load it
                ft = new FTSound(filepath, this);
                ui->listSndFiles->addItem(ft);
            }
            else{
                // If more than one channel, ask what to do
                m_dlgProgress->setValue(m_dlgProgress->maximum()); // Stop the loading bar
                WDialogSelectChannel dlg(filepath, nchan, this);
                if(dlg.exec()) {
                    if(dlg.ui->rdbImportEachChannel->isChecked()){
                        for(int ci=1; ci<=nchan; ci++){
                            ft = new FTSound(filepath, this, ci);
                            ui->listSndFiles->addItem(ft);
                        }
                    }
                    else if(dlg.ui->rdbImportOnlyOneChannel->isChecked()){
                        ft = new FTSound(filepath, this, dlg.ui->sbChannelID->value());
                        ui->listSndFiles->addItem(ft);
                    }
                    else if(dlg.ui->rdbMergeAllChannels->isChecked()){
                        ft = new FTSound(filepath, this, -2); // -2 is a code for merging the channels
                        ui->listSndFiles->addItem(ft);
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
        #ifdef SUPPORT_SDIF
        else if(FileType::isFileSDIF(filepath)) {
            if(FileType::SDIF_hasFrame(filepath, "1FQ0"))
                ft = new FTFZero(filepath, this);
            else if (FileType::SDIF_hasFrame(filepath, "1MRK"))
                ft = new FTLabels(filepath, this, FTLabels::FFSDIF);
            else
                throw QString("Unsupported SDIF data.");

            ui->listSndFiles->addItem(ft);
        }
        #endif
        else if(FileType::isFileASCII(filepath)) {

            ft = new FTLabels(filepath, this);

            // TODO Manage F0 bpf

            ui->listSndFiles->addItem(ft);
        }
        #ifndef SUPPORT_SDIF
        else if(FileType::hasFileExtension(filepath, ".sdif")) {
            throw QString("Support of SDIF files not compiled in this version.");
        }
        #endif
        else {
            throw QString("Cannot find any data or audio channel in this file that is handled by this version of DFasma.");
        }
    }
    catch (QString err)
    {
        m_dlgProgress->setValue(m_dlgProgress->maximum());
        QMessageBox::warning(this, "Failed to load file ...", "Data from the following file can't be loaded:\n"+filepath+"'\n\nReason:\n"+err);
    }
}

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
        m_gvSpectrum->updateSceneRect();
        m_gvPhaseSpectrum->updateSceneRect();
        if(isfirsts){
            m_gvWaveform->viewSet(m_gvWaveform->m_scene->sceneRect(), false);
            m_gvSpectrogram->viewSet(m_gvSpectrogram->m_scene->sceneRect(), false);
            m_gvSpectrum->viewSet(m_gvSpectrum->m_scene->sceneRect(), false);
            m_gvPhaseSpectrum->viewSet(m_gvPhaseSpectrum->m_scene->sceneRect(), false);
        }
        allSoundsChanged();
    }
}

void WMainWindow::updateWindowTitle() {
    int count = ui->listSndFiles->count();
    if(count>0) setWindowTitle("DFasma ("+QString::number(count)+")");
    else        setWindowTitle("DFasma");
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

void WMainWindow::duplicateCurrentFile(){
    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());
    if(currenItem){
        FileType* ft = currenItem->duplicate();
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

void WMainWindow::dropEvent(QDropEvent *event){
//    cout << "Contents: " << event->mimeData()->text().toLatin1().data() << endl;

    bool isfirsts = ftsnds.size()==0;

    QList<QUrl>	lurl = event->mimeData()->urls();

    m_dlgProgress->setMaximum(lurl.size());
    for(int lurli=0; lurli<lurl.size() && !m_dlgProgress->wasCanceled(); lurli++){
        m_dlgProgress->setValue(lurli);
        addFile(lurl[lurli].toLocalFile());
    }
    m_dlgProgress->setValue(lurl.size());
    updateViewsAfterAddFile(isfirsts);
}
void WMainWindow::dragEnterEvent(QDragEnterEvent *event){
    event->acceptProposedAction();
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
        if(m_gvWaveform->m_aShowSelectedWaveformOnTop){
            m_gvWaveform->m_scene->update();
            m_gvSpectrum->m_scene->update();
            m_gvPhaseSpectrum->m_scene->update();
        }
        m_gvSpectrogram->updateSTFTPlot();
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
    m_gvSpectrum->allSoundsChanged(); // Can be also very heavy if updating multiple files
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
    m_gvSpectrum->allSoundsChanged();
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

    for(int i=0; i<l.size(); i++){

        FileType* ft = (FileType*)l.at(i);

        ft->reload();

        if(ft==m_lastSelectedSound)
            reloadSelectedSound = true;
    }

    fileInfoUpdate();

    if(reloadSelectedSound){
        m_gvWaveform->m_scene->update();
        m_gvSpectrum->allSoundsChanged(); // TODO The DFT of all files are updated here, whereas only the slected files need to be ! Need to run computeDFTs on a given file.
        m_gvSpectrogram->updateSTFTPlot(true); // Force the STFT computation
    }

//    COUTD << "WMainWindow::~selectedFileReload" << endl;
}


void WMainWindow::setInWaitingForFileState(){
    if(ui->listSndFiles->count()>0)
        return;

    ui->splitterViews->hide();
    FTSound::fs_common = 0;
    ui->actionSelectedFilesClose->setEnabled(false);
    ui->actionSelectedFilesReload->setEnabled(false);
    ui->actionSelectedFilesToggleShown->setEnabled(false);
    ui->actionPlay->setEnabled(false);
}

// Audio management ============================================================

void WMainWindow::initializeSoundSystem(float fs) {

    m_audioengine->initialize(fs);

    ui->actionPlay->setEnabled(true);
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
                m_dlgSettings->ui->cbAudioOutputDevices->setCurrentIndex(di);
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
    }
//    cout << "WMainWindow::~audioOutputFormatChanged" << endl;
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
                if(m_lastFilteredSound && m_lastFilteredSound->isFiltered())
                    m_lastFilteredSound->resetFiltering();

                double tstart = m_gvWaveform->m_giPlayCursor->pos().x();
                double tstop = getMaxLastSampleTime();
                if(m_gvWaveform->m_selection.width()>0){
                    tstart = m_gvWaveform->m_selection.left();
                    tstop = m_gvWaveform->m_selection.right();
                }

                double fstart = 0.0;
                double fstop = getFs();
                if(QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) &&
                    m_gvSpectrum->m_selection.width()>0){
                    fstart = m_gvSpectrum->m_selection.left();
                    fstop = m_gvSpectrum->m_selection.right();
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
                    m_audioengine->startPlayback(currentftsound, tstart, tstop, fstart, fstop);

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
                    // to avoid the audio engine to got hysterical and crash.
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
    }

//    DEBUGSTRING << "~MainWindow::stateChanged " << state << endl;
}

void WMainWindow::localEnergyChanged(double e){

//    cout << 20*log10(e) << " " << flush;

    if(e==0) m_pbVolume->setValue(m_pbVolume->minimum());
    else     m_pbVolume->setValue(20*log10(e)); // In dB

    m_pbVolume->repaint();
}
