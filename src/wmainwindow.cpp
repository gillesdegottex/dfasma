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

#include "gvwaveform.h"
#include "gvspectrum.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "ftlabels.h"
#include "../external/audioengine/audioengine.h"

#include "qtextedit.h"

#include <math.h>
#include <iostream>
#include <algorithm>
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

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
#endif

WMainWindow::WMainWindow(QStringList sndfiles, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::WMainWindow)
    , m_dlgSettings(NULL)
    , m_gvWaveform(NULL)
    , m_gvSpectrum(NULL)
    , m_audioengine(NULL)
{
    ui->setupUi(this);

    m_dlgSettings = new WDialogSettings(this);
    m_dlgSettings->ui->lblLibraryAudioFileReading->setText(FTSound::getAudioFileReadingDescription());

    #ifdef FFT_FFTW3
        m_dlgSettings->ui->vlLibraries->addWidget(new QLabel("<i>Library for the Fast Fourier Transform (FFT):</i> <a href=\"http://www.fftw.org\">FFTW</a> version 3"));
    #elif FFT_FFTREAL
        m_dlgSettings->ui->vlLibraries->addWidget(new QLabel("<i>Library for the Fast Fourier Transform (FFT):</i> <a href=\"http://ldesoras.free.fr/prod.html#src_audio\">FFTReal</a> version 2.11"));
    #endif

    #ifdef SUPPORT_SDIF
        m_dlgSettings->ui->vlLibraries->addWidget(new QLabel("<i>Support SDIF format:</i> <a href=\"http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/\">Easdif</a> version "+QString(EASDIF_VERSION_STRING)));
    #else
        m_dlgSettings->ui->vlLibraries->addWidget(new QLabel("<i>No SDIF format support</i>"));
    #endif

    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));

    setAcceptDrops(true);
    ui->listSndFiles->setAcceptDrops(true);
    ui->listSndFiles->setSelectionRectVisible(false);
//    ui->listSndFiles->setSelectionMode(QAbstractItemView::MultiSelection); // TODO fix BUG1 below first
    ui->listSndFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listSndFiles, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showFileContextMenu(const QPoint&)));

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

    // Audio engine for playing the selections
    ui->actionPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->actionPlay->setEnabled(false);
    connect(ui->actionPlay, SIGNAL(triggered()), this, SLOT(play()));

    m_gvWaveform = new QGVWaveform(this);
    ui->lWaveformGraphicsView->addWidget(m_gvWaveform);
    ui->splitterViews->hide();

    m_gvSpectrum = new QGVSpectrum(this);
    ui->lSpectrumGraphicsView->addWidget(m_gvSpectrum);
    ui->gvPSpectrum->hide(); // TODO Phase spectrum

    for(int f=0; f<sndfiles.size(); f++)
        addFile(sndfiles[f]);
}

void WMainWindow::execAbout(){
    QMessageBox::about(this, "About this software", "\
    <h1>DFasma</h1>\
    version master\
    <p>Copyright (&copy;) 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a></p>\
    <br/><p><i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains.</p>\
    <p>It is coded in C++/<a href='http://qt-project.org'>Qt</a> under the <a href='http://www.gnu.org/licenses/gpl.html'>GPL (v3) License</a>.\
    <br/>The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.</p>\
    <p>Its purpose and design are inspired by the <i>Xspect</i> software developed at <a href='http://www.ircam.fr'>Ircam</a>.</p>\
    <br/><p>Any contribution of any sort is very welcome and will be rewarded by your name in this about box, in addition to a pint of your favorite <a href='http://wildsidevancouver.com/wp-content/uploads/2013/07/beer.jpg'>beer</a> during the next signal processing <a href='http://www.obsessedwithsports.com/wp-content/uploads/2013/03/revenge-of-the-nerds-sloan-conference.png'>conference</a>!</p>");
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

WMainWindow::~WMainWindow()
{
    delete ui;
}

void WMainWindow::keyPressEvent(QKeyEvent* event){

//    cout << "QGVWaveform::keyPressEvent " << endl;

    if(event->key()==Qt::Key_CapsLock){
        if(ui->actionEditMode->isChecked())
            setSelectionMode(true);
        else
            setEditMode(true);
    }
    else if(event->key()==Qt::Key_Shift){
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
    cout << "MainWindow::addFile " << filepath.toLocal8Bit().constData() << endl;

    try
    {
        FileType* ft = NULL;

        QFileInfo fileinfo(filepath);
        if(fileinfo.suffix()=="sdif") { // TODO case insensitive
            #ifdef SUPPORT_SDIF

            if(0) { // TODO check kind of data in the file, f0

                FTFZero* ftf0 = new FTFZero(filepath, this);
                ftfzeros.push_back(ftf0);
                ft = ftf0;
            }
            else if (1) { // TODO labels
                FTLabels* ftlabel = new FTLabels(filepath, this);
                ftlabels.push_back(ftlabel);
                ft = ftlabel;
            }
            #else
            throw QString("SDIF files support not compiled in this version.");
            #endif
        }
        else {
            FTSound* ftsnd = new FTSound(filepath, this);
            ftsnds.push_back(ftsnd);
            ft = ftsnd;

            ui->splitterViews->show();
            soundsChanged();
            ui->actionCloseFile->setEnabled(true);

            // The first sound will determine the common fs for the audio output
            if(ftsnds.size()==1)
                initializeSoundSystem(ftsnds[0]->fs);

            m_gvWaveform->fitViewToSoundsAmplitude();
        //    cout << "~MainWindow::addFile" << endl;
        }

        // Set properties common to all files
        QFileInfo fileInfo(filepath);
        ft->setText(fileInfo.fileName());
        ft->setToolTip(fileInfo.absoluteFilePath());

        QPixmap pm(32,32);
        pm.fill(ft->color);
        ft->setIcon(QIcon(pm));

        ui->listSndFiles->addItem(ft);
    }
    catch (QString err)
    {
        QMessageBox::warning(this, "Failed to load file ...", "The audio waveform in the following file can't be loaded:\n"+filepath+"'\nReason:\n"+err);

        return;
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

FTSound* WMainWindow::getCurrentFTSound() {
    FileType* currenItem = (FileType*)(ui->listSndFiles->currentItem());

    if(currenItem->type==FileType::FTSOUND)
        return (FTSound*)currenItem;

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
        contextmenu.addAction(ui->actionCloseFile);
    }

    int contextmenuheight = contextmenu.sizeHint().height();
    QPoint posglobal = mapToGlobal(pos)+QPoint(24,contextmenuheight/2);
    contextmenu.exec(posglobal);
}
void WMainWindow::fileSelectionChanged(){
    ui->actionMultiShow->disconnect();
    connect(ui->actionMultiShow, SIGNAL(triggered()), this, SLOT(soundsChanged()));

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();
    for(int i=0; i<l.size(); i++)
        connect(ui->actionMultiShow, SIGNAL(triggered()), ((FileType*)l.at(i))->m_actionShow, SLOT(toggle()));
}
void WMainWindow::setSoundShown(bool show){
    QListWidgetItem* li = ui->listSndFiles->currentItem();
    if(show)    li->setForeground(QBrush(QColor(0,0,0)));
    else        li->setForeground(QBrush(QColor(168,168,168)));

    soundsChanged();
}
void WMainWindow::resetAmpScale(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_ampscale = 1.0;

        soundsChanged();
    }
}
void WMainWindow::resetDelay(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_delay = 0;

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

        cout << "Closing file: \"" << ft->fileName.toLocal8Bit().constData() << "\"" << endl;

        delete ft; // Remove it from the listview

        // Remove it from its own type-related list
        if(ft->type==FileType::FTSOUND)
            ftsnds.erase(std::find(ftsnds.begin(), ftsnds.end(), (FTSound*)ft));
        else if(ft->type==FileType::FTFZERO)
            ftfzeros.erase(std::find(ftfzeros.begin(), ftfzeros.end(), (FTFZero*)ft));
        else if(ft->type==FileType::FTLABELS)
            ftlabels.erase(std::find(ftlabels.begin(), ftlabels.end(), (FTLabels*)ft));
    }

    // If there is no more files, put the interface in a waiting-for-file state.
    if(ui->listSndFiles->count()==0){
        ui->splitterViews->hide();
        FTSound::fs_common = 0;
        ui->actionCloseFile->setEnabled(false);
    }
    else
        soundsChanged();
}

// Audio management ============================================================

void WMainWindow::initializeSoundSystem(float fs){

//    cout << "MainWindow::initializeSoundSystem fs=" << fs << endl;

    if(m_audioengine){
        delete m_audioengine;
        m_audioengine = NULL;
    }

    m_audioengine = new AudioEngine(fs, this);
    connect(m_audioengine, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioStateChanged(QAudio::State)));
    connect(m_audioengine, SIGNAL(audioOutputDeviceChanged(const QAudioDeviceInfo&)), this, SLOT(audioOutputDeviceChanged(const QAudioDeviceInfo&)));
    connect(m_audioengine, SIGNAL(playPositionChanged(double)), m_gvWaveform, SLOT(setPlayCursor(double)));
    connect(m_audioengine, SIGNAL(localEnergyChanged(double)), this, SLOT(localEnergyChanged(double)));


    // Provide some information
    QString txt = m_audioengine->audioOutputDevice().deviceName();
    m_dlgSettings->ui->lblSoundSystem->setText(txt);
    m_dlgSettings->ui->lblSoundSystem->setToolTip(txt);

    txt = QString("%1Hz").arg(fs);
    ui->lblFs->setText(txt);
    ui->lblFs->setToolTip(txt);

    ui->actionPlay->setEnabled(true);
//    cout << "~MainWindow::initializeSoundSystem" << endl;
}

void WMainWindow::audioOutputDeviceChanged(const QAudioDeviceInfo& device){
    // Provide some information
    QString txt = device.deviceName();
    m_dlgSettings->ui->lblSoundSystem->setText(txt);
    m_dlgSettings->ui->lblSoundSystem->setToolTip(txt);
}

void WMainWindow::play()
{
//    DEBUGSTRING << "MainWindow::play state=" << m_audioengine->state() << "(" << QAudio::StoppedState << ")" << endl;

    if(m_audioengine && m_audioengine->isInitialized()){

        if(m_audioengine->state()==QAudio::IdleState || m_audioengine->state()==QAudio::StoppedState){
//            DEBUGSTRING << "MainWindow::play QAudio::IdleState || QAudio::StoppedState" << endl;

            // If stopped, play the whole signal or its selection
            FTSound* currentftsound = getCurrentFTSound();
            if(currentftsound) {
                ui->actionPlay->setEnabled(false);
    //            DEBUGSTRING << "MainWindow::play currentRow: " << fi << " " << snds.size() << endl;

                if(m_gvWaveform->m_selection.width()>0){
    //                DEBUGSTRING << "MainWindow::play 1" << endl;
                    int nleft = int(0.5+m_gvWaveform->m_selection.left()*getFs());
                    int nright = int(0.5+m_gvWaveform->m_selection.right()*getFs());

    //                DEBUGSTRING << "MainWindow::play 2" << endl;
                    QString txt = QString("Play selection: [%1 (%2s), %3, (%4s)]").arg(nleft).arg((nleft-1)/getFs()).arg(nright).arg((nright-1)/getFs());
                    statusBar()->showMessage(txt);

    //                DEBUGSTRING << "MainWindow::play 3" << endl;
                    m_audioengine->startPlayback(currentftsound, m_gvWaveform->m_selection.left(), m_gvWaveform->m_selection.right());
                }
                else{
                    QString filetoplay = currentftsound->fileName;

                    QString txt = QString("Play full file: ")+filetoplay;
                    statusBar()->showMessage(txt);
    //                DEBUGSTRING << "MainWindow::play " << txt.toLocal8Bit().constData() << "'" << endl;

                    m_audioengine->startPlayback(currentftsound);
                }
                QTimer::singleShot(250, this, SLOT(enablePlay())); // Delay the stop and re-play, in order to avoid the audio engine to crash hysterically.
            }
        }
        else if(m_audioengine->state()==QAudio::ActiveState){
//            DEBUGSTRING << "MainWindow::play QAudio::ActiveState" << endl;
            // If playing, just stop it
            m_audioengine->stopPlayback();
            m_gvWaveform->m_giPlayCursor->hide();
        }
    }
    else
        statusBar()->showMessage("The engine is not ready for playing. Missing available sound device ?");

//    DEBUGSTRING << "~MainWindow::play " << m_audioengine->state() << endl;
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

    if(e==0)
        ui->pbVolume->setValue(ui->pbVolume->minimum());
    else
        ui->pbVolume->setValue(20*log10(e)); // In dB

    ui->pbVolume->repaint();
}
