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

#include "gvspectrumamplitudewdialogsettings.h"
#include "ui_gvspectrumamplitudewdialogsettings.h"

#include "gvspectrumamplitude.h"

#include <iostream>
using namespace std;

GVAmplitudeSpectrumWDialogSettings::GVAmplitudeSpectrumWDialogSettings(GVSpectrumAmplitude* parent)
    : QDialog((QWidget*)parent)
    , ui(new Ui::GVAmplitudeSpectrumWDialogSettings)
{
    ui->setupUi(this);

    ui->lblAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->hide();
    ui->sbAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->hide();

    m_ampspec = parent;

    // Load the settings
    #ifdef FFTW3RESIZINGMAXTIMESPENT
    ui->lblAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->show();
    ui->sbAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->show();
    gMW->m_settings.add(ui->sbAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation);
    #endif
    gMW->m_settings.add(ui->cbAmplitudeSpectrumWindowSizeForcedOdd);
    gMW->m_settings.add(ui->cbAmplitudeSpectrumLimitWindowDuration);
    gMW->m_settings.add(ui->sbAmplitudeSpectrumWindowDurationLimit);
    gMW->m_settings.add(ui->cbAmplitudeSpectrumWindowType);
    gMW->m_settings.add(ui->spAmplitudeSpectrumWindowNormPower);
    gMW->m_settings.add(ui->spAmplitudeSpectrumWindowNormSigma);
    gMW->m_settings.add(ui->spAmplitudeSpectrumWindowExpDecay);
    ui->lblWindowNormSigma->hide();
    ui->spAmplitudeSpectrumWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spAmplitudeSpectrumWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spAmplitudeSpectrumWindowExpDecay->hide();
    gMW->m_settings.add(ui->cbAmplitudeSpectrumWindowsNormalisation);
    gMW->m_settings.add(ui->sbAmplitudeSpectrumDFTSize);
    gMW->m_settings.add(ui->sbAmplitudeSpectrumOversamplingFactor);
    gMW->m_settings.add(ui->cbAmplitudeSpectrumDFTSizeType);
    DFTSizeTypeChanged(ui->cbAmplitudeSpectrumDFTSizeType->currentIndex());

    adjustSize();

    connect(ui->cbAmplitudeSpectrumWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));
    connect(ui->cbAmplitudeSpectrumDFTSizeType, SIGNAL(currentIndexChanged(int)), this, SLOT(DFTSizeTypeChanged(int)));

    // Update the DFT view automatically
    connect(ui->cbAmplitudeSpectrumLimitWindowDuration, SIGNAL(toggled(bool)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbAmplitudeSpectrumWindowDurationLimit, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumDFTSizeType, SIGNAL(currentIndexChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbAmplitudeSpectrumDFTSize, SIGNAL(valueChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbAmplitudeSpectrumOversamplingFactor, SIGNAL(valueChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumWindowSizeForcedOdd, SIGNAL(toggled(bool)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumWindowType, SIGNAL(currentIndexChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowNormPower, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowNormSigma, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowExpDecay, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumWindowsNormalisation, SIGNAL(currentIndexChanged(int)), m_ampspec, SLOT(settingsModified()));
}

void GVAmplitudeSpectrumWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
    ui->lblWindowNormSigma->hide();
    ui->spAmplitudeSpectrumWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spAmplitudeSpectrumWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spAmplitudeSpectrumWindowExpDecay->hide();
    ui->lblWindowNormSigma->setToolTip("");
    ui->spAmplitudeSpectrumWindowNormSigma->setToolTip("");

    if(txt=="Generalized Normal") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("sigma=");
        ui->lblWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->spAmplitudeSpectrumWindowNormSigma->show();
        ui->spAmplitudeSpectrumWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->lblWindowNormPower->show();
        ui->spAmplitudeSpectrumWindowNormPower->show();
    }
    else if(txt=="Gaussian") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("standard-deviation=");
        ui->lblWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
        ui->spAmplitudeSpectrumWindowNormSigma->show();
        ui->spAmplitudeSpectrumWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
    }
    else if(txt=="Exponential") {
        ui->lblWindowExpDecay->show();
        ui->spAmplitudeSpectrumWindowExpDecay->show();
    }

    adjustSize();
}

void GVAmplitudeSpectrumWDialogSettings::DFTSizeTypeChanged(int index) {
    if(index==0){
        ui->sbAmplitudeSpectrumOversamplingFactor->hide();
        ui->sbAmplitudeSpectrumDFTSize->show();
    }
    else if(index==1){
        ui->sbAmplitudeSpectrumOversamplingFactor->show();
        ui->sbAmplitudeSpectrumDFTSize->hide();
    }
    else if(index==2){
        ui->sbAmplitudeSpectrumOversamplingFactor->hide();
        ui->sbAmplitudeSpectrumDFTSize->hide();
    }
}

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings() {
    delete ui;
}
