#include "wdialogfiletypechoosertxt.h"
#include "ui_wdialogfiletypechoosertxt.h"

#include "wmainwindow.h"

#include "qaehelpers.h"

WDialogFileTypeChooserTxt::WDialogFileTypeChooserTxt(QWidget *parent, const QString filename)
    : QDialog(parent)
    , ui(new Ui::WDialogFileTypeChooserTxt)
{
    ui->setupUi(this);

    ui->lblFileName->setText(filename);

    ui->cbFileType->addItem("Fundamental Frequency (F0)"); // Index=0

    ui->cbFileType->addItem("Generic Time/Value (add a new view)");

    for(int n=1; n<=gMW->m_wGenericTimeValues.size(); ++n){
        QString nstr = QString::number(n);
        if(n%10==1) nstr+="st";
        else if(n%10==2) nstr+="nd";
        else if(n%10==3) nstr+="rd";
        else nstr+="th";
        ui->cbFileType->addItem("Generic Time/Value (add to "+nstr+" view from the top)");
    }
}

FileType::FType WDialogFileTypeChooserTxt::selectedFileType(){
    if(ui->cbFileType->currentIndex()==0)
        return FileType::FTFZERO;
    else if(ui->cbFileType->currentIndex()>=1)
        return FileType::FTGENTIMEVALUE;

    return FileType::FTUNSET;
}

int WDialogFileTypeChooserTxt::selectedView(){
    if(ui->cbFileType->currentIndex()>1)
        return ui->cbFileType->currentIndex()-2; // Change this if add a type in cbFileType

    return -1;
}

WDialogFileTypeChooserTxt::~WDialogFileTypeChooserTxt(){
    delete ui;
}
