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

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings()
{
    delete ui;
}
