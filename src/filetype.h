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
#include <QDateTime>

class WMainWindow;

class FileType : public QListWidgetItem
{
protected:
    virtual void load(const QString& _fileName) =0;

    QDateTime m_lastreadtime;

protected:
    void checkFileExists(const QString& fullfilepath);

public:
    enum FILETYPE {FTUNSET, FTSOUND, FTFZERO, FTLABELS};

    FileType(FILETYPE _type, const QString& _fileName, QObject *parent);

    FILETYPE type;
    QString fileFullPath;
    QColor color;

    QAction* m_actionShow;
    QAction* m_actionReload;

    ~FileType();

    #ifdef SUPPORT_SDIF
    static bool SDIF_hasFrame(const QString& filename, const QString& framesignature);
    #endif

    virtual QString info() const =0;
    void setShown(bool shown);
    virtual bool isModified() {return false;}
    void setColor(const QColor& _color);
    virtual double getLastSampleTime() const =0;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);

    virtual void setStatus();
};

#endif // FILETYPE_H
