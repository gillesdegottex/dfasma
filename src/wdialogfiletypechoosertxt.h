#ifndef WDIALOGFILETYPECHOOSERTXT_H
#define WDIALOGFILETYPECHOOSERTXT_H

#include <QDialog>

#include "filetype.h"

namespace Ui {
class WDialogFileTypeChooserTxt;
}

class WDialogFileTypeChooserTxt : public QDialog
{
    Q_OBJECT

public:
    explicit WDialogFileTypeChooserTxt(QWidget *parent, const QString filename);
    ~WDialogFileTypeChooserTxt();

    FileType::FType selectedFileType();
    int selectedView();

private:
    Ui::WDialogFileTypeChooserTxt *ui;
};

#endif // WDIALOGFILETYPECHOOSERTXT_H
