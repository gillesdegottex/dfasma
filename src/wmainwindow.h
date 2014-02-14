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

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QColor>
#include <QListWidgetItem>
#include <QThread>
#include <QTimer>

#include "wdialogsettings.h"

#include "iodsound.h"
#include "../external/audioengine/audioengine.h"

namespace Ui {
class WMainWindow;
}

class QGVWaveform;
class QGVSpectrum;

class WMainWindow : public QMainWindow
{
    Q_OBJECT

    void initializeSoundSystem(float fs);

    void connectModes();
    void disconnectModes();

protected:
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);

private slots:
    void openFile();
    void closeSelectedFile();
    void play();
    void enablePlay();
    void localEnergyChanged(double);

    void audioStateChanged(QAudio::State state);
    void audioOutputDeviceChanged(const QAudioDeviceInfo& device);
    void execAbout();
    void showSoundContextMenu(const QPoint&);
    void soundsChanged();
    void setSoundShown(bool show);
    void resetAmpScale();
    void resetDelay();
    void setSelectionMode(bool checked);
    void setEditMode(bool checked);

public:
    explicit WMainWindow(QStringList sndfiles, QWidget* parent=0);
    ~WMainWindow();

    Ui::WMainWindow* ui;

    WDialogSettings* m_dlgSettings;

    std::deque<IODSound*> snds;
    void addFile(const QString& filepath);
    inline bool hasFilesLoaded() const {return snds.size();}
    float getFs();
    unsigned int getMaxWavSize();
    float getMaxDuration();
    float getMaxLastSampleTime();

    QGVWaveform* m_gvWaveform;
    QGVSpectrum* m_gvSpectrum;

    AudioEngine* m_audioengine;
};



#endif // WMAINWINDOW_H
