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

#include "ftlabels.h"

#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
using namespace std;

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include <QMenu>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QGraphicsSimpleTextItem>
#include <qmath.h>
#include <qendian.h>

#include "wmainwindow.h"
#include "ui_wmainwindow.h"

void FTLabels::init(){
    m_isedited = false;
    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels times (overwrite the file !)"));
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
    m_actionSaveAs = new QAction("Save as...", this);
    m_actionSaveAs->setStatusTip(tr("Save the labels times in a given file..."));
    connect(m_actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));
}

FTLabels::FTLabels(const QString& _fileName, QObject *parent)
    : FileType(FTLABELS, _fileName, this)
{
    Q_UNUSED(parent)

    init();

    if(fileFullPath.isEmpty()){
       setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");
    }
    else{
        checkFileStatus(CFSMEXCEPTION);
        load();
    }

    WMainWindow::getMW()->ftlabels.push_back(this);
}

FTLabels::FTLabels(const FTLabels& ft)
    : QObject(ft.parent())
    , FileType(FTLABELS, ft.fileFullPath, this)
{
    init();

    starts = ft.starts;
    // ends = fr.ends;
    labels = ft.labels;

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    WMainWindow::getMW()->ftlabels.push_back(this);
}

FileType* FTLabels::duplicate(){
    return new FTLabels(*this);
}

void FTLabels::load() {

#ifdef SUPPORT_SDIF
    // TODO load .lab files

    SDIFEntity readentity;

    try {
        if (!readentity.OpenRead(fileFullPath.toLocal8Bit().constData()) )
            throw QString("SDIF: Cannot open file");
    }
    catch(SDIFBadHeader& e) {
        e.ErrorMessage();
        throw QString("SDIF: bad header");
    }

    readentity.ChangeSelection("/1LAB"); // Select directly the character values

    SDIFFrame frame;
    try{
        while (1) {

            /* reading the next frame of the EntityRead, return the number of
            bytes read, return 0 if the frame is not selected
            and produces an exception if eof is reached */
            if(!readentity.ReadNextFrame(frame))
                continue;

            for (unsigned int i=0 ; i < frame.GetNbMatrix() ; i++) {
                /*take the matrix number "i" and put it in tmpMatrix */
                SDIFMatrix tmpMatrix = frame.GetMatrix(i);

                double t = frame.GetTime();

//                 cout << tmpMatrix.GetNbRows() << " " << tmpMatrix.GetNbCols() << endl;

                if(tmpMatrix.GetNbRows()*tmpMatrix.GetNbCols() == 0) {
//                    cout << "add last marker" << endl;
                    // We reached an ending marker (without char) closing the previous segment
                    // ends.push_back(t);
                }
                else { // There should be a char
                    if(tmpMatrix.GetNbCols()==0)
                        throw QString("label value is missing in the 1LAB frame at time ")+QString::number(t);

                    // Read the label (try both directions)
                    QString str(tmpMatrix.GetUChar(0,0));
                    for(int ci=1; ci<tmpMatrix.GetNbCols(); ci++)
                        str += tmpMatrix.GetUChar(0,ci);
                    for(int ri=1; ri<tmpMatrix.GetNbRows(); ri++)
                        str += tmpMatrix.GetUChar(ri,0);

//                    cout << str.toLatin1().constData() << endl;

                    if(str.size()==0) {
                        // No char given, assume an ending marker closing the previous segment
                        // ends.push_back(t);
                    }
                    else {
                        // if(!starts.empty()) ends.push_back(t);
                        starts.push_back(t);
                        labels.push_back(new QGraphicsSimpleTextItem(str));
                    }
                }
            }

            frame.ClearData();
        }
    }
    catch(SDIFEof& e) {
    }
    catch(SDIFUnDefined& e) {
        e.ErrorMessage();
        throw QString("SDIF: ")+e.what();
    }
    catch(Easdif::SDIFException&e) {
        e.ErrorMessage();
        throw QString("SDIF: ")+e.what();
    }
    catch(std::exception &e) {
        throw QString("SDIF: ")+e.what();
    }
    catch(QString &e) {
        throw QString("SDIF: ")+e;
    }

//    // Add the last ending time, if it was not specified in the SDIF file
//    if(ends.size() < starts.size()){
//        if(starts.size() > 1)
//            ends.push_back(starts.back() + (starts.back() - *(starts.end()-2)));
//        else
//            ends.push_back(starts.back() + 0.01); // fix the duration of the last segment to 10ms
//    }
#endif

    m_lastreadtime = QDateTime::currentDateTime();
    setStatus();
}

void FTLabels::reload() {
//    cout << "FTLabels::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return;

    // Reset everything ...
    starts.clear();
    // ends.clear();
    labels.clear();

    // ... and reload the data from the file
    load();
}

void FTLabels::saveAs() {
    if(labels.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    QString fp = QFileDialog::getSaveFileName(NULL, "Save file as...", fileFullPath);
    if(!fp.isEmpty()){
        try{
            setFullPath(fp);
            save();
        }
        catch(QString &e) {
            QMessageBox::critical(NULL, "Cannot save file!", e);
        }
    }
}

void FTLabels::save() {

#ifdef SUPPORT_SDIF
    // TODO load .lab files

    SdifFileT* filew = SdifFOpen(fileFullPath.toLatin1().constData(), eWriteFile);

    if (!filew)
        throw QString("SDIF: Cannot save the data in the specified file (permission denied?)");

    size_t generalHeaderw = SdifFWriteGeneralHeader(filew);
    size_t asciiChunksw = SdifFWriteAllASCIIChunks(filew);

    for(size_t li=0; li<labels.size(); li++) {
        // cout << labels[li].toLatin1().constData() << ": " << starts[li] << ":" << ends[li] << endl;

        // Prepare the frame
        SDIFFrame frameToWrite;
        /*set the header of the frame*/
        frameToWrite.SetStreamID(0); // TODO Ok ??
        frameToWrite.SetTime(starts[li]);
        frameToWrite.SetSignature("1MRK");

        // Fill the matrix
        SDIFMatrix tmpMatrix("1LAB");
        tmpMatrix.Set(std::string(labels[li]->text().toLatin1().constData()));
        frameToWrite.AddMatrix(tmpMatrix);

        frameToWrite.Write(filew);
        frameToWrite.ClearData();
    }

//    // Write a last empty frame for the last time
//    SDIFFrame frameToWrite;
//    frameToWrite.SetStreamID(0); // TODO Ok ??
//    frameToWrite.SetTime(ends.back());
//    frameToWrite.SetSignature("1MRK");
//    SDIFMatrix tmpMatrix("1LAB", 0, 0);
//    frameToWrite.AddMatrix(tmpMatrix);
//    frameToWrite.Write(filew);

    SdifFClose(filew);

#else
    QMessageBox::warning(NULL, "No way to save!", "There is no way to write down a label file (need to compile DFasma with SDIF support).");
    return;
#endif

    m_lastreadtime = QDateTime::currentDateTime();
    m_isedited = false;
}


QString FTLabels::info() const {
    QString str = FileType::info();
    return str;
}

void FTLabels::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {
    FileType::fillContextMenu(contextmenu, mainwindow);

    contextmenu.setTitle("Labels");

    if(WMainWindow::getMW()->ui->actionEditMode->isChecked()){
        contextmenu.addAction(m_actionSave);
        contextmenu.addAction(m_actionSaveAs);
    }
}

double FTLabels::getLastSampleTime() const {

    if(starts.empty())
        return 0.0;
    else
        return starts.front();

//    if(ends.empty())
//        return 0.0;
//    else
//        return *((ends.end()-1));
}

void FTLabels::remove(int index){
    starts.erase(starts.begin()+index);
    labels.erase(labels.begin()+index);
    m_isedited = true;
    setStatus();
}

template <typename Container>
struct compare_indirect_index
  {
  const Container& container;
  compare_indirect_index( const Container& container ): container( container ) { }
  bool operator () ( size_t lindex, size_t rindex ) const
    {
    return container[ lindex ] < container[ rindex ];
    }
  };

void FTLabels::sort(){
//    cout << "FTLabels::sort" << endl;

//    cout << "unsorted starts: ";
//    for(size_t u=0; u<starts.size(); ++u)
//        cout << starts[u] << " ";
//    cout << endl;

    vector<size_t> indices(starts.size(), 0);
    for(size_t u=0; u<indices.size(); ++u)
        indices[u] = u;

    std::sort(indices.begin(), indices.end(), compare_indirect_index < std::deque<double> >(starts));

    std::deque<QGraphicsSimpleTextItem*> sortedlabels(labels.size());
    std::deque<double> sortedstarts(labels.size());
    for(size_t u=0; u<labels.size(); ++u){
        sortedlabels[u] = labels[indices[u]];
        sortedstarts[u] = starts[indices[u]];
    }

    labels = sortedlabels;
    starts = sortedstarts;

//    cout << "sorted starts: ";
//    for(size_t u=0; u<starts.size(); ++u)
//        cout << starts[u] << " ";
//    cout << endl;

//    cout << "FTLabels::~sort" << endl;
}

FTLabels::~FTLabels() {
}
