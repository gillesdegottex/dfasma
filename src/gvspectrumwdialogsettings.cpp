#include "gvspectrumwdialogsettings.h"
#include "ui_gvspectrumwdialogsettings.h"

GVSpectrumWDialogSettings::GVSpectrumWDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GVSpectrumWDialogSettings)
{
    ui->setupUi(this);
}

GVSpectrumWDialogSettings::~GVSpectrumWDialogSettings()
{
    delete ui;
}
