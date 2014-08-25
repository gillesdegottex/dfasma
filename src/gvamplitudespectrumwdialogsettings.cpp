#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"

GVAmplitudeSpectrumWDialogSettings::GVAmplitudeSpectrumWDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GVAmplitudeSpectrumWDialogSettings)
{
    ui->setupUi(this);

    #ifndef FFTW3RESIZINGMAXTIMESPENT
    ui->lblFFTW3ResizingMaxTimeSpent->hide();
    ui->sbFFTW3ResizingMaxTimeSpent->hide();
    #endif

    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();

    adjustSize();

    connect(ui->cbSpectrumWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));
}

void GVAmplitudeSpectrumWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
    ui->lblWindowNormSigma->hide();
    ui->spWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spWindowExpDecay->hide();

    if(txt=="Gaussian") {
        ui->lblWindowNormSigma->show();
        ui->spWindowNormSigma->show();
        ui->lblWindowNormPower->show();
        ui->spWindowNormPower->show();
    }
    else if(txt=="Exponential") {
        ui->lblWindowExpDecay->show();
        ui->spWindowExpDecay->show();
    }

    adjustSize();
}

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings()
{
    delete ui;
}
