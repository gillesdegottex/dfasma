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

#include <deque>
#include <QString>
#include <QColor>
#include <QAction>
#include <QListWidgetItem>
#include <QDateTime>

class WMainWindow;

class FileType : public QListWidgetItem
{
public:
    enum FType {FTUNSET=0, FTSOUND, FTFZERO, FTLABELS}; // Names corresponding to possible classes

private:
    FType m_type;
    static std::deque<QString> s_types_name_and_extensions;
    static struct ClassConstructor{ClassConstructor();} s_class_constructor;

    void constructor_internal(); // Called by each FileType constructor

    QColor m_color;
    bool m_is_editing; // True if the file is currently under edition and the icon is changed accordingly.
    bool m_is_source;  // True if the file has to show the source symbol.

protected:
    bool m_is_edited;   // True if the file has been modified and might need to be saved.
    QDateTime m_modifiedtime;
    QDateTime m_lastreadtime;

    void constructor_external(); // Has to be called by each subclass constructor
                                 // once the file is guaranteed to be included in the application
    virtual void setDrawIcon(QPixmap& pm);

public:
    inline FType getType() const {return m_type;}
    inline static QString getTypeNameAndExtensions(FType type){return s_types_name_and_extensions[type];}
    inline bool is(FType type) const {return m_type==type;}
    enum FileContainer {FCUNSET=0, FCANYSOUND, FCTEXT, FCASCII, FCSDIF}; // File Containers (not format !)
    static QColor GetNextColor();

    FileType(FType _type, const QString& _fileName, QObject *parent, const QColor& _color=GetNextColor());

    QString fileFullPath;   // The file path on storage place
    QString visibleName;    // The name shown in the File List

    const QColor& getColor() const {return m_color;}

    QAction* m_actionShow;

    ~FileType();

    static bool hasFileExtension(const QString& filepath, const QString& ext);
    static bool isFileASCII(const QString& filename);
    static bool isFileTEXT(const QString& filename);
    static FileContainer guessContainer(const QString& filepath);
    #ifdef SUPPORT_SDIF
    static bool isFileSDIF(const QString& filename);
    static bool SDIF_hasFrame(const QString& filename, const QString& framesignature);
    #endif

    virtual QString info() const;
    void setEditing(bool editing);
    void setIsSource(bool issource);
    virtual void setColor(const QColor& _color);
    virtual void updateIcon();
    virtual void setVisible(bool shown);
    bool isVisible(){return m_actionShow->isChecked();}
    virtual bool isModified() {return m_is_edited;}
    virtual double getLastSampleTime() const =0;
    virtual void fillContextMenu(QMenu& contextmenu);
    virtual FileType* duplicate();

    void setFullPath(const QString& fp);
    virtual bool reload()=0;
    enum CHECKFILESTATUSMGT {CFSMQUIET, CFSMMESSAGEBOX, CFSMEXCEPTION};
    bool checkFileStatus(CHECKFILESTATUSMGT cfsmgt=CFSMQUIET);
    virtual void setStatus();
};

#endif // FILETYPE_H
