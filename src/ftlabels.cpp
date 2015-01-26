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
#include <fstream>
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
#include "gvwaveform.h"
#include "gvspectrogram.h"

void FTLabels::init(){
    m_isedited = false;
    m_fileformat = FFNotSpecified;
    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels times (overwrite the file !)"));
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
    m_actionSaveAs = new QAction("Save as...", this);
    m_actionSaveAs->setStatusTip(tr("Save the labels times in a given file..."));
    connect(m_actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));

    connect(m_actionReload, SIGNAL(triggered()), this, SLOT(reload()));
}

// Construct an empty label object
FTLabels::FTLabels(QObject *parent)
    : FileType(FTLABELS, "", this)
{
    Q_UNUSED(parent);

    init();

    setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");

    WMainWindow::getMW()->ftlabels.push_back(this);
}

// Construct from an existing file name
FTLabels::FTLabels(const QString& _fileName, QObject *parent)
    : FileType(FTLABELS, _fileName, this)
{
    Q_UNUSED(parent);

    if(fileFullPath.isEmpty())
        throw QString("This ctor use an existing file. Don't use this ctor for an empty label object.");

    init();

    checkFileStatus(CFSMEXCEPTION);
    load();

    WMainWindow::getMW()->ftlabels.push_back(this);
}

// Copy constructor
FTLabels::FTLabels(const FTLabels& ft)
    : QObject(ft.parent())
    , FileType(FTLABELS, ft.fileFullPath, this)
{
    init();

    starts = ft.starts;
    waveform_labels = ft.waveform_labels; // TODO Need to duplicate the items, no ?
    spectrogram_labels = ft.spectrogram_labels; // TODO Need to duplicate the items, no ?
    waveform_lines = ft.waveform_lines; // TODO Need to duplicate the items, no ?
    spectrogram_lines = ft.spectrogram_lines; // TODO Need to duplicate the items, no ?

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    WMainWindow::getMW()->ftlabels.push_back(this);
}

FileType* FTLabels::duplicate(){
    return new FTLabels(*this);
}

void FTLabels::load() {

    clear(); // First, ensure there is no leftover

    if(m_fileformat==FFNotSpecified)
        m_fileformat = FFAutoDetect;

    #ifdef SUPPORT_SDIF
        if(m_fileformat==FFAutoDetect) {
            SDIFEntity readentity;
            if (readentity.OpenRead(fileFullPath.toLocal8Bit().constData())){
                m_fileformat = FFSDIF;
                readentity.Close();
            }
        }
    #endif

//    if(m_fileformat==FFAutoDetect) {
//        // TODO Check if text file or binary
//    }

    if(m_fileformat==FFAutoDetect)
        throw QString("Cannot detect the file format of this label file");

    if(m_fileformat==FFAsciiTimeText){
//        ofstream fout(fileFullPath.toLatin1().constData());
//        for(size_t li=0; li<starts.size(); li++) {
//            fout << starts[li] << " " << waveform_labels[li]->text().toLatin1().constData() << endl;
//        }
    }
    else if(m_fileformat==FFAsciiSegments){
//        ofstream fout(fileFullPath.toLatin1().constData());
//        for(size_t li=0; li<starts.size(); li++) {
//            fout << starts[li] << " ";
//            double last = starts[li] + 1.0/WMainWindow::getMW()->getFs();
//            if(li<starts.size()-1)
//                last = starts[li+1];
//            else {
//                if(WMainWindow::getMW()->ftsnds.size()>0)
//                    last = WMainWindow::getMW()->getCurrentFTSound(true)->getLastSampleTime();
//            }
//            fout << last << " ";
//            fout << waveform_labels[li]->text().toLatin1().constData() << endl;
//        }
    }
    else if(m_fileformat==FFAsciiSegmentsHTK){
//        ofstream fout(fileFullPath.toLatin1().constData());
//        // TODO check the format
//        for(size_t li=0; li<starts.size(); li++) {
//            fout << int(0.5+WMainWindow::getMW()->getFs()*starts[li]) << " ";
//            double last = starts[li] + 1.0/WMainWindow::getMW()->getFs();
//            if(li<starts.size()-1)
//                last = starts[li+1];
//            else {
//                if(WMainWindow::getMW()->ftsnds.size()>0)
//                    last = WMainWindow::getMW()->getCurrentFTSound(true)->getLastSampleTime();
//            }
//            fout << int(0.5+WMainWindow::getMW()->getFs()*last) << " ";
//            fout << waveform_labels[li]->text().toLatin1().constData() << endl;
//        }
    }
    else if(m_fileformat==FFSDIF){
        #ifdef SUPPORT_SDIF

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

                        double position = frame.GetTime();

        //                 cout << tmpMatrix.GetNbRows() << " " << tmpMatrix.GetNbCols() << endl;

                        if(tmpMatrix.GetNbRows()*tmpMatrix.GetNbCols() == 0) {
        //                    cout << "add last marker" << endl;
                            // We reached an ending marker (without char) closing the previous segment
                            // ends.push_back(t);
                        }
                        else { // There should be a char
                            if(tmpMatrix.GetNbCols()==0)
                                throw QString("label value is missing in the 1LAB frame at time ")+QString::number(position);

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
                            else
                                addLabel(position, str);
                        }
                    }

                    frame.ClearData();
                }
            }
            catch(SDIFEof& e) {
                readentity.Close();
            }
            catch(SDIFUnDefined& e) {
                e.ErrorMessage();
                readentity.Close();
                throw QString("SDIF: ")+e.what();
            }
            catch(Easdif::SDIFException&e) {
                e.ErrorMessage();
                readentity.Close();
                throw QString("SDIF: ")+e.what();
            }
            catch(std::exception &e) {
                readentity.Close();
                throw QString("SDIF: ")+e.what();
            }
            catch(QString &e) {
                readentity.Close();
                throw QString("SDIF: ")+e;
            }

            //    // Add the last ending time, if it was not specified in the SDIF file
            //    if(ends.size() < starts.size()){
            //        if(starts.size() > 1)
            //            ends.push_back(starts.back() + (starts.back() - *(starts.end()-2)));
            //        else
            //            ends.push_back(starts.back() + 0.01); // fix the duration of the last segment to 10ms
            //    }
        #else
            throw QString("SDIF file support is not compiled in this distribution of DFasma.");
        #endif
    }
    else{
        throw QString("File format not recognized for loading this label file.");
    }

    m_lastreadtime = QDateTime::currentDateTime();
    m_isedited = false;
    setStatus();
}

void FTLabels::clear() {
    if(getNbLabels()>0) {
        starts.clear();
        for(size_t li=0; li<waveform_labels.size(); li++)
            delete waveform_labels[li];
        waveform_labels.clear();
        for(size_t li=0; li<spectrogram_labels.size(); li++)
            delete spectrogram_labels[li];
        for(size_t li=0; li<waveform_lines.size(); li++)
            delete waveform_lines[li];
        waveform_lines.clear();
        for(size_t li=0; li<spectrogram_lines.size(); li++)
            delete spectrogram_lines[li];
        spectrogram_lines.clear();

        m_isedited = true;
        setStatus();
    }
}

void FTLabels::reload() {
    // cout << "FTLabels::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return;

    // Reload the data from the file
    load();
}

void FTLabels::saveAs() {
    if(starts.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    // TODO Ask the desired format in the qfiledialog

    QString fp = QFileDialog::getSaveFileName(NULL, "Save file as...", fileFullPath);
    if(!fp.isEmpty()){
        try{
            setFullPath(fp);
            if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
                m_fileformat = FFAsciiTimeText;
            save();
        }
        catch(QString &e) {
            QMessageBox::critical(NULL, "Cannot save under this file path.", e);
        }
    }
}

void FTLabels::save() {

    sort(); // In the file, we want the label' times in ascending order

    if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
        m_fileformat = FFAsciiTimeText;

    if(m_fileformat==FFAsciiTimeText){
        ofstream fout(fileFullPath.toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++) {
            fout << starts[li] << " " << waveform_labels[li]->text().toLatin1().constData() << endl;
        }
    }
    else if(m_fileformat==FFAsciiSegments){
        ofstream fout(fileFullPath.toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++) {
            fout << starts[li] << " ";
            double last = starts[li] + 1.0/WMainWindow::getMW()->getFs();
            if(li<starts.size()-1)
                last = starts[li+1];
            else {
                if(WMainWindow::getMW()->ftsnds.size()>0)
                    last = WMainWindow::getMW()->getCurrentFTSound(true)->getLastSampleTime();
            }
            fout << last << " ";
            fout << waveform_labels[li]->text().toLatin1().constData() << endl;
        }
    }
    else if(m_fileformat==FFAsciiSegmentsHTK){
        ofstream fout(fileFullPath.toLatin1().constData());
        // TODO check the format
        for(size_t li=0; li<starts.size(); li++) {
            fout << int(0.5+WMainWindow::getMW()->getFs()*starts[li]) << " ";
            double last = starts[li] + 1.0/WMainWindow::getMW()->getFs();
            if(li<starts.size()-1)
                last = starts[li+1];
            else {
                if(WMainWindow::getMW()->ftsnds.size()>0)
                    last = WMainWindow::getMW()->getCurrentFTSound(true)->getLastSampleTime();
            }
            fout << int(0.5+WMainWindow::getMW()->getFs()*last) << " ";
            fout << waveform_labels[li]->text().toLatin1().constData() << endl;
        }
    }
    else if(m_fileformat==FFSDIF){
        #ifdef SUPPORT_SDIF
            SdifFileT* filew = SdifFOpen(fileFullPath.toLatin1().constData(), eWriteFile);

            if (!filew)
                throw QString("SDIF: Cannot save the data in the specified file (permission denied?)");

            size_t generalHeaderw = SdifFWriteGeneralHeader(filew);
            size_t asciiChunksw = SdifFWriteAllASCIIChunks(filew);

            for(size_t li=0; li<starts.size(); li++) {
                // cout << labels[li].toLatin1().constData() << ": " << starts[li] << ":" << ends[li] << endl;

                // Prepare the frame
                SDIFFrame frameToWrite;
                /*set the header of the frame*/
                frameToWrite.SetStreamID(0); // TODO Ok ??
                frameToWrite.SetTime(starts[li]);
                frameToWrite.SetSignature("1MRK");

                // Fill the matrix
                SDIFMatrix tmpMatrix("1LAB");
                tmpMatrix.Set(std::string(waveform_labels[li]->text().toLatin1().constData()));
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
            throw QString("SDIF file support is not compiled in this distribution of DFasma.");
        #endif
    }
    else{
        throw QString("File format not recognized for writing this label file.");
    }

    m_lastreadtime = QDateTime::currentDateTime();
    m_isedited = false;
    setStatus();
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

void FTLabels::addLabel(double position, const QString& text){

    QPen pen(color);
    pen.setWidth(0);   

    starts.push_back(position);
    waveform_labels.push_back(new QGraphicsSimpleTextItem(text));
//    WMainWindow::getMW()->m_gvWaveform->m_scene->addItem(waveform_labels.back()); // TODO
    spectrogram_labels.push_back(new QGraphicsSimpleTextItem(text));
//    WMainWindow::getMW()->m_gvSpectrogram->m_scene->addItem(spectrogram_labels.back()); // TODO

    waveform_lines.push_back(new QGraphicsLineItem(0, -1, 0, 1));
    waveform_lines.back()->setPos(position, 0);
    waveform_lines.back()->setPen(pen);
    WMainWindow::getMW()->m_gvWaveform->m_scene->addItem(waveform_lines.back());

    spectrogram_lines.push_back(new QGraphicsLineItem(0, 0, 0, WMainWindow::getMW()->getFs()/2));
    spectrogram_lines.back()->setPos(position, 0);
    spectrogram_lines.back()->setPen(pen);
    WMainWindow::getMW()->m_gvSpectrogram->m_scene->addItem(spectrogram_lines.back());

    m_isedited = true;
    setStatus();
}

void FTLabels::moveLabel(int index, double position){
    starts[index] = position;
    waveform_lines[index]->setPos(position, 0);
    spectrogram_lines[index]->setPos(position, 0);

    m_isedited = true;
    setStatus();
}

void FTLabels::changeText(int index, const QString& text){
    waveform_labels[index]->setText(text);
    spectrogram_labels[index]->setText(text);

    m_isedited = true;
    setStatus();
}

void FTLabels::setShown(bool shown){
    FileType::setShown(shown);

    // TODO groupitem ?
    for(size_t u=0; u<starts.size(); ++u){
        waveform_labels[u]->setVisible(shown);
        spectrogram_labels[u]->setVisible(shown);
        waveform_lines[u]->setVisible(shown);
        spectrogram_lines[u]->setVisible(shown);
    }
}

void FTLabels::removeLabel(int index){

    starts.erase(starts.begin()+index);

    QGraphicsSimpleTextItem* labeltoremove = *(waveform_labels.begin()+index);
    waveform_labels.erase(waveform_labels.begin()+index);
    delete labeltoremove;
    labeltoremove = *(spectrogram_labels.begin()+index);
    spectrogram_labels.erase(spectrogram_labels.begin()+index);
    delete labeltoremove;

    QGraphicsLineItem* linetoremove = *(waveform_lines.begin()+index);
    waveform_lines.erase(waveform_lines.begin()+index);
    delete linetoremove;
    linetoremove = *(spectrogram_lines.begin()+index);
    spectrogram_lines.erase(spectrogram_lines.begin()+index);
    delete linetoremove;

    m_isedited = true;
    setStatus();
}


// Sorting functions
// (used for writing the files with ascending times)

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

    std::deque<double> sorted_starts(starts.size());
    std::deque<QGraphicsSimpleTextItem*> sorted_waveform_labels(waveform_labels.size());
    std::deque<QGraphicsSimpleTextItem*> sorted_spectrogram_labels(spectrogram_labels.size());
    std::deque<QGraphicsLineItem*> sorted_waveform_lines(starts.size());
    std::deque<QGraphicsLineItem*> sorted_spectrogram_lines(starts.size());
    for(size_t u=0; u<starts.size(); ++u){
        sorted_starts[u] = starts[indices[u]];
        sorted_waveform_labels[u] = waveform_labels[indices[u]];
        sorted_spectrogram_labels[u] = spectrogram_labels[indices[u]];
        sorted_waveform_lines[u] = waveform_lines[indices[u]];
        sorted_spectrogram_lines[u] = spectrogram_lines[indices[u]];
    }

    starts = sorted_starts;
    waveform_labels = sorted_waveform_labels;
    spectrogram_labels = sorted_spectrogram_labels;
    waveform_lines = sorted_waveform_lines;
    spectrogram_lines = sorted_spectrogram_lines;

//    cout << "sorted starts: ";
//    for(size_t u=0; u<starts.size(); ++u)
//        cout << starts[u] << " ";
//    cout << endl;

//    cout << "FTLabels::~sort" << endl;
}

FTLabels::~FTLabels() {
    clear();
}
