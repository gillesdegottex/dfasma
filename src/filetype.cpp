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

#include "filetype.h"

#include <iostream>
#include <fstream>
#include <deque>
using namespace std;

#include <QMenu>
#include <QFileInfo>
#include <QColorDialog>
#include <QMessageBox>
#include <QSvgRenderer>
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrum.h"

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include "qthelper.h"

static deque<QColor> s_colors;
static int s_colors_loaded = 0;

QColor FileType::GetNextColor(){

    // If empty, initialize the colors
    if(s_colors.empty()){
        if(s_colors_loaded==1){
            s_colors.push_back(QColor(64, 64, 64).lighter());
            s_colors.push_back(QColor(0, 0, 255).lighter());
            s_colors.push_back(QColor(0, 127, 0).lighter());
            s_colors.push_back(QColor(255, 0, 0).lighter());
            s_colors.push_back(QColor(0, 192, 192).lighter());
            s_colors.push_back(QColor(192, 0, 192).lighter());
            s_colors.push_back(QColor(192, 192, 0).lighter());
        }
        else if(s_colors_loaded==2){
            s_colors.push_back(QColor(64, 64, 64).darker());
            s_colors.push_back(QColor(0, 0, 255).darker());
            s_colors.push_back(QColor(0, 127, 0).darker());
            s_colors.push_back(QColor(255, 0, 0).darker());
            s_colors.push_back(QColor(0, 192, 192).darker());
            s_colors.push_back(QColor(192, 0, 192).darker());
            s_colors.push_back(QColor(192, 192, 0).darker());
        }
        else{
            s_colors.push_back(QColor(64, 64, 64));
            s_colors.push_back(QColor(0, 0, 255));
            s_colors.push_back(QColor(0, 127, 0));
            s_colors.push_back(QColor(255, 0, 0));
            s_colors.push_back(QColor(0, 192, 192));
            s_colors.push_back(QColor(192, 0, 192));
            s_colors.push_back(QColor(192, 192, 0));
        }
        s_colors_loaded++;
    }

    QColor color = s_colors.front();

    s_colors.pop_front();

//    cout << "1) R:" << color.redF() << " G:" << color.greenF() << " B:" << color.blueF() << " m:" << (color.redF()+color.greenF()+color.blueF())/3.0 << " L:" << color.lightnessF() << " V:" << color.valueF() << endl;

    return color;
}

std::deque<QString> FileType::s_types_name_and_extensions;
FileType::ClassConstructor::ClassConstructor(){
    // Map FileTypes with corresponding strings
    // Attention ! It has to correspond to FType definition's order.
    if(FileType::s_types_name_and_extensions.empty()){
        FileType::s_types_name_and_extensions.push_back("All files (*)");
        FileType::s_types_name_and_extensions.push_back("Sound (*.wav *.aiff *.pcm *.snd *.flac *.ogg)");
        FileType::s_types_name_and_extensions.push_back("F0 (*.f0.bpf *.bpf *.sdif)");
        FileType::s_types_name_and_extensions.push_back("Label (*.bpf *.lab *.sdif)");
    }
}
FileType::ClassConstructor FileType::s_class_constructor;

void FileType::constructor_common(){
    m_is_editing = false;
    m_is_edited = false;
    m_is_source = false;

    gFL->m_present_files.insert(make_pair(this,true));
}

FileType::FileContainer FileType::guessContainer(const QString& filepath){
    // TODO Could make a ligther one based on the file extension, instead of opening the file

    int nchan = FTSound::getNumberOfChannels(filepath);
    if(nchan>0)
        return FCANYSOUND;
    #ifdef SUPPORT_SDIF
    else if(FileType::isFileSDIF(filepath))
        return FCSDIF;
    #endif
    else if(FileType::isFileTEXT(filepath))// This detection is not 100% accurate
        return FCTEXT;
//    else if(FileType::isFileASCII(filepath))// This detection is not 100% accurate
//        return FCASCII;
    #ifndef SUPPORT_SDIF
    else if(FileType::hasFileExtension(filepath, ".sdif"))
        throw QString("Support of SDIF files not compiled in this distribution of DFasma.");
    #endif
    else
        throw QString("The container(format) of this file is not managed by DFasma.");

    return FileType::FCUNSET;
}

FileType::FileType(FType _type, const QString& _fileName, QObject * parent, const QColor& _color)
    : m_type(_type)
    , m_color(_color)
    , fileFullPath(_fileName)
{
    FileType::constructor_common();
//    cout << "FileType::FileType: " << _fileName.toLocal8Bit().constData() << endl;

    m_actionShow = new QAction("Show", parent);
    m_actionShow->setStatusTip("Show this file in the views");
    m_actionShow->setCheckable(true);
    m_actionShow->setChecked(true);
    m_actionShow->setShortcut(gMW->ui->actionSelectedFilesToggleShown->shortcut());

    updateIcon();
    setFullPath(fileFullPath);
    setFlags(flags()|Qt::ItemIsEditable);

//    QIODevice::open(QIODevice::ReadOnly);
}
void FileType::setFullPath(const QString& fp){
    fileFullPath = fp;
    // Set properties common to all files
    QFileInfo fileInfo(fileFullPath);
    visibleName = fileInfo.fileName();
    setText(visibleName);
    setToolTip(fileInfo.absoluteFilePath());
}

QString FileType::info() const {

    QString str;

    QString datestr = m_lastreadtime.toString("HH:mm:ss ddMMM");
    if(m_modifiedtime>m_lastreadtime) datestr = "<b>"+datestr+"</b>";
    if(m_lastreadtime!=QDateTime())
        str += "Loaded at "+datestr+"<br/>";

    if(m_lastreadtime==QDateTime())
        str += "<b>Not saved yet</b>";
    else if(m_modifiedtime==QDateTime())
        str += "<b>Currently inaccessible</b>";
    else{
        datestr = m_modifiedtime.toString("HH:mm:ss ddMMM");
        if(m_modifiedtime>m_lastreadtime) datestr = "<b>"+datestr+"</b>";
        str += "Last file modification at "+datestr;
    }

    str += "<hr/>";

    return str;
}

bool FileType::checkFileStatus(CHECKFILESTATUSMGT cfsmgt){
    QFileInfo fileInfo(fileFullPath);
    if(!fileInfo.exists()){
        if(cfsmgt==CFSMEXCEPTION)
            throw QString("The file: ")+fileFullPath+" doesn't seem to exist.";
        else if(cfsmgt==CFSMMESSAGEBOX)
            QMessageBox::critical(NULL, "Cannot load file", QString("The file: ")+fileFullPath+" doesn't seem to exist.");

        m_modifiedtime = QDateTime();
        setStatus();
        return false;
    }
    else{
        QFileInfo fi(fileFullPath);
        m_modifiedtime = fi.lastModified();
        setStatus();
    }

    return true;
}

void FileType::setDrawIcon(QPixmap& pm){
    pm.fill(m_color);
    if(!isVisible()){
        // If Invisible, whiten the left half.
        QPainter p(&pm);
        p.setBrush(QColor(255,255,255));
        p.setPen(QColor(255,255,255));
        QRect rect(0, 0, 16, 32);
        p.drawRect(rect);
    }
    if(m_is_editing){
        QSvgRenderer renderer(QString(":/icons/edit2_white.svg"));
        QPainter p(&pm);
        renderer.render(&p);
    }    
    if(m_is_source){
        QSvgRenderer renderer(QString(":/icons/source.svg"));
        QPainter p(&pm);
        renderer.render(&p);
    }
}

void FileType::setColor(const QColor& color) {
    m_color = color;
    updateIcon();
}

void FileType::updateIcon(){
    QPixmap pm(32,32);
    setDrawIcon(pm);
    setIcon(QIcon(pm));
}

void FileType::setEditing(bool editing){
    if(m_is_editing!=editing){
        m_is_editing=editing;
        updateIcon();
    }
}

void FileType::setIsSource(bool issource){
    if(m_is_source!=issource){
        m_is_source=issource;
        updateIcon();
    }
}


void FileType::fillContextMenu(QMenu& contextmenu) {
    contextmenu.addAction(m_actionShow);
    contextmenu.addAction(gMW->ui->actionSelectedFilesReload);
    contextmenu.addAction(gMW->ui->actionSelectedFilesDuplicate);
    QColorDialog* colordialog = new QColorDialog(&contextmenu); // TODO delete this !!!
    QObject::connect(colordialog, SIGNAL(colorSelected(const QColor &)), gFL, SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), gFL, SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), gMW->m_gvSpectrum, SLOT(allSoundsChanged()));

    // Add the available Matlab colors to the custom colors
    int ci = 0;
    for(std::deque<QColor>::iterator it=s_colors.begin(); it!=s_colors.end(); it++,ci++)
        QColorDialog::setCustomColor(ci, (*it));

    contextmenu.addAction("Color...", colordialog, SLOT(exec()));
    contextmenu.addAction(gMW->ui->actionSelectedFilesClose);
    contextmenu.addSeparator();
}

FileType* FileType::duplicate(){
    return NULL;
}

void FileType::setVisible(bool shown) {
//    COUTD << "FileType::setVisible" << endl;
    m_actionShow->setChecked(shown);
    if(shown) {
        setForeground(QGuiApplication::palette().text());
    }
    else {
        // TODO add diagonal gray stripes in the icon
        setForeground(QGuiApplication::palette().brush(QPalette::Disabled, QPalette::WindowText));
    }
    updateIcon();
    setStatus();
//    COUTD << "FileType::~setVisible" << endl;
}

void FileType::setStatus() {
    QFileInfo fileInfo(fileFullPath);

    QString liststr = visibleName;
    QString tooltipstr = fileInfo.absoluteFilePath();

//    cout << m_lastreadtime.toString().toLatin1().constData() << ":" << m_modifiedtime.toString().toLatin1().constData() << endl;
    if(m_lastreadtime<m_modifiedtime || m_modifiedtime==QDateTime()){
        liststr = '!'+liststr;
        if(m_lastreadtime==QDateTime())
            tooltipstr = "(unread)"+tooltipstr;
        else if(m_modifiedtime==QDateTime())
            tooltipstr = "(Inaccessible)"+tooltipstr;
        else if(m_lastreadtime<m_modifiedtime)
            tooltipstr = "(external modification)"+tooltipstr;
    }
    if(isModified()) {
        liststr = '*'+liststr;
        tooltipstr = "(modified)"+tooltipstr;
        // TODO Set plain icon in the square ?
    }
    if(!m_actionShow->isChecked()) {
        // liststr = '('+liststr+')';
        tooltipstr = "(hidden)"+tooltipstr;
        // TODO Add diagonal stripes in the icon
    }

    setText(liststr);
    setToolTip(tooltipstr);
}

FileType::~FileType() {
    gFL->m_present_files.erase(this);

    s_colors.push_front(m_color);
}


bool FileType::hasFileExtension(const QString& filepath, const QString& ext){
    int ret = filepath.lastIndexOf(ext, -1, Qt::CaseInsensitive);

    return ret!=-1 && (ret==filepath.length()-ext.length());
}

// This function TRY TO GUESS if the file is ASCII or not
// It may say it is ASCII whereas it is a non-ASCII text file (e.g. UTF) or a binary file (possible false positive)
// If the file is truely a simple ASCII (non-UTF), the return value is always correct (no possible false negative)
// For this reason, is container type has to be the last one to be tested.
bool FileType::isFileASCII(const QString& filename) {
//    COUTD << "FileType::isFileASCII " << filename.toLatin1().constData() << endl;

    // ASCII chars should be 0< c <=127

    int c;
    // COUTD << "EOF='" << EOF << "'" << endl;
    std::ifstream a(filename.toLatin1().constData());
    int n = 0;
    // Assume the first Ko is sufficient for testing ASCII content
    while((c = a.get()) != EOF && n<1000){
//         COUTD << "'" << c << "'" << endl;
        if(c==0 || c>127)
            return false;
    }

    return true;
}

// This function TRY TO GUESS if the file is simple  text (ascii or utf or whatever text) or not (e.g. binary)
// It may say it is text whereas it is a binary file (possible false positive)
// If the file is truely a simple text file, the return value is always correct (no possible false negative)
// For this reason, is container type has to be the last one to be tested.
bool FileType::isFileTEXT(const QString& filename) {
//    COUTD << "FileType::isFileText " << filename.toLatin1().constData() << endl;

    // ASCII chars should be 0< c <=127

    int c;
    // COUTD << "EOF='" << EOF << "'" << endl;
    std::ifstream a(filename.toLatin1().constData());
    int n = 0;
    // Assume the first Ko is sufficient for testing ASCII content
    while((c = a.get()) != EOF && n<1000){
//         COUTD << "'" << c << "'" << endl;
        if(c==0)
            return false;
    }

    return true;
}

#ifdef SUPPORT_SDIF

bool FileType::isFileSDIF(const QString& filename) {
    try{
        SDIFEntity readentity;
        if (readentity.OpenRead(filename.toLocal8Bit().constData())){
            readentity.Close();
            return true;
        }
    }
    catch(SDIFBadHeader& e) {
    }
    catch(SDIFEof& e) {
    }

    return false;
}

bool FileType::SDIF_hasFrame(const QString& filename, const QString& framesignature) {

    SDIFEntity readentity;
    // open the file
    if(!readentity.OpenRead(filename.toLocal8Bit().constData()))
        return false;

    // enable frame dir to be able to work with selections
    readentity.EnableFrameDir();

    bool found = false;

    // The header is not always filled properly.
    // Thus, parse the file until the given frame is found
    // create frame directory output
    {
        Easdif::SDIFEntity::const_iterator it = readentity.begin();
        Easdif::SDIFEntity::const_iterator ite = readentity.end();
        // establish directory
        while(it !=ite)
            ++it;
        readentity.Rewind();
        // const Easdif::Directory & dir= readentity.GetDirectory();
        // dir.size()

        it = readentity.begin();
        for(int ii=0; !found && it!=ite; ++it,++ii) {
            char* sigstr = SdifSignatureToString(it.GetLoc().LocSignature());
            // cout << sigstr << endl;
            if (QString(sigstr).compare(framesignature)==0)
                found = true;
        }
    }

    return found;
}
#endif
