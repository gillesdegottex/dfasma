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
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrum.h"

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include "qthelper.h"

static int sg_colors_loaded = 0;
static deque<QColor> sg_colors;

QColor GetNextColor(){

    // If empty, initialize the colors
    if(sg_colors.empty()){
        if(sg_colors_loaded==1){
            sg_colors.push_back(QColor(64, 64, 64).lighter());
            sg_colors.push_back(QColor(0, 0, 255).lighter());
            sg_colors.push_back(QColor(0, 127, 0).lighter());
            sg_colors.push_back(QColor(255, 0, 0).lighter());
            sg_colors.push_back(QColor(0, 192, 192).lighter());
            sg_colors.push_back(QColor(192, 0, 192).lighter());
            sg_colors.push_back(QColor(192, 192, 0).lighter());
        }
        else if(sg_colors_loaded==2){
            sg_colors.push_back(QColor(64, 64, 64).darker());
            sg_colors.push_back(QColor(0, 0, 255).darker());
            sg_colors.push_back(QColor(0, 127, 0).darker());
            sg_colors.push_back(QColor(255, 0, 0).darker());
            sg_colors.push_back(QColor(0, 192, 192).darker());
            sg_colors.push_back(QColor(192, 0, 192).darker());
            sg_colors.push_back(QColor(192, 192, 0).darker());
        }
        else{
            sg_colors.push_back(QColor(64, 64, 64));
            sg_colors.push_back(QColor(0, 0, 255));
            sg_colors.push_back(QColor(0, 127, 0));
            sg_colors.push_back(QColor(255, 0, 0));
            sg_colors.push_back(QColor(0, 192, 192));
            sg_colors.push_back(QColor(192, 0, 192));
            sg_colors.push_back(QColor(192, 192, 0));
        }
        sg_colors_loaded++;
    }

    QColor color = sg_colors.front();

    sg_colors.pop_front();

//    cout << "1) R:" << color.redF() << " G:" << color.greenF() << " B:" << color.blueF() << " m:" << (color.redF()+color.greenF()+color.blueF())/3.0 << " L:" << color.lightnessF() << " V:" << color.valueF() << endl;

    return color;
}

std::deque<QString> FileType::m_typestrings;

void FileType::init(){
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

FileType::FileType(FType _type, const QString& _fileName, QObject * parent)
    : type(_type)
    , fileFullPath(_fileName)
    , color(GetNextColor())
{
    init();
//    cout << "FileType::FileType: " << _fileName.toLocal8Bit().constData() << endl;

    m_actionShow = new QAction("Show", parent);
    m_actionShow->setStatusTip("Show this file in the views");
    m_actionShow->setCheckable(true);
    m_actionShow->setChecked(true);
    m_actionShow->setShortcut(gMW->ui->actionSelectedFilesToggleShown->shortcut());

    updateIcon();

    // Set properties common to all files
    QFileInfo fileInfo(fileFullPath);
    setText(fileInfo.fileName());
    setToolTip(fileInfo.absoluteFilePath());

//    QIODevice::open(QIODevice::ReadOnly);
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
    pm.fill(color);
    if(!isVisible()){
        // If Invisible, whiten the left half.
        QPainter p(&pm);
        p.setBrush(QColor(255,255,255));
        p.setPen(QColor(255,255,255));
        QRect rect(0, 0, 16, 32);
        p.drawRect(rect);
    }
}

void FileType::setColor(const QColor& _color) {
    color = _color;
    updateIcon();
}

void FileType::updateIcon(){
    QPixmap pm(32,32);
    setDrawIcon(pm);
    setIcon(QIcon(pm));
}

void FileType::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {
    contextmenu.addAction(m_actionShow);
    contextmenu.addAction(mainwindow->ui->actionSelectedFilesReload);
    contextmenu.addAction(mainwindow->ui->actionSelectedFilesDuplicate);
    QColorDialog* colordialog = new QColorDialog(&contextmenu); // TODO delete this !!!
    QObject::connect(colordialog, SIGNAL(colorSelected(const QColor &)), gMW, SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), gMW, SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), gMW->m_gvSpectrum, SLOT(allSoundsChanged()));

    // Add the available Matlab colors to the custom colors
    int ci = 0;
    for(std::deque<QColor>::iterator it=sg_colors.begin(); it!=sg_colors.end(); it++,ci++)
        QColorDialog::setCustomColor(ci, (*it));

    contextmenu.addAction("Color...", colordialog, SLOT(exec()));
    contextmenu.addAction(mainwindow->ui->actionSelectedFilesClose);
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

    QString liststr = fileInfo.fileName();
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
    sg_colors.push_front(color);
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
namespace SDIFAccess {
    // TODO Clean this
    // global list for all easdif file pointers
    typedef std::list<std::pair<Easdif::SDIFEntity *,Easdif::SDIFEntity::const_iterator> > EASDList;
    EASDList pList;

    // validate file pointer
    bool CheckList(Easdif::SDIFEntity *p, EASDList::iterator&it) {
      it = pList.begin();
      EASDList::iterator ite = pList.end();
      while(it!=ite){
        if(p==it->first){
          return true;
        }
        ++it;
      }
      return false;
    }
};

// mexAtExit cleanup function
void FileType::cleanupAndEEnd() {
  SDIFAccess::EASDList::iterator it = SDIFAccess::pList.begin();
  SDIFAccess::EASDList::iterator ite = SDIFAccess::pList.end();
  while(it!=ite){
    delete it->first;
    it->first = 0;
    ++it;
  }
  SDIFAccess::pList.clear();
}

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

    // get a free slot in sdif file pointer list
    Easdif::SDIFEntity *p = 0;
    SDIFAccess::EASDList::iterator it = SDIFAccess::pList.begin();
    SDIFAccess::EASDList::iterator ite = SDIFAccess::pList.end();
    while(it!=ite){
      if(! it->first->IsOpen()){
        p = it->first;
        break;
      }
      ++it;
    }
    if(it==ite){
      p = new Easdif::SDIFEntity();
      if(p){
        SDIFAccess::pList.push_back(std::pair<Easdif::SDIFEntity*,Easdif::SDIFEntity::const_iterator>(p,Easdif::SDIFEntity::const_iterator()));
        it = --SDIFAccess::pList.end();
      }
      else
        throw QString("Failed allocating Easdif file");
    }

    // open the file
    if(!it->first->OpenRead(filename.toLocal8Bit().constData()))
        return false;

    // enable frame dir to be able to work with selections
    it->first->EnableFrameDir();
    it->second = it->first->begin();

    bool found = false;

    // The header is not always filled properly.
    // Thus, parse the file until the given frame is found
    // create frame directory output
    {
        Easdif::SDIFEntity::const_iterator it = p->begin();
        Easdif::SDIFEntity::const_iterator ite = p->end();
        // establish directory
        while(it !=ite)
            ++it;
        p->Rewind();
        // const Easdif::Directory & dir= p->GetDirectory();
        // dir.size()

        it = p->begin();
        for(int ii=0; !found && it!=ite; ++it,++ii) {
            char* sigstr = SdifSignatureToString(it.GetLoc().LocSignature());
            // cout << sigstr << endl;
            if (QString(sigstr).compare(framesignature)==0)
                found = true;
        }
    }

    // Close the file
    SDIFAccess::EASDList::iterator itl;
    // validate pointer
    if(SDIFAccess::CheckList(p,itl) && p->IsOpen())
        p->Close();

    return found;
}
#endif
