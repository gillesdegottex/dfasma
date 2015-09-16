#ifndef GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H
#define GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVAmplitudeSpectrumWDialogSettings;
}

class QGVSpectrumAmplitude;

class GVAmplitudeSpectrumWDialogSettings : public QDialog
{
    Q_OBJECT

    QGVSpectrumAmplitude* m_ampspec;

public:
    explicit GVAmplitudeSpectrumWDialogSettings(QGVSpectrumAmplitude* parent);
    ~GVAmplitudeSpectrumWDialogSettings();

    Ui::GVAmplitudeSpectrumWDialogSettings *ui;
private:

private slots:
    void CBSpectrumWindowTypeCurrentIndexChanged(QString txt);
    void DFTSizeTypeChanged(int index);
};

#endif // GVSPECTRUMAMPLITUDEWDIALOGSETTINGS_H
