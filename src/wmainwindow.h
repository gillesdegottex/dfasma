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

#ifndef WMAINWINDOW_H
#define WMAINWINDOW_H

#include <deque>

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QColor>
#include <QListWidgetItem>
#include <QThread>
#include <QAudio>
#include <QAudioDeviceInfo>
#include <QTimer>
#include <QSettings>

#include "QSettingsAuto.h"
#include "wdialogsettings.h"
#include "filetype.h"

class FTSound;
class FTFZero;
class FTLabels;
class AudioEngine;
class QGVWaveform;
class QGVAmplitudeSpectrum;
class QGVPhaseSpectrum;
class QGVSpectrogram;
class QHBoxLayout;
class QProgressBar;
class QProgressDialog;
class QLabel;
class QxtSpanSlider;

namespace Ui {
class WMainWindow;
}

class WMainWindow;
extern WMainWindow* gMW; // Global accessor of the Main Window

class WMainWindow : public QMainWindow
{
    Q_OBJECT

    static WMainWindow* sm_mainwindow;
    bool m_loading;

    FTSound* m_lastSelectedSound;
    FTSound* m_lastFilteredSound;

    void addFilesRecursive(const QStringList& files, FileType::FType type=FileType::FTUNSET);
    QProgressDialog* m_prgdlg;
    void stopFileProgressDialog();

    void connectModes();
    void disconnectModes();

    void initializeSoundSystem(double fs);
    QProgressBar* m_pbVolume;

protected:
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void dropEvent(QDropEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);

private slots:
    void newFile();
    void openFile();
    void selectedFilesClose();
    void selectedFilesReload();
    void selectedFilesToggleShown();
    void play();
    void enablePlay();
    void localEnergyChanged(double);

    void audioStateChanged(QAudio::State state);
    void audioOutputFormatChanged(const QAudioFormat& format);
    void showFileContextMenu(const QPoint&);
    void resetAmpScale();
    void resetDelay();
    void setSelectionMode(bool checked);
    void setEditMode(bool checked);
    void setLabelsEditable(bool editable);
    void execAbout();
    void fileSelectionChanged();
    void viewsDisplayedChanged();
    void changeToolBarSizes(int size);

public slots:
    void updateWindowTitle();
    void fileInfoUpdate();
    void allSoundsChanged(); // TODO Should drop this
    void selectAudioOutputDevice(int di);
    void selectAudioOutputDevice(const QString& devicename);
    void colorSelected(const QColor& color);
    void checkFileModifications();
    void duplicateCurrentFile();
    void setInWaitingForFileState();
    void updateViewsAfterAddFile(bool isfirsts);
    void changeFileListItemsSize();

public:
    explicit WMainWindow(QStringList files, QWidget* parent=0);
    ~WMainWindow();

    QSettingsAuto m_settings;
    WDialogSettings* m_dlgSettings;

    Ui::WMainWindow* ui;
    bool isLoading() const {return m_loading;}

    // Waiting bar for operations blocking the main window
    // (The DFT resizing is NOT blocking because in a separate thread)
    QLabel* m_globalWaitingBarLabel;
    QProgressBar* m_globalWaitingBar;

    void addFiles(const QStringList& files, FileType::FType type=FileType::FTUNSET);
    void addFile(const QString& filepath, FileType::FType type=FileType::FTUNSET);
    std::deque<FTSound*> ftsnds;
    std::deque<FTFZero*> ftfzeros;
    std::deque<FTLabels*> ftlabels;
    FTSound* getCurrentFTSound(bool forceselect=false);
    FTLabels* getCurrentFTLabels(bool forceselect=false);

    double getFs();
    unsigned int getMaxWavSize();
    double getMaxDuration();
    double getMaxLastSampleTime();

    QGVWaveform* m_gvWaveform;
    QGVAmplitudeSpectrum* m_gvAmplitudeSpectrum;
    QGVPhaseSpectrum* m_gvPhaseSpectrum;
    QGVSpectrogram* m_gvSpectrogram;
    QxtSpanSlider* m_qxtSpectrogramSpanSlider;

    AudioEngine* m_audioengine;
    FTSound* m_playingftsound;
};

#endif // WMAINWINDOW_H
