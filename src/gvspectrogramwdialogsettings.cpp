#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"

#include "gvspectrogram.h"

#include <QSettings>

GVSpectrogramWDialogSettings::GVSpectrogramWDialogSettings(QGVSpectrogram* parent) :
    QDialog((QWidget*)parent),
    ui(new Ui::GVSpectrogramWDialogSettings)
{
    ui->setupUi(this);

    m_spectrogram = parent;

    // Load the settings
    QSettings settings;
    ui->sbSpectrogramAmplitudeRangeMin->setValue(settings.value("qgvspectrogram/sbSpectrogramAmplitudeRangeMin", -215).toInt());
    ui->sbSpectrogramAmplitudeRangeMax->setValue(settings.value("qgvspectrogram/sbSpectrogramAmplitudeRangeMax", 10).toInt());
    ui->sbSpectrogramOversamplingFactor->setValue(settings.value("qgvspectrogram/sbSpectrogramOversamplingFactor", 1).toInt());

    ui->cbWindowSizeForcedOdd->setChecked(settings.value("qgvspectrogram/cbWindowSizeForcedOdd", false).toBool());
    ui->cbSpectrogramWindowType->setCurrentIndex(settings.value("qgvspectrogram/cbSpectrogramWindowType", 0).toInt());
    ui->spWindowNormPower->setValue(settings.value("qgvspectrogram/spWindowNormPower", 2.0).toDouble());
    ui->spWindowNormSigma->setValue(settings.value("qgvspectrogram/spWindowNormSigma", 0.3).toDouble());
    ui->spWindowExpDecay->setValue(settings.value("qgvspectrogram/spWindowExpDecay", 60.0).toDouble());

    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();

    adjustSize();

    connect(ui->cbSpectrogramWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));

    // Update the DFT view automatically
    connect(ui->sbSpectrogramAmplitudeRangeMin, SIGNAL(valueChanged(double)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->sbSpectrogramAmplitudeRangeMax, SIGNAL(valueChanged(double)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->sbSpectrogramOversamplingFactor, SIGNAL(valueChanged(int)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->cbWindowSizeForcedOdd, SIGNAL(toggled(bool)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->cbSpectrogramWindowType, SIGNAL(currentIndexChanged(int)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->spWindowNormPower, SIGNAL(valueChanged(double)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->spWindowNormSigma, SIGNAL(valueChanged(double)), m_spectrogram, SLOT(settingsModified()));
    connect(ui->spWindowExpDecay, SIGNAL(valueChanged(double)), m_spectrogram, SLOT(settingsModified()));
}

void GVSpectrogramWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
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
    if(txt=="Gaussian") {
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

GVSpectrogramWDialogSettings::~GVSpectrogramWDialogSettings()
{
    delete ui;
}
