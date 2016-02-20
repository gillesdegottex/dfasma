#include "wdialogfilecreate.h"
#include "ui_wdialogfilecreate.h"

WDialogFileCreate::WDialogFileCreate(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogFileCreate)
{
    ui->setupUi(this);

    setWindowTitle("Create a new file...");
}

WDialogFileCreate::~WDialogFileCreate()
{
    delete ui;
}

FileType::FType WDialogFileCreate::selectedFileType()
{
    if(ui->rbFTLabel->isChecked())
        return FileType::FTLABELS;
    else if(ui->rbFTFZero->isChecked())
        return FileType::FTFZERO;
    else
        return FileType::FTUNSET;
}
