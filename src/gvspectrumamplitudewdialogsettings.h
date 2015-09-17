#ifndef GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H
#define GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVAmplitudeSpectrumWDialogSettings;
}

class GVSpectrumAmplitude;

class GVAmplitudeSpectrumWDialogSettings : public QDialog
{
    Q_OBJECT

    GVSpectrumAmplitude* m_ampspec;

public:
    explicit GVAmplitudeSpectrumWDialogSettings(GVSpectrumAmplitude* parent);
    ~GVAmplitudeSpectrumWDialogSettings();

    Ui::GVAmplitudeSpectrumWDialogSettings *ui;
private:

private slots:
    void CBSpectrumWindowTypeCurrentIndexChanged(QString txt);
    void DFTSizeTypeChanged(int index);
};

#endif // GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H
