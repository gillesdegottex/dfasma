#ifndef GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H
#define GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVAmplitudeSpectrumWDialogSettings;
}

class GVAmplitudeSpectrumWDialogSettings : public QDialog
{
    Q_OBJECT

public:
    explicit GVAmplitudeSpectrumWDialogSettings(QWidget *parent = 0);
    ~GVAmplitudeSpectrumWDialogSettings();

    Ui::GVAmplitudeSpectrumWDialogSettings *ui;
private:
};

#endif // GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H
