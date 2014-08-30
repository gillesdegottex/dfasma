#ifndef GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H
#define GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVAmplitudeSpectrumWDialogSettings;
}

class QGVAmplitudeSpectrum;

class GVAmplitudeSpectrumWDialogSettings : public QDialog
{
    Q_OBJECT

    QGVAmplitudeSpectrum* m_ampspec;

public:
    explicit GVAmplitudeSpectrumWDialogSettings(QGVAmplitudeSpectrum* parent);
    ~GVAmplitudeSpectrumWDialogSettings();

    Ui::GVAmplitudeSpectrumWDialogSettings *ui;
private:

private slots:
    void CBSpectrumWindowTypeCurrentIndexChanged(QString txt);
};

#endif // GVAMPLITUDESPECTRUMWDIALOGSETTINGS_H
