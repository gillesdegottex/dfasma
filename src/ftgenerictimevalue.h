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

#ifndef FTGENERICTIMEVALUE_H
#define FTGENERICTIMEVALUE_H

#include <deque>
#include <vector>

class QString;
class QColor;
class QAction;
class QGraphicsSimpleTextItem;
class QAEGISampledSignal;
class WidgetGenericTimeValue;

#include "filetype.h"

class GVGenericTimeValue;

class FTGenericTimeValue : public QObject, public FileType
{
    Q_OBJECT

    double m_meandts;
    double m_meanvalue;
    double m_valuemin;
    double m_valuemax;

    QString m_dataselectors;

public:
    enum FileFormat {FFNotSpecified=0, FFAutoDetect, FFAsciiAutoDetect, FFAsciiTimeValue, FFAsciiValue, FFBinaryAutoDetect, FFBinaryFloat32, FFBinaryFloat64, FFSDIF};
    static std::deque<QString> s_formatstrings;

private:
    static struct ClassConstructor{ClassConstructor();} s_class_constructor;
    void constructor_internal(GVGenericTimeValue* view);
    void constructor_external();
    void load();

    FileFormat m_fileformat;

public:
    FTGenericTimeValue(const QString& _fileName, WidgetGenericTimeValue* parent, FileType::FileContainer container=FileType::FCUNSET, FileFormat fileformat=FFNotSpecified);
    virtual FileType* duplicate();
    FTGenericTimeValue(const FTGenericTimeValue& ft);  // Duplicate
    ~FTGenericTimeValue();

    GVGenericTimeValue* m_view;
    GVGenericTimeValue* gview() const {return m_view;}

    std::vector<double> ts;
    std::vector<double> values;
    QAEGISampledSignal* m_giGenericTimeValue;
    double m_values_min;
    double m_values_max;

//    QGraphicsSimpleTextItem* m_aspec_txt; // TODO
    virtual void fillContextMenu(QMenu& contextmenu);
    void setColor(const QColor& _color);
    virtual void zposReset();
    virtual void zposBringForward();

    void updateStatistics();
    virtual QString info() const;
    virtual double getLastSampleTime() const;

    // Edition
//    void edit(double t, double f0);

public slots:
    bool reload();
//    void save();
//    void saveAs();
    void setVisible(bool shown);
    void updateTextsGeometry();
    void updateMinMaxValues();
};

#endif // FTGENERICTIMEVALUE_H
