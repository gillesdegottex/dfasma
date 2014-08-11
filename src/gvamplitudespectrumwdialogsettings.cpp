#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"

GVAmplitudeSpectrumWDialogSettings::GVAmplitudeSpectrumWDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GVAmplitudeSpectrumWDialogSettings)
{
    ui->setupUi(this);
}

GVAmplitudeSpectrumWDialogSettings::~GVAmplitudeSpectrumWDialogSettings()
{
    delete ui;
}
