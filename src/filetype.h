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
    QDateTime m_modifiedtime;
    QDateTime m_lastreadtime;

    virtual void setDrawIcon(QPixmap& pm);

public:
    enum FType {FTUNSET=0, FTSOUND, FTFZERO, FTLABELS}; // Names corresponding to possible classes
    enum FileContainer {FCUNSET=0, FCANYSOUND, FCASCII, FCSDIF}; // File Containers (not format !)

    FileType(FType _type, const QString& _fileName, QObject *parent);

    FType type;
    QString fileFullPath;
    QColor color;

    QAction* m_actionShow;
    QAction* m_actionDuplicate;

    ~FileType();

    static bool hasFileExtension(const QString& filepath, const QString& ext);
    static bool isFileASCII(const QString& filename);
    static FileContainer guessContainer(const QString& filepath);
    #ifdef SUPPORT_SDIF
    // mexAtExit cleanup function
    static void cleanupAndEEnd();
    static bool isFileSDIF(const QString& filename);
    static bool SDIF_hasFrame(const QString& filename, const QString& framesignature);
    #endif

    virtual QString info() const;
    virtual void setVisible(bool shown);
    bool isVisible(){return m_actionShow->isChecked();}
    virtual bool isModified() {return false;}
    virtual void setColor(const QColor& _color);
    virtual void updateIcon();
    virtual double getLastSampleTime() const =0;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);
    virtual FileType* duplicate();

    enum CHECKFILESTATUSMGT {CFSMQUIET, CFSMMESSAGEBOX, CFSMEXCEPTION};
    bool checkFileStatus(CHECKFILESTATUSMGT cfsmgt=CFSMQUIET);
    virtual void setStatus();

    void setFullPath(const QString& fp){fileFullPath = fp;}
    virtual void reload()=0;
};

#endif // FILETYPE_H
