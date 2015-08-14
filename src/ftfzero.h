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

#ifndef FTFZERO_H
#define FTFZERO_H

#include <deque>
#include <vector>

#include <QString>
#include <QColor>
#include <QAction>
class QGraphicsSimpleTextItem;

#include "filetype.h"
class FTSound;

class FTFZero : public QObject, public FileType
{
    Q_OBJECT

    void init();
    void load();

    QAction* m_actionSave;
    QAction* m_actionSaveAs;

    bool m_isedited;

    int m_fileformat;

public:
    enum FileFormat {FFNotSpecified=0, FFAutoDetect, FFAsciiAutoDetect, FFAsciiTimeValue, FFSDIF};
    static std::deque<QString> m_formatstrings;
//    FTFZero(QObject* parent, const QString& _fileName="");
    FTFZero(const QString& _fileName, QObject* parent, FileType::FileContainer container=FileType::FCUNSET, FileFormat fileformat=FFNotSpecified);
    virtual FileType* duplicate();
    FTFZero(const FTFZero& ft);  // Duplicate
    FTFZero(FTSound *ftsnd, double f0min, double f0max, QObject* parent); // Analysis

    std::deque<double> ts;
    std::deque<double> f0s;

    QGraphicsSimpleTextItem* m_aspec_txt;
    virtual void fillContextMenu(QMenu& contextmenu);
    void updateTextsGeometry();
    void setColor(const QColor& _color);

    virtual QString info() const;
    virtual double getLastSampleTime() const;

    ~FTFZero();

public slots:
    bool reload();
    void save();
    void saveAs();
    void setVisible(bool shown);
};

#endif // FTFZERO_H
