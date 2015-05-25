#ifndef GVSPECTROGRAMWDIALOGSETTINGS_H
#define GVSPECTROGRAMWDIALOGSETTINGS_H

#include <QDialog>

#include "colormap.h"

namespace Ui {
class GVSpectrogramWDialogSettings;
}

class QGVSpectrogram;

class GVSpectrogramWDialogSettings : public QDialog
{
    Q_OBJECT

    QGVSpectrogram* m_spectrogram;

    ColorMapGray cmapgray;
    ColorMapJet cmapjet;

public:
    explicit GVSpectrogramWDialogSettings(QGVSpectrogram* parent);
    ~GVSpectrogramWDialogSettings();

    Ui::GVSpectrogramWDialogSettings *ui;

    void settingsSave();

private:

private slots:
    void CBSpectrumWindowTypeCurrentIndexChanged(QString txt);

public slots:
    void checkImageSize();
};

#endif // GVSPECTROGRAMWDIALOGSETTINGS_H
