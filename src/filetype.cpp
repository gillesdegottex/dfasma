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
#include <deque>
using namespace std;

#include <QMenu>
#include <QFileInfo>
#include <QColorDialog>
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvamplitudespectrum.h"

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

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

FileType::FileType(FILETYPE _type, const QString& _fileName, QObject * parent)
    : type(_type)
    , fileFullPath(_fileName)
    , color(GetNextColor())
{
//    cout << "FileType::FileType: " << _fileName.toLocal8Bit().constData() << endl;

    QPixmap pm(32,32);
    pm.fill(color);
    setIcon(QIcon(pm));

    // Set properties common to all files
    QFileInfo fileInfo(fileFullPath);
    setText(fileInfo.fileName());
    setToolTip(fileInfo.absoluteFilePath());

    m_actionShow = new QAction("Show", parent);
    m_actionShow->setStatusTip("Show the sound in the views");
//    m_actionShow->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_H));
    m_actionShow->setCheckable(true);
    m_actionShow->setChecked(true);

//    QIODevice::open(QIODevice::ReadOnly);
}

void FileType::setColor(const QColor& _color) {
    color = _color;
    QPixmap pm(32,32);
    pm.fill(color);
    setIcon(QIcon(pm));
}

void FileType::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {
    contextmenu.addAction(m_actionShow);
    mainwindow->connect(m_actionShow, SIGNAL(toggled(bool)), mainwindow, SLOT(setSoundShown(bool)));
    QColorDialog* colordialog = new QColorDialog(&contextmenu);
    QObject::connect(colordialog, SIGNAL(colorSelected(const QColor &)), WMainWindow::getMW(), SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), WMainWindow::getMW(), SLOT(colorSelected(const QColor &)));
//    QObject::connect(colordialog, SIGNAL(currentColorChanged(const QColor &)), WMainWindow::getMW()->m_gvSpectrum, SLOT(soundsChanged()));

    // Add the available Matlab colors to the custom colors
    int ci = 0;
    for(std::deque<QColor>::iterator it=sg_colors.begin(); it!=sg_colors.end(); it++,ci++)
        QColorDialog::setCustomColor(ci, (*it));

    contextmenu.addAction("Color ...", colordialog, SLOT(exec()));
    contextmenu.addAction(mainwindow->ui->actionCloseFile);
    contextmenu.addSeparator();
}

void FileType::setModifiedState(bool modified) {
    QFileInfo fileInfo(fileFullPath);
    if(modified) {
        setText("*"+fileInfo.fileName());
        setToolTip("(modified) "+fileInfo.absoluteFilePath());
    }
    else {
        setText(fileInfo.fileName());
        setToolTip(fileInfo.absoluteFilePath());
    }
}

FileType::~FileType() {
//    delete m_actionShow; // Make the closure crash (??)

    sg_colors.push_front(color);
}


#ifdef SUPPORT_SDIF
// Based on EASDIF_SDIF/matlab/FSDIF_read_handler.cpp: Copyright (c) 2008 by IRCAM

// global list for all easdif file pointers
typedef std::list<std::pair<Easdif::SDIFEntity *,Easdif::SDIFEntity::const_iterator> > EASDList;
EASDList pList;

// mexAtExit cleanup function
void cleanup() {
  EASDList::iterator it = pList.begin();
  EASDList::iterator ite = pList.end();
  while(it!=ite){
    delete it->first;
    it->first = 0;
    ++it;
  }
  pList.clear();
}
void cleanupAndEEnd() {
  cleanup();
}

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


bool FileType::SDIF_hasFrame(const QString& filename, const QString& framesignature) {

    // get a free slot in sdif file pointer list
    Easdif::SDIFEntity *p = 0;
    EASDList::iterator it = pList.begin();
    EASDList::iterator ite = pList.end();
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
        pList.push_back(std::pair<Easdif::SDIFEntity*,Easdif::SDIFEntity::const_iterator>(p,Easdif::SDIFEntity::const_iterator()));
        it = --pList.end();
      }
      else
        throw QString("Failed allocating Easdif file");
    }

    // open the file
    if(!it->first->OpenRead(filename.toLocal8Bit().constData()))
        throw QString("Cannot open file: ")+filename;

    // enable frame dir to be able to work with selections
    it->first->EnableFrameDir();
    it->second = it->first->begin();


/*    // NVTs (Complementary information provided in the header)
    {
        int numNVTs =  p->GetNbNVT();

        int sumNVTEntries = 0;
        for(int ii=0;ii!=numNVTs;++ii){
          sumNVTEntries += p->GetNVT(ii).size();
        }

        int offvalue = std::max(0,sumNVTEntries + numNVTs - 1);
        int inval=0;
        for(int invt=0; invt != numNVTs ; ++invt){
            Easdif::SDIFNameValueTable &currNvt = p->GetNVT(invt);
            std::map<std::string,std::string>::const_iterator it = currNvt.begin();
            std::map<std::string,std::string>::const_iterator ite = currNvt.end();
            while(it != ite){
                cout << (*it).first.c_str() << ":" << (*it).second.c_str() << endl;
                ++it;
                ++inval;
            }
        }
    }*/

/*  //      IDS (e.g. streams information)
  {
    int numstreamid;
    SdifStringT *string;
    char idnumasstring[30];
    SdifFileT *input = p->GetFile();

    string = SdifStringNew();

    if((numstreamid=SdifStreamIDTableGetNbData(input->StreamIDsTable)) > 0){
      unsigned int iID;
      SdifHashNT* pID;
      SdifStreamIDT *sd;

      for(iID=0; iID<input->StreamIDsTable->SIDHT->HashSize; iID++)
        for (pID = input->StreamIDsTable->SIDHT->Table[iID]; pID; pID = pID->Next) {
          SdifStringAppend(string,"IDS ");
          sd = ((SdifStreamIDT * )(pID->Data));
          sprintf(idnumasstring,"%d ",sd->NumID);
          SdifStringAppend(string,idnumasstring);
          SdifStringAppend(string,sd->Source);
          SdifStringAppend(string,":");
          SdifStringAppend(string,sd->TreeWay);
          SdifStringAppend(string,"\n");
        }
    }

    if (string->SizeW)
        cout << "IDS: " << string->str << endl;

    SdifStringFree(string);
  }*/

    /*//TYP (non-standard type definitions)
    {
        // matrix types
        {
          std::vector<Easdif::MatrixType> matrixtypes;
          p->GetTypes(matrixtypes);

          for(unsigned int ii=0;ii!=matrixtypes.size();++ii){
            Easdif::MatrixType &mat = matrixtypes[ii];

            char* sigstr = SdifSignatureToString(mat.GetSignature());
            cout << "Matrix: " << sigstr << endl;

            for(unsigned int ic=0;ic!=mat.mvColumnNames.size();++ic){
                cout << "Matrix: " << mat.mvColumnNames[ic].c_str() << endl;
            }
          }
        }

        // frame types
        {

          std::vector<Easdif::FrameType> frametypes;
          p->GetTypes(frametypes);

          for(unsigned int ii=0;ii!=frametypes.size();++ii){
            Easdif::FrameType &frm = frametypes[ii];

            char* sigstr = SdifSignatureToString(frm.GetSignature());
            cout << "Frame: " << sigstr << endl;

    //        int off   = frm.mvMatrixNames.size();
    //        pd= reinterpret_cast<double*>( mxGetData(ftypmsig));
    //        for(unsigned int im=0;im!=frm.mvMatrixTypes.size();++im){
    //          sigstr = SdifSignatureToString(frm.mvMatrixTypes[im].GetSignature());
    //          *pd         = sigstr[0];
    //          *(pd+off)   = sigstr[1];
    //          *(pd+2*off) = sigstr[2];
    //          *(pd+3*off) = sigstr[3];
    //          ++pd;
    //          mxSetCell(ftypmnam,im,mxCreateString(frm.mvMatrixNames[im].c_str()));
    //        }
          }
        }
    }*/

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
    EASDList::iterator itl;
    // validate pointer
    if(CheckList(p,itl) && p->IsOpen())
        p->Close();

    return found;
}
#endif
