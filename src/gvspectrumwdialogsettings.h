#ifndef GVSPECTRUMWDIALOGSETTINGS_H
#define GVSPECTRUMWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVSpectrumWDialogSettings;
}

class GVSpectrumWDialogSettings : public QDialog
{
    Q_OBJECT

public:
    explicit GVSpectrumWDialogSettings(QWidget *parent = 0);
    ~GVSpectrumWDialogSettings();

    Ui::GVSpectrumWDialogSettings *ui;
private:
};

#endif // GVSPECTRUMWDIALOGSETTINGS_H
