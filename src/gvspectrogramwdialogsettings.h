#ifndef GVSPECTROGRAMWDIALOGSETTINGS_H
#define GVSPECTROGRAMWDIALOGSETTINGS_H

#include <QDialog>

#include "qaecolormap.h"

namespace Ui {
class GVSpectrogramWDialogSettings;
}

class QGVSpectrogram;

class GVSpectrogramWDialogSettings : public QDialog
{
    Q_OBJECT

    QGVSpectrogram* m_spectrogram;

    QAEColorMapGray cmapgray;
    QAEColorMapJet cmapjet;

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
    void DFTSizeTypeChanged(int index);
    void DFTSizeChanged(int value);
};

#endif // GVSPECTROGRAMWDIALOGSETTINGS_H
