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
#include "ftsound.h"

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
}

void WDialogSettings::setSBButterworthOrderChangeValue(int order) {
    if(order%2==1)
        ui->sbButterworthOrder->setValue(order+1);
}

void WDialogSettings::setCKAvoidClicksAddWindows(bool add) {
    FTSound::sm_playwin_use = add;
    ui->sbAvoidClicksWindowDuration->setEnabled(add);
    ui->lblAvoidClicksWindowDurationLabel->setEnabled(add);

    FTSound::setAvoidClicksWindowDuration(ui->sbAvoidClicksWindowDuration->value());
}

void WDialogSettings::setSBAvoidClicksWindowDuration(double halfduration) {
    FTSound::setAvoidClicksWindowDuration(halfduration);
}

WDialogSettings::~WDialogSettings() {
    delete ui;
}
