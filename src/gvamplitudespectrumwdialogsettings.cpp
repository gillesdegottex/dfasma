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

    m_ampspec = parent;

    #ifndef FFTW3RESIZINGMAXTIMESPENT
    ui->lblFFTW3ResizingMaxTimeSpent->hide();
    ui->sbFFTW3ResizingMaxTimeSpent->hide();
    #endif

    // Load the settings
    QSettings settings;
    ui->sbSpectrumAmplitudeRangeMin->setValue(settings.value("qgvamplitudespectrum/sbSpectrumAmplitudeRangeMin", -215).toInt());
    ui->sbSpectrumAmplitudeRangeMax->setValue(settings.value("qgvamplitudespectrum/sbSpectrumAmplitudeRangeMax", 10).toInt());
    ui->sbSpectrumOversamplingFactor->setValue(settings.value("qgvamplitudespectrum/sbSpectrumOversamplingFactor", 1).toInt());
    ui->sbFFTW3ResizingMaxTimeSpent->setValue(settings.value("qgvamplitudespectrum/sbFFTW3ResizingMaxTimeSpent", 1).toDouble());

    ui->cbWindowSizeForcedOdd->setChecked(settings.value("qgvamplitudespectrum/cbWindowSizeForcedOdd", false).toBool());
    ui->cbSpectrumAmplitudeLimitWindowDuration->setChecked(settings.value("qgvamplitudespectrum/cbSpectrumAmplitudeLimitWindowDuration", true).toBool());
    ui->sbSpectrumAmplitudeWindowDurationLimit->setValue(settings.value("qgvamplitudespectrum/sbSpectrumAmplitudeWindowDurationLimit", 1.0).toDouble());

    ui->cbSpectrumWindowType->setCurrentIndex(settings.value("qgvamplitudespectrum/cbSpectrumWindowType", 0).toInt());
    ui->spWindowNormPower->setValue(settings.value("qgvamplitudespectrum/spWindowNormPower", 2.0).toDouble());
    ui->spWindowNormSigma->setValue(settings.value("qgvamplitudespectrum/spWindowNormSigma", 0.3).toDouble());
    ui->spWindowExpDecay->setValue(settings.value("qgvamplitudespectrum/spWindowExpDecay", 60.0).toDouble());

    ui->cbShowMusicNoteNames->setChecked(settings.value("qgvamplitudespectrum/cbShowMusicNoteNames", false).toBool());
    ui->cbAddMarginsOnSelection->setChecked(settings.value("qgvamplitudespectrum/cbAddMarginsOnSelection", true).toBool());

    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();

    adjustSize();

    connect(ui->cbSpectrumWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));

    // Update the DFT view automatically
    connect(ui->sbSpectrumAmplitudeRangeMin, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbSpectrumAmplitudeRangeMax, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->sbSpectrumOversamplingFactor, SIGNAL(valueChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbWindowSizeForcedOdd, SIGNAL(toggled(bool)), m_ampspec, SLOT(settingsModified()));
    connect(ui->cbSpectrumWindowType, SIGNAL(currentIndexChanged(int)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spWindowNormPower, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spWindowNormSigma, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
    connect(ui->spWindowExpDecay, SIGNAL(valueChanged(double)), m_ampspec, SLOT(settingsModified()));
}

void GVAmplitudeSpectrumWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();
    ui->lblWindowNormSigma->setToolTip("");
    ui->spWindowNormSigma->setToolTip("");

    if(txt=="Generalized Normal") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("sigma=");
        ui->lblWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->spWindowNormSigma->show();
        ui->spWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->lblWindowNormPower->show();
        ui->spWindowNormPower->show();
    }
    else if(txt=="Gaussian") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("standard-deviation=");
        ui->lblWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
        ui->spWindowNormSigma->show();
        ui->spWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
    }
    else if(txt=="Exponential") {
        ui->lblWindowExpDecay->show();
        ui->spWindowExpDecay->show();
    }

    adjustSize();
}

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings() {
    delete ui;
}
