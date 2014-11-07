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
using namespace std;

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include <QMenu>
#include <qmath.h>
#include <qendian.h>

FTLabels::FTLabels(const QString& _fileName, QObject *parent)
    : FileType(FTLABELS, _fileName, this)
{
    Q_UNUSED(parent)

    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels times (overwrite the file !)"));

    load(_fileName);
}

void FTLabels::load(const QString& _fileName) {

    fileFullPath = _fileName;

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
                    ends.push_back(t);
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
                        ends.push_back(t);
                    }
                    else {
                        if(!starts.empty())
                            ends.push_back(t);
                        starts.push_back(t);
                        labels.push_back(str);
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

    // Add the last ending time, if it was not specified in the SDIF file
    if(ends.size() < starts.size()){
        if(starts.size() > 1)
            ends.push_back(starts.back() + (starts.back() - *(starts.end()-2)));
        else
            ends.push_back(starts.back() + 0.01); // fix the duration of the last segment to 10ms
    }
#endif

    m_lastreadtime = QDateTime::currentDateTime();
}

void FTLabels::reload() {
//    cout << "FTLabels::reload" << endl;

    // Reset everything ...
    starts.clear();
    ends.clear();
    labels.clear();

    // ... and reload the data from the file
    load(fileFullPath);
}

void FTLabels::save() {

#ifdef SUPPORT_SDIF
    // TODO load .lab files

    size_t generalHeaderw;
    size_t asciiChunksw;

    SdifFileT* filew = SdifFOpen ("ecr.sdif", eWriteFile);
    generalHeaderw = SdifFWriteGeneralHeader(filew);
    asciiChunksw = SdifFWriteAllASCIIChunks(filew);

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
        tmpMatrix.Set(std::string(labels[li].toLatin1().constData()));
        frameToWrite.AddMatrix(tmpMatrix);

        frameToWrite.Write(filew);
        frameToWrite.ClearData();
    }

    // Write a last empty frame for the last time
    SDIFFrame frameToWrite;
    frameToWrite.SetStreamID(0); // TODO Ok ??
    frameToWrite.SetTime(ends.back());
    frameToWrite.SetSignature("1MRK");
    SDIFMatrix tmpMatrix("1LAB", 0, 0);
    frameToWrite.AddMatrix(tmpMatrix);
    frameToWrite.Write(filew);

    SdifFClose(filew);

#endif
    m_lastreadtime = QDateTime::currentDateTime();
}


QString FTLabels::info() const {
    QString str = "";
    str += "Loaded at "+m_lastreadtime.toString("HH:mm:ss ddMMM")+"<br/>";
    return str;
}

void FTLabels::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {
    FileType::fillContextMenu(contextmenu, mainwindow);

    contextmenu.setTitle("Labels");

    contextmenu.addAction(m_actionSave);
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
}

double FTLabels::getLastSampleTime() const {
    if(ends.empty())
        return 0.0;
    else
        return *((ends.end()-1));
}

FTLabels::~FTLabels() {
}
