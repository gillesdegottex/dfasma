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
#include "iodsound.h"

#include "qtextedit.h"

#include <math.h>
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
#include <QtMultimedia/QSound>
#include <QFileDialog>
#include <QMessageBox>

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
    m_dlgSettings->ui->lblAudioFileReading->setText(IODSound::getAudioFileReadingDescription());
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(execAbout()));

    ui->listSndFiles->setSelectionRectVisible(false);
//    ui->listSndFiles->setSelectionMode(QAbstractItemView::MultiSelection); // TODO fix BUG1 below first
    ui->listSndFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listSndFiles, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showSoundContextMenu(const QPoint&)));

    ui->actionAbout->setIcon(QIcon(":/icons/about.svg"));
    ui->actionSettings->setIcon(QIcon(":/icons/settings.svg"));
    ui->actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionCloseSelectedSound->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionCloseSelectedSound->setEnabled(false);
    connect(ui->actionSettings, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    ui->actionSelectionMode->setChecked(true);
    connectModes();

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
    <p><i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains.</p>\
    <p>Its purpose and design are inspired by the <i>Xspect</i> software developed internally at <a href='http://www.ircam.fr'>Ircam</a>.</p>\
    <p>It is coded in C++/<a href='http://qt-project.org'>Qt</a> under the <a href='http://www.gnu.org/licenses/gpl.html'>GPL (v3) License</a>.\
    <br/>Copyright (&copy;) 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a>\
    <br/>The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.</p>\
    <br/><p>Any contribution of any sort is very welcome and will be rewarded by your name in this about box, in addition to a pint of your favorite <a href='http://wildsidevancouver.com/wp-content/uploads/2013/07/beer.jpg'>beer</a> during the next signal processing <a href='http://www.obsessedwithsports.com/wp-content/uploads/2013/03/revenge-of-the-nerds-sloan-conference.png'>conference</a>!</p>");
//    QMessageBox::aboutQt(this, "About this software");
}

float WMainWindow::getFs(){
    if(hasFilesLoaded())
        return snds[0]->fs;
    else
        return 44100.0;   // Fake a ghost sound using 44.1kHz sampling rate
}
unsigned int WMainWindow::getMaxWavSize(){

    if(!hasFilesLoaded())
        return 44100;      // Fake a one second ghost sound of 44.1kHz sampling rate

    unsigned int s = 0;

    for(unsigned int fi=0; fi<snds.size(); fi++)
        s = max(s, snds[fi]->wav.size());

    return s;
}
float WMainWindow::getMaxDuration(){

    return getMaxWavSize()/getFs();
}
float WMainWindow::getMaxLastSampleTime(){

    return getMaxDuration()-1.0/getFs();
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

// Sound file management =======================================================

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
//    cout << "MainWindow::addFile" << endl;

    IODSound* snd = NULL;
    try
    {
        std::cout << filepath.toLocal8Bit().constData() << endl;
        snd = new IODSound(filepath, this);
    }
    catch (QString err)
    {
        QMessageBox::warning(this, "Failed to load file ...", "The audio waveform in the following file can't be loaded:\n"+filepath+"'\nReason:\n"+err);

        return;
    }

    snds.push_back(snd);

    QFileInfo fileInfo(filepath);

    QListWidgetItem* li = new QListWidgetItem(fileInfo.fileName());
    li->setToolTip(fileInfo.absoluteFilePath());

    QPixmap pm(32,32);
    pm.fill(snd->color);
    li->setIcon(QIcon(pm));

    ui->listSndFiles->addItem(li);

    ui->splitterViews->show();
    soundsChanged();
    ui->actionCloseSelectedSound->setEnabled(true);

    // The first sound will determine the common fs for the audio output
    if(snds.size()==1)
        initializeSoundSystem(snds[0]->fs);

    m_gvWaveform->fitViewToSoundsAmplitude();
//    cout << "~MainWindow::addFile" << endl;
}

void WMainWindow::showSoundContextMenu(const QPoint& pos) {
    int row = ui->listSndFiles->currentRow();

    QMenu contextmenu(this);
    contextmenu.addAction(ui->actionPlay);
    contextmenu.addAction(ui->actionCloseSelectedSound);
    contextmenu.addAction(snds[row]->m_actionShow);
    connect(snds[row]->m_actionShow, SIGNAL(toggled(bool)), this, SLOT(setSoundShown(bool)));
    contextmenu.addAction(snds[row]->m_actionInvPolarity);
    connect(snds[row]->m_actionInvPolarity, SIGNAL(toggled(bool)), this, SLOT(soundsChanged()));
    snds[row]->m_actionResetAmpScale->setText(QString("Reset amplitude scaling (%1dB) to 0dB").arg(20*log10(snds[row]->m_ampscale), 0, 'g', 3));
    snds[row]->m_actionResetAmpScale->setDisabled(snds[row]->m_ampscale==1.0);
    contextmenu.addAction(snds[row]->m_actionResetAmpScale);
    connect(snds[row]->m_actionResetAmpScale, SIGNAL(triggered()), this, SLOT(resetAmpScale()));
    snds[row]->m_actionResetDelay->setText(QString("Reset delay (%1s) to 0s").arg(snds[row]->m_delay, 0, 'g', 3));
    snds[row]->m_actionResetDelay->setDisabled(snds[row]->m_delay==0.0);
    contextmenu.addAction(snds[row]->m_actionResetDelay);
    connect(snds[row]->m_actionResetDelay, SIGNAL(triggered()), this, SLOT(resetDelay()));

//    int contextmenuheight = contextmenu.sizeHint().height();
//    int contextmenuheight = contextmenu.actioncontextmenu.height();
    int contextmenuheight = 0;
    QPoint posglobal = mapToGlobal(pos+QPoint(0,contextmenuheight/2));
    contextmenu.exec(posglobal);
}
void WMainWindow::setSoundShown(bool show){
    QListWidgetItem* li = ui->listSndFiles->currentItem();
    if(show)    li->setForeground(QBrush(QColor(0,0,0)));
    else        li->setForeground(QBrush(QColor(168,168,168)));

    soundsChanged();
}
void WMainWindow::resetAmpScale(){
    int row = ui->listSndFiles->currentRow();
    snds[row]->m_ampscale = 1.0;

    soundsChanged();
}
void WMainWindow::resetDelay(){
    int row = ui->listSndFiles->currentRow();
    snds[row]->m_delay = 0.0;

    soundsChanged();
}

void WMainWindow::soundsChanged(){
    m_gvWaveform->soundsChanged();
    m_gvSpectrum->soundsChanged();
}

void WMainWindow::closeSelectedFile() {

    m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = ui->listSndFiles->selectedItems();

    for(int i=0; i<l.size(); i++){
        int r = ui->listSndFiles->row(l.at(i));
        cout << "Closing file: " << snds[r]->fileName.toLocal8Bit().constData() << endl;
        delete l.at(i);
        IODSound* snd = snds[r];
        snds.erase(snds.begin()+r); // TODO BUG1 Works only if the selection is unique
        delete snd;
    }

    // If there is no more files, put the interface in a waiting-for-file state.
    if(ui->listSndFiles->count()==0){
        ui->splitterViews->hide();
        IODSound::fs_common = 0;
        ui->actionCloseSelectedSound->setEnabled(false);
    }
    else
        soundsChanged();
}

// Audio play management =======================================================

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

            ui->actionPlay->setEnabled(false);

            int fi = ui->listSndFiles->currentRow();
            if(fi==-1) fi=0;
            ui->listSndFiles->setCurrentRow(fi); // Force the selection of the current file
//            DEBUGSTRING << "MainWindow::play currentRow: " << fi << " " << snds.size() << endl;

            if(m_gvWaveform->m_selection.width()>0){
//                DEBUGSTRING << "MainWindow::play 1" << endl;
                int nleft = int(0.5+m_gvWaveform->m_selection.left()*getFs());
                int nright = int(0.5+m_gvWaveform->m_selection.right()*getFs());

//                DEBUGSTRING << "MainWindow::play 2" << endl;
                QString txt = QString("Play selection: [%1 (%2s), %3, (%4s)]").arg(nleft).arg((nleft-1)/getFs()).arg(nright).arg((nright-1)/getFs());
                statusBar()->showMessage(txt);

//                DEBUGSTRING << "MainWindow::play 3" << endl;
                m_audioengine->startPlayback(snds[fi], m_gvWaveform->m_selection.left(), m_gvWaveform->m_selection.right());
            }
            else{
                QString filetoplay = ui->listSndFiles->currentItem()->text();

                QString txt = QString("Play full file: ")+filetoplay;
                statusBar()->showMessage(txt);
//                DEBUGSTRING << "MainWindow::play " << txt.toLocal8Bit().constData() << "'" << endl;

                m_audioengine->startPlayback(snds[fi]);
            }
            QTimer::singleShot(250, this, SLOT(enablePlay())); // Delay the stop and re-play, in order to avoid the audio engine to crash hysterically.
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
