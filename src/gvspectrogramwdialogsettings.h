#ifndef GVSPECTROGRAMWDIALOGSETTINGS_H
#define GVSPECTROGRAMWDIALOGSETTINGS_H

#include <QDialog>

namespace Ui {
class GVSpectrogramWDialogSettings;
}

class QGVSpectrogram;

class GVSpectrogramWDialogSettings : public QDialog
{
    Q_OBJECT

    QGVSpectrogram* m_spectrogram;

public:
    explicit GVSpectrogramWDialogSettings(QGVSpectrogram* parent);
    ~GVSpectrogramWDialogSettings();

    Ui::GVSpectrogramWDialogSettings *ui;
private:

private slots:
    void CBSpectrumWindowTypeCurrentIndexChanged(QString txt);
};

#endif // GVSPECTROGRAMWDIALOGSETTINGS_H
