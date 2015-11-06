/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

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
