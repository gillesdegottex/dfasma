#ifndef WDIALOGFILECREATE_H
#define WDIALOGFILECREATE_H

#include <QDialog>

#include "filetype.h"

namespace Ui {
class WDialogFileCreate;
}

class WDialogFileCreate : public QDialog
{
    Q_OBJECT

public:
    explicit WDialogFileCreate(QWidget *parent = 0);
    ~WDialogFileCreate();

    FileType::FType selectedFileType();

private:
    Ui::WDialogFileCreate *ui;
};

#endif // WDIALOGFILECREATE_H
