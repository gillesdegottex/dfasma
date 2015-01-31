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

WDialogSettings::WDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogSettings)
{
    ui->setupUi(this);

    setWindowIcon(QIcon(":/icons/settings.svg"));
    setWindowIconText("Settings");
    setWindowTitle("Settings");

    connect(ui->ckAvoidClicksAddWindows, SIGNAL(toggled(bool)), this, SLOT(setCKAvoidClicksAddWindows(bool)));
    connect(ui->sbButterworthOrder, SIGNAL(valueChanged(int)), this, SLOT(setSBButterworthOrderChangeValue(int)));
    connect(ui->sbAvoidClicksWindowDuration, SIGNAL(valueChanged(double)), this, SLOT(setSBAvoidClicksWindowDuration(double)));
    connect(ui->btnSettingsSave, SIGNAL(clicked()), this, SLOT(settingsSave()));
    connect(ui->btnSettingsClear, SIGNAL(clicked()), this, SLOT(settingsClear()));

    QSettings settings;
    ui->sbButterworthOrder->setValue(settings.value("playback/sbButterworthOrder", 32).toInt());
    ui->cbFilteringCompensateEnergy->setChecked(settings.value("playback/cbFilteringCompensateEnergy", false).toBool());
    ui->ckAvoidClicksAddWindows->setChecked(settings.value("playback/ckAvoidClicksAddWindows", false).toBool());
    ui->sbAvoidClicksWindowDuration->setValue(settings.value("playback/sbAvoidClicksWindowDuration", 0.050).toDouble());

    ui->cbShowMusicNoteNames->setChecked(settings.value("cbShowMusicNoteNames", false).toBool());
    ui->sbToolBarSizes->setValue(settings.value("sbToolBarSizes", 32).toInt());
}

void WDialogSettings::setSBButterworthOrderChangeValue(int order) {
    if(order%2==1)
        ui->sbButterworthOrder->setValue(order+1);
}

void WDialogSettings::setCKAvoidClicksAddWindows(bool add) {
    FTSound::sm_playwin_use = add;
    FTSound::setAvoidClicksWindowDuration(ui->sbAvoidClicksWindowDuration->value());
    ui->sbAvoidClicksWindowDuration->setEnabled(add);
    ui->lblAvoidClicksWindowDurationLabel->setEnabled(add);
}

void WDialogSettings::setSBAvoidClicksWindowDuration(double halfduration) {
    FTSound::setAvoidClicksWindowDuration(halfduration);
}

void WDialogSettings::settingsSave() {
    QSettings settings;
    settings.setValue("playback/AudioOutputDeviceName", ui->cbAudioOutputDevices->currentText());
    settings.setValue("playback/sbButterworthOrder", ui->sbButterworthOrder->value());
    settings.setValue("playback/cbFilteringCompensateEnergy", ui->cbFilteringCompensateEnergy->isChecked());
    settings.setValue("playback/ckAvoidClicksAddWindows", ui->ckAvoidClicksAddWindows->isChecked());
    settings.setValue("playback/sbAvoidClicksWindowDuration", ui->sbAvoidClicksWindowDuration->value());

    settings.setValue("cbShowMusicNoteNames", ui->cbShowMusicNoteNames->isChecked());
    settings.setValue("sbToolBarSizes", ui->sbToolBarSizes->value());

    gMW->settingsSave();
}
void WDialogSettings::settingsClear() {
    QSettings settings;
    settings.clear();
    QMessageBox::information(this, "Reset factory settings", "<p>The settings have been reset to their original values.</p><p>Please restart DFasma</p>");
}

WDialogSettings::~WDialogSettings() {
    delete ui;
}
