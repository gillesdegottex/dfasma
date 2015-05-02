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

    gMW->m_settings.add(ui->cbSpectrogramWindowSizeForcedOdd);
    gMW->m_settings.add(ui->cbSpectrogramWindowType);
    gMW->m_settings.add(ui->spSpectrogramWindowNormPower);
    gMW->m_settings.add(ui->spSpectrogramWindowNormSigma);
    gMW->m_settings.add(ui->spSpectrogramWindowExpDecay);
    ui->lblWindowNormSigma->hide();
    ui->spSpectrogramWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spSpectrogramWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spSpectrogramWindowExpDecay->hide();
    gMW->m_settings.add(ui->sbSpectrogramStepSize);
    gMW->m_settings.add(ui->sbSpectrogramWindowSize);
    gMW->m_settings.add(ui->sbSpectrogramOversamplingFactor);
    gMW->m_settings.add(ui->gbSpectrogramCepstralLiftering);
    gMW->m_settings.add(ui->sbSpectrogramCepstralLifteringOrder);
    gMW->m_settings.add(ui->cbSpectrogramCepstralLifteringPreserveDC);
    QStringList colormaps = ColorMap::getAvailableColorMaps();
    for(QStringList::Iterator it=colormaps.begin(); it!=colormaps.end(); ++it)
        ui->cbSpectrogramColorMaps->addItem(*it);
    gMW->m_settings.add(ui->cbSpectrogramColorMaps);
    gMW->m_settings.add(ui->cbSpectrogramColorMapReversed);
    gMW->m_settings.add(ui->cbSpectrogramF0ShowHarmonics);

    adjustSize();

    connect(ui->cbSpectrogramWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));
}

void GVSpectrogramWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
    ui->lblWindowNormSigma->hide();
    ui->spSpectrogramWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spSpectrogramWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spSpectrogramWindowExpDecay->hide();
    ui->lblWindowNormSigma->setToolTip("");
    ui->spSpectrogramWindowNormSigma->setToolTip("");

    if(txt=="Generalized Normal") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("sigma=");
        ui->lblWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->spSpectrogramWindowNormSigma->show();
        ui->spSpectrogramWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->lblWindowNormPower->show();
        ui->spSpectrogramWindowNormPower->show();
    }
    else if(txt=="Gaussian") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("standard-deviation=");
        ui->lblWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
        ui->spSpectrogramWindowNormSigma->show();
        ui->spSpectrogramWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
    }
    else if(txt=="Exponential") {
        ui->lblWindowExpDecay->show();
        ui->spSpectrogramWindowExpDecay->show();
    }

    adjustSize();
}

GVSpectrogramWDialogSettings::~GVSpectrogramWDialogSettings()
{
    delete ui;
}
