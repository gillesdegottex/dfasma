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
    ui->cbWindowSizeForcedOdd->setChecked(settings.value("qgvspectrogram/cbWindowSizeForcedOdd", false).toBool());
    ui->cbSpectrogramWindowType->setCurrentIndex(settings.value("qgvspectrogram/cbSpectrogramWindowType", 0).toInt());
    ui->spWindowNormPower->setValue(settings.value("qgvspectrogram/spWindowNormPower", 2.0).toDouble());
    ui->spWindowNormSigma->setValue(settings.value("qgvspectrogram/spWindowNormSigma", 0.3).toDouble());
    ui->spWindowExpDecay->setValue(settings.value("qgvspectrogram/spWindowExpDecay", 60.0).toDouble());
    ui->sbSpectrogramOversamplingFactor->setValue(settings.value("qgvspectrogram/sbSpectrogramOversamplingFactor", 1).toInt());

    ui->sbStepSize->setValue(settings.value("qgvspectrogram/sbStepSize", 0.005).toDouble());
    ui->sbWindowSize->setValue(settings.value("qgvspectrogram/sbWindowSize", 0.030).toDouble());
    ui->sbSpectrogramOversamplingFactor->setValue(settings.value("qgvspectrogram/sbSpectrogramOversamplingFactor", 1).toInt());

    ui->gbCepstralLiftering->setChecked(settings.value("qgvspectrogram/gbCepstralLiftering", false).toBool());
    ui->sbCepstralLifteringOrder->setValue(settings.value("qgvspectrogram/sbCepstralLifteringOrder", 8).toInt());
    ui->cbCepstralLifteringPreserveDC->setChecked(settings.value("qgvspectrogram/cbCepstralLifteringPreserveDC", true).toBool());

    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();

    QStringList colormaps = ColorMap::getAvailableColorMaps();
    for(QStringList::Iterator it=colormaps.begin(); it!=colormaps.end(); ++it)
        ui->cbSpectrogramColorMaps->addItem(*it);
    ui->cbSpectrogramColorMaps->setCurrentIndex(settings.value("qgvspectrogram/cbSpectrogramColorMaps", 0).toInt());
    ui->cbSpectrogramColorMapReversed->setChecked(settings.value("qgvspectrogram/cbSpectrogramColorMapReversed", true).toBool());

    ui->cbF0ShowHarmonics->setChecked(settings.value("qgvspectrogram/cbF0ShowHarmonics", false).toBool());

    adjustSize();

    connect(ui->cbSpectrogramWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));

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
GVSpectrogramWDialogSettings::~GVSpectrogramWDialogSettings()
{
    delete ui;
}
