#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"

#include "gvamplitudespectrum.h"

#include <iostream>
using namespace std;

GVAmplitudeSpectrumWDialogSettings::GVAmplitudeSpectrumWDialogSettings(QGVAmplitudeSpectrum* parent)
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
    gMW->m_settings.add(ui->sbAmplitudeSpectrumOversamplingFactor);

    adjustSize();

    connect(ui->cbAmplitudeSpectrumWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));

    // Update the DFT view automatically
    connect(ui->cbAmplitudeSpectrumLimitWindowDuration, SIGNAL(toggled(bool)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbAmplitudeSpectrumWindowDurationLimit, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbAmplitudeSpectrumOversamplingFactor, SIGNAL(valueChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumWindowSizeForcedOdd, SIGNAL(toggled(bool)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbAmplitudeSpectrumWindowType, SIGNAL(currentIndexChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowNormPower, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowNormSigma, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spAmplitudeSpectrumWindowExpDecay, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
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

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings() {
    delete ui;
}
