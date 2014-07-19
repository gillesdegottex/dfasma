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

#ifndef FILETYPE_H
#define FILETYPE_H

#include <QString>
#include <QColor>
#include <QAction>
#include <QListWidgetItem>

class WMainWindow;

class FileType : public QListWidgetItem
{
public:
    enum FILETYPE {FTUNSET, FTSOUND, FTFZERO, FTLABELS};

    FileType(FILETYPE _type, const QString& _fileName, QObject *parent);

    virtual double getLastSampleTime() const =0;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);

    FILETYPE type;
    QString fileName;
    QColor color;

    QAction* m_actionShow;

    ~FileType();
};

#endif // FILETYPE_H
