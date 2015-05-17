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

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include <iostream>
#include <QSettings>
#include <QMessageBox>

#include "ftsound.h"
#include "wmainwindow.h"
#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "gvspectrogram.h"
#include "gvspectrogramwdialogsettings.h"

WDialogSettings::WDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogSettings)
{
    ui->setupUi(this);

    setWindowIcon(QIcon(":/icons/settings.svg"));
    setWindowIconText("Settings");
    setWindowTitle("Settings");

    connect(ui->ckPlaybackAvoidClicksAddWindows, SIGNAL(toggled(bool)), this, SLOT(setCKAvoidClicksAddWindows(bool)));
    connect(ui->sbPlaybackButterworthOrder, SIGNAL(valueChanged(int)), this, SLOT(setSBButterworthOrderChangeValue(int)));
    connect(ui->sbPlaybackAvoidClicksWindowDuration, SIGNAL(valueChanged(double)), this, SLOT(setSBAvoidClicksWindowDuration(double)));
    connect(ui->btnSettingsSave, SIGNAL(clicked()), this, SLOT(settingsSave()));
    connect(ui->btnSettingsClear, SIGNAL(clicked()), this, SLOT(settingsClear()));  

    gMW->m_settings.add(ui->sbPlaybackButterworthOrder);
    gMW->m_settings.add(ui->cbPlaybackFilteringCompensateEnergy);
    gMW->m_settings.add(ui->ckPlaybackAvoidClicksAddWindows);
    gMW->m_settings.add(ui->sbPlaybackAvoidClicksWindowDuration);
    gMW->m_settings.add(ui->sbViewsToolBarSizes);
    gMW->m_settings.add(ui->sbFileListItemSize);
    gMW->m_settings.add(ui->sbViewsTimeDecimals);
    gMW->m_settings.add(ui->cbViewsShowMusicNoteNames);
    gMW->m_settings.add(ui->cbViewsAddMarginsOnSelection);

    adjustSize();
}

void WDialogSettings::setSBButterworthOrderChangeValue(int order) {
    if(order%2==1)
        ui->sbPlaybackButterworthOrder->setValue(order+1);
}

void WDialogSettings::setCKAvoidClicksAddWindows(bool add) {
    FTSound::sm_playwin_use = add;
    FTSound::setAvoidClicksWindowDuration(ui->sbPlaybackAvoidClicksWindowDuration->value());
    ui->sbPlaybackAvoidClicksWindowDuration->setEnabled(add);
    ui->lblAvoidClicksWindowDurationLabel->setEnabled(add);
}

void WDialogSettings::setSBAvoidClicksWindowDuration(double halfduration) {
    FTSound::setAvoidClicksWindowDuration(halfduration);
}

void WDialogSettings::settingsSave() {
    // Save all the automatic settings
    gMW->m_settings.saveAll();

    // Save the particular global settings
    gMW->m_settings.setValue("cbPlaybackAudioOutputDevices", ui->cbPlaybackAudioOutputDevices->currentText());

    // Save the particular settings of different widgets
    gMW->m_gvSpectrogram->settingsSave();
}
void WDialogSettings::settingsClear() {
    gMW->m_settings.clearAll();
    QMessageBox::information(this, "Reset factory settings", "<p>The settings have been reset to their original values.</p><p>Please restart DFasma</p>");
}

WDialogSettings::~WDialogSettings() {
    delete ui;
}
