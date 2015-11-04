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

#include "fileslistwidget.h"
#include "qaesettingsauto.h"
#include "wdialogsettings.h"
#include "filetype.h"

class FTSound;
class FTFZero;
class FTLabels;
class AudioEngine;
class GVWaveform;
class GVSpectrumAmplitude;
class GVSpectrumPhase;
class GVSpectrumGroupDelay;
class GVSpectrogram;
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

    bool m_loading;
    QString m_version;

    FilesListWidget* m_fileslist;
    FileType* m_last_file_editing;

    void connectModes();
    void disconnectModes();

    QProgressBar* m_pbVolume;
    QAction* m_pbVolumeAction;
    QAction* m_audioSeparatorAction;

    // Global waiting bar for operations blocking the main window
    QProgressBar* m_globalWaitingBar;

protected:
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void dropEvent(QDropEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);

private slots:
    void newFile();
    void openFile();

    void play(bool filtered=false);
    void playFiltered();
    void audioStateChanged(QAudio::State state);
    void audioOutputFormatChanged(const QAudioFormat& format);
    void enablePlay();
    void localEnergyChanged(double);

    void setSelectionMode(bool checked);
    void setEditMode(bool checked);
    void execAbout();
    void viewsDisplayedChanged();
    void viewsSpectrogramToggled(bool show);
    void changeToolBarSizes(int size);

public slots:
    void focusWindowChanged(QWindow*win);
    void updateWindowTitle();
    void allSoundsChanged(); // TODO Should drop this
    void audioSelectOutputDevice(int di);
    void audioSelectOutputDevice(const QString& devicename);
    void audioEnable(bool enable);
    void audioInitialize(double fs);
    void resetFiltering();

    void setInWaitingForFileState();
    void updateViewsAfterAddFile(bool isfirsts);
    void setEditing(FileType* ft);
    void checkEditHiddenFile();
    void updateMouseCursorState(bool kshift, bool kcontrol);

public:
    explicit WMainWindow(QStringList files, QWidget* parent=0);
    ~WMainWindow();
    bool isLoading() const {return m_loading;}
    QString version();

    QAESettingsAuto m_settings;
    WDialogSettings* m_dlgSettings;
    Ui::WMainWindow* ui;

    void globalWaitingBarMessage(const QString& statusmessage, int max=0);
    void globalWaitingBarSetMaximum(int max);
    void globalWaitingBarSetValue(int value);
    void globalWaitingBarDone();
    void globalWaitingBarClear();
    void statusBarSetText(const QString& text, int timeout=0, QColor color=QColor());

    // Views
    GVWaveform* m_gvWaveform;
    GVSpectrumAmplitude* m_gvSpectrumAmplitude;
    GVSpectrumPhase* m_gvSpectrumPhase;
    GVSpectrumGroupDelay* m_gvSpectrumGroupDelay;
    GVSpectrogram* m_gvSpectrogram;
    QxtSpanSlider* m_qxtSpectrogramSpanSlider;

    // Audio
    AudioEngine* m_audioengine;
    FTSound* m_playingftsound;
    FTSound* m_lastFilteredSound;
};

#endif // WMAINWINDOW_H
