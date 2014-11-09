#include "wdialogselectchannel.h"
#include "ui_wdialogselectchannel.h"

WDialogSelectChannel::WDialogSelectChannel(const QString& fileName, int nchan, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogSelectChannel)
{
    ui->setupUi(this);

    ui->lblFileName->setText(fileName);
    ui->sbChannelID->setMaximum(nchan);
}

WDialogSelectChannel::~WDialogSelectChannel()
{
    delete ui;
}
