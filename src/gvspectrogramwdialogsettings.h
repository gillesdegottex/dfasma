#ifndef GVSPECTROGRAMWDIALOGSETTINGS_H
#define GVSPECTROGRAMWDIALOGSETTINGS_H

#include <QDialog>

#include "qaecolormap.h"

namespace Ui {
class GVSpectrogramWDialogSettings;
}

class GVSpectrogram;

class GVSpectrogramWDialogSettings : public QDialog
{
    Q_OBJECT

    GVSpectrogram* m_spectrogram;

    QAEColorMapGray cmapgray;
    QAEColorMapJet cmapjet;
    QAEColorMapTransparent cmaptrans;

public:
    explicit GVSpectrogramWDialogSettings(GVSpectrogram* parent);
    ~GVSpectrogramWDialogSettings();

    Ui::GVSpectrogramWDialogSettings *ui;

    void settingsSave();

private:

private slots:
    void windowTypeCurrentIndexChanged(QString txt);
    void colorRangeModeCurrentIndexChanged(int index);

public slots:
    void checkImageSize();
    void DFTSizeTypeChanged(int index);
    void DFTSizeChanged(int value);
};

#endif // GVSPECTROGRAMWDIALOGSETTINGS_H
