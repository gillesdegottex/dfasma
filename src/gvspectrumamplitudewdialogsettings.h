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
