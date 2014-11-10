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

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
#endif

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
#define FFTREAL_VERSION "2.11" // This is the current built-in version
#define FFTW_VERSION "3" // Used interface's version

WMainWindow* WMainWindow::sm_mainwindow = NULL;

WMainWindow::WMainWindow(QStringList sndfiles, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::WMainWindow)
    , m_dlgSettings(NULL)
    , m_gvWaveform(NULL)
    , m_gvSpectrum(NULL)
    , m_gvPhaseSpectrum(NULL)
    , m_audioengine(NULL)
{
    ui->setupUi(this);
    ui->lblFileInfo->hide();

    sm_mainwindow = this;

    m_dlgSettings = new WDialogSettings(this);
    m_dlgSettings->ui->lblLibraryAudioFileReading->setText(FTSound::getAudioFileReadingDescription());
    m_dlgSettings->ui->lblAudioOutputDeviceFormat->hide();
    m_dlgSettings->adjustSize();

    ui->mainToolBar->setIconSize(QSize(1.5*m_dlgSettings->ui->sbToolBarSizes->value(),1.5*m_dlgSettings->ui->sbToolBarSizes->value()));


    QString sdifinfostr = "";
    #ifdef SUPPORT_SDIF
        sdifinfostr = "<br/><i>SDIF file format:</i> <a href=\"http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/\">Easdif</a> version "+QString(EASDIF_VERSION_STRING);
    #else
        sdifinfostr = "<br/><i>No support for SDIF file format</i>";
    #endif
    m_dlgSettings->ui->vlLibraries->addWidget(new QLabel(sdifinfostr));

    QString fftinfostr = "";
    fftinfostr += "<br/><i>Fast Fourier Transform (FFT):</i> ";
    #ifdef FFT_FFTW3
        fftinfostr += "<a href=\"http://www.fftw.org\">FFTW</a> version "+QString(FFTW_VERSION);
    #elif FFT_FFTREAL
        fftinfostr += "<a href=\"http://ldesoras.free.fr/prod.html#src_audio\">FFTReal</a> version "+QString(FFTREAL_VERSION);
    #endif
    fftinfostr += " ("+QString::number(sizeof(FFTTYPE)*8)+"bits; smallest: "+QString::number(20*log10(std::numeric_limits<FFTTYPE>::min()))+"dB)";
    fftinfostr += "</p>";
    m_dlgSettings->ui->vlLibraries->addWidget(new QLabel(fftinfostr));

    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));

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
    ui->actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionCloseFile->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionCloseFile->setEnabled(false);
    connect(ui->actionSettings, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    ui->actionSelectionMode->setChecked(true);
    connectModes();
    connect(ui->listSndFiles, SIGNAL(itemSelectionChanged()), this, SLOT(fileSelectionChanged()));
    addAction(ui->actionMultiShow);
    addAction(ui->actionMultiReload);
    addAction(ui->actionPlayFiltered);

    // Audio engine for playing the selections
    ui->actionPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->actionPlay->setEnabled(false);
    connect(ui->actionPlay, SIGNAL(triggered()), this, SLOT(play()));
    connect(ui->actionPlayFiltered, SIGNAL(triggered()), this, SLOT(play()));

    m_gvWaveform = new QGVWaveform(this);
    ui->lWaveformGraphicsView->addWidget(m_gvWaveform);

    m_gvSpectrum = new QGVAmplitudeSpectrum(this);
    ui->wAmplitudeSpectrum->layout()->addWidget(m_gvSpectrum);

    m_gvPhaseSpectrum = new QGVPhaseSpectrum(this);
    ui->wPhaseSpectrum->layout()->addWidget(m_gvPhaseSpectrum);

    m_gvSpectrogram = new QGVSpectrogram(this);
    ui->wSpectrogram->layout()->addWidget(m_gvSpectrogram);
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

    // Set visible views
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), this, SLOT(viewsDisplayedChanged()));
    connect(ui->actionShowAmplitudeSpectrum, SIGNAL(toggled(bool)), ui->wAmplitudeSpectrum, SLOT(setVisible(bool)));
    connect(ui->actionShowPhaseSpectrum, SIGNAL(toggled(bool)), ui->wPhaseSpectrum, SLOT(setVisible(bool)));
    connect(ui->actionShowSpectrogram, SIGNAL(toggled(bool)), ui->wSpectrogram, SLOT(setVisible(bool)));
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

    for(int f=0; f<sndfiles.size(); f++)
        addFile(sndfiles[f]);
}

void WMainWindow::changeToolBarSizes(int size) {
    WMainWindow::getMW()->m_gvWaveform->m_toolBar->setIconSize(QSize(size,size));
    WMainWindow::getMW()->m_gvSpectrum->m_toolBar->setIconSize(QSize(size,size));
    WMainWindow::getMW()->m_gvSpectrogram->m_toolBar->setIconSize(QSize(size,size));
    ui->mainToolBar->setIconSize(QSize(1.5*size,1.5*size));
}

void WMainWindow::viewsDisplayedChanged() {
    ui->wSpectra->setVisible(ui->actionShowAmplitudeSpectrum->isChecked() || ui->actionShowPhaseSpectrum->isChecked());

    if(ui->actionShowPhaseSpectrum->isChecked())
        m_gvSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else
        m_gvSpectrum->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
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

    QString curdate = QString(__DATE__)+" at "+__TIME__;
    QString txt = QString("\
    <h1>DFasma</h1>\
    ")+dfasmaversion+" (compiled on "+curdate+")";

    txt += "<h4>Purpose</h4>";
    txt += "<i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains. "; //  Even though there are a few scaling functionalities, DFasma is basically <u>not</u> a sound editor
    txt += "Its design is inspired by the <i>Xspect</i> software which was developed at <a href='http://www.ircam.fr'>Ircam</a>.</p>";
    // <a href='http://recherche.ircam.fr/equipes/analyse-synthese/DOCUMENTATIONS/xspect/xsintro1.2.html'>Xspect software</a>

    txt += "<p>To suggest a new functionality or report a bug, do not hesitate to <a href='https://github.com/gillesdegottex/dfasma/issues'>raise an issue on GitHub.</a></p>";

    txt += "<h4>Legal</h4>\
            Copyright &copy; 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a><br/>\
            <i>DFasma</i> is coded in C++/<a href='http://qt-project.org'>Qt</a> under the <a href='http://www.gnu.org/licenses/gpl.html'>GPL (v3) License</a>.\
            The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.";

    txt += "<h4>Disclaimer</h4>\
            All the functionalities of <i>DFasma</i> and its code are provided WITHOUT ANY WARRANTY \
            (e.g. there is NO WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE). \
            Also, the author does NOT TAKE ANY LEGAL RESPONSIBILITY regarding <i>DFasma</i> or its code \
            (e.g. consequences of bugs or erroneous implementation).";

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

    QMessageBox::about(this, "About this software                                                                                                ", txt);

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
        }
        else if(ui->actionEditMode->isChecked()){
            m_gvWaveform->setCursor(Qt::SizeHorCursor);
        }
    }
    else if(event->key()==Qt::Key_Control){
        if(ui->actionSelectionMode->isChecked()){
            m_gvWaveform->setCursor(Qt::OpenHandCursor);
            m_gvSpectrum->setCursor(Qt::OpenHandCursor);
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
    }
    else
        setSelectionMode(true);
}

WMainWindow::~WMainWindow() {
    delete ui;
}


// File management =======================================================

void WMainWindow::openFile() {

//      const QString &caption = QString(),
//      const QString &dir = QString(),
//      const QString &filter = QString(),
//      QString *selectedFilter = 0,
//      Options options = 0);
    QStringList l = QFileDialog::getOpenFileNames(this, "Open File(s) ...", QString(), QString(), 0, QFileDialog::ReadOnly);

    for(int f=0; f<l.size(); f++)
        addFile(l.at(f));
}

void WMainWindow::addFile(const QString& filepath) {
    cout << "INFO: Add file: " << filepath.toLocal8Bit().constData() << endl;

    try
    {
        FileType* ft = NULL;

        QFileInfo fileinfo(filepath);
        if(fileinfo.suffix().compare("sdif", Qt::CaseInsensitive)==0) {
            #ifdef SUPPORT_SDIF
            // The following check doesnt work for some files which can be loaded
            //if(!SdifCheckFileFormat(filepath.toLocal8Bit()))
            //    throw QString("This file looks like an SDIF file, but it does not contain SDIF data.");

            if(FileType::SDIF_hasFrame(filepath, "1FQ0"))
                ft = new FTFZero(filepath, this);
            else if (FileType::SDIF_hasFrame(filepath, "1MRK"))
                ft = new FTLabels(filepath, this);
            else
                throw QString("Unsupported SDIF data.");
            #else
            throw QString("Support of SDIF files not compiled in this version.");
            #endif

            ui->listSndFiles->addItem(ft);
        }
        else { // Assume it is an audio file

            int nchan = FTSound::getNumberOfChannels(filepath);

            if(nchan==0)
                throw QString("There is not even a single channel.");
            else if(nchan==1){
                ft = new FTSound(filepath, this);
                ui->listSndFiles->addItem(ft);
            }
            else{
                WDialogSelectChannel* dlg = new WDialogSelectChannel(filepath, nchan, this);
                dlg->exec();

                if(dlg->ui->rdbImportEachChannel->isChecked()){
                    for(int ci=1; ci<=nchan; ci++){
                        ft = new FTSound(filepath, this, ci);
                        ui->listSndFiles->addItem(ft);
                    }
                }
                else if(dlg->ui->rdbImportOnlyOneChannel->isChecked()){
                    ft = new FTSound(filepath, this, dlg->ui->sbChannelID->value());
                    ui->listSndFiles->addItem(ft);
                }
                else if(dlg->ui->rdbMergeAllChannels->isChecked()){
                    ft = new FTSound(filepath, this, -2); // -2 is a code for merging the channels
                    ui->listSndFiles->addItem(ft);
                }

                delete dlg;
            }

            // The first sound determines the common fs for the audio output
            if(ftsnds.size()==1)
                initializeSoundSystem(ftsnds[0]->fs);

            m_gvWaveform->fitViewToSoundsAmplitude();
        //    cout << "~MainWindow::addFile" << endl;
        }

        m_gvWaveform->updateSceneRect();
        soundsChanged();
        ui->actionCloseFile->setEnabled(true);
        ui->splitterViews->show();
        updateWindowTitle();
//        WMainWindow::getMW()->ui->splitterViews->handle(2)->hide();
    }
    catch (QString err)
    {
        QMessageBox::warning(this, "Failed to load file ...", "Data from the following file can't be loaded:\n"+filepath+"'\n\nReason:\n"+err);

        return;
    }
}

void WMainWindow::updateWindowTitle() {
    int count = ui->listSndFiles->count();
    if(count>0) setWindowTitle("DFasma ("+QString::number(count)+")");
    else        setWindowTitle("DFasma");
}

void WMainWindow::checkFileModifications(){
//    cout << "GET FOCUS " << QDateTime::currentMSecsSinceEpoch() << endl;
    for(size_t fi=0; fi<ftsnds.size(); fi++)
        ftsnds[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftfzeros.size(); fi++)
        ftfzeros[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftlabels.size(); fi++)
        ftlabels[fi]->checkFileStatus();
}

void WMainWindow::duplicateCurrentFile(){
    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());
    if(currenItem){
        FileType* ft = currenItem->duplicate();
        if(ft){
            ui->listSndFiles->addItem(ft);
            m_gvWaveform->updateSceneRect();
            soundsChanged();
            ui->actionCloseFile->setEnabled(true);
            ui->splitterViews->show();
            updateWindowTitle();
        }
    }
}

void WMainWindow::dropEvent(QDropEvent *event){
//    cout << "Contents: " << event->mimeData()->text().toLatin1().data() << endl;

    QList<QUrl>	lurl = event->mimeData()->urls();
    for(int lurli=0; lurli<lurl.size(); lurli++)
        addFile(lurl[lurli].toLocalFile());
}
void WMainWindow::dragEnterEvent(QDragEnterEvent *event){
    event->acceptProposedAction();
}

FTSound* WMainWindow::getCurrentFTSound(bool defselectfirst) {

    if(ftsnds.empty())
        return NULL;

    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());

    if(currenItem && currenItem->type==FileType::FTSOUND)
        return (FTSound*)currenItem;

    if(defselectfirst)
        return ftsnds[0];

    return NULL;
}

FTLabels* WMainWindow::getCurrentFTLabels(bool defselectfirst) {

    if(ftlabels.empty())
        return NULL;

    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());

    if(currenItem && currenItem->type==FileType::FTLABELS)
        return (FTLabels*)currenItem;

    if(defselectfirst)
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
        contextmenu.addAction(ui->actionMultiShow);
        contextmenu.addAction(ui->actionMultiReload);
        contextmenu.addAction(ui->actionCloseFile);
    }

    int contextmenuheight = contextmenu.sizeHint().height();
    QPoint posglobal = mapToGlobal(pos)+QPoint(24,contextmenuheight/2);
    contextmenu.exec(posglobal);
}

void WMainWindow::fileSelectionChanged() {
//    cout << "WMainWindow::fileSelectionChanged" << endl;
    ui->actionMultiShow->disconnect();
    ui->actionMultiReload->disconnect();

    QList<QListWidgetItem*> list = ui->listSndFiles->selectedItems();
    for(int i=0; i<list.size(); i++) {
        connect(ui->actionMultiShow, SIGNAL(triggered()), ((FileType*)list.at(i))->m_actionShow, SLOT(toggle()));
        connect(ui->actionMultiReload, SIGNAL(triggered()), ((FileType*)list.at(i))->m_actionReload, SLOT(trigger()));
        connect(ui->actionMultiReload, SIGNAL(triggered()), this, SLOT(fileInfoUpdate()));
    }

    connect(ui->actionMultiShow, SIGNAL(triggered()), this, SLOT(toggleSoundShown()));
    connect(ui->actionMultiReload, SIGNAL(triggered()), this, SLOT(soundsChanged()));

    fileInfoUpdate();
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
void WMainWindow::toggleSoundShown() {
    QList<QListWidgetItem*> list = ui->listSndFiles->selectedItems();
    for(int i=0; i<list.size(); i++)
        ((FileType*)list.at(i))->setShown(((FileType*)list.at(i))->m_actionShow->isChecked());

    if(list.size()>0)
        soundsChanged();
}
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

        soundsChanged();
    }
}
void WMainWindow::resetDelay(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_delay = 0.0;

        currentftsound->setStatus();

        soundsChanged();
    }
}

void WMainWindow::soundsChanged(){
    m_gvWaveform->soundsChanged();
    m_gvSpectrum->soundsChanged();
}

void WMainWindow::closeSelectedFile() {

    m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();

    for(int i=0; i<l.size(); i++){

        FileType* ft = (FileType*)l.at(i);

        cout << "Closing file: \"" << ft->fileFullPath.toLocal8Bit().constData() << "\"" << endl;

        delete ft; // Remove it from the listview

        // Remove it from its own type-related list
        if(ft->type==FileType::FTSOUND)
            ftsnds.erase(std::find(ftsnds.begin(), ftsnds.end(), (FTSound*)ft));
        else if(ft->type==FileType::FTFZERO)
            ftfzeros.erase(std::find(ftfzeros.begin(), ftfzeros.end(), (FTFZero*)ft));
        else if(ft->type==FileType::FTLABELS)
            ftlabels.erase(std::find(ftlabels.begin(), ftlabels.end(), (FTLabels*)ft));

        updateWindowTitle();
    }

    // If there is no more files, put the interface in a waiting-for-file state.
    if(ui->listSndFiles->count()==0){
        ui->splitterViews->hide();
        FTSound::fs_common = 0;
        ui->actionCloseFile->setEnabled(false);
        ui->actionPlay->setEnabled(false);
    }
    else
        soundsChanged();
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
}

void WMainWindow::play()
{
    if(m_audioengine && m_audioengine->isInitialized()){

        if(m_audioengine->state()==QAudio::IdleState || m_audioengine->state()==QAudio::StoppedState){
        // DEBUGSTRING << "MainWindow::play QAudio::IdleState || QAudio::StoppedState" << endl;

            // If stopped, play the whole signal or its selection
            FTSound* currentftsound = getCurrentFTSound();
            if(currentftsound) {

                // Start by reseting any filtered sounds
                for(size_t fi=0; fi<ftsnds.size(); fi++)
                    if(ftsnds[fi]!=currentftsound)
                        ftsnds[fi]->wavtoplay = &(ftsnds[fi]->wav);

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

    if(e==0) ui->pbVolume->setValue(ui->pbVolume->minimum());
    else     ui->pbVolume->setValue(20*log10(e)); // In dB

    ui->pbVolume->repaint();
}
