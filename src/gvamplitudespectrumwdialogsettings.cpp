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
}

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings()
{
    delete ui;
}
