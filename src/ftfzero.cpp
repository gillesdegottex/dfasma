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

#include "ftfzero.h"

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include <QFileInfo>
#include <QMessageBox>
#include <QGraphicsSimpleTextItem>
#include <QDir>
#include <QFileDialog>
#include <QStatusBar>
#include <qmath.h>
#include <qendian.h>

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ui_wdialogsettings.h"
#include "gvamplitudespectrum.h"

#include "qthelper.h"

std::deque<QString> FTFZero::s_formatstrings;
FTFZero::ClassConstructor::ClassConstructor(){
    // Attention ! It has to correspond to FType definition's order.
    // Map F0 FileFormat with corresponding strings
    if(FTFZero::s_formatstrings.empty()){
        FTFZero::s_formatstrings.push_back("Unspecified");
        FTFZero::s_formatstrings.push_back("Auto");
        FTFZero::s_formatstrings.push_back("Auto Text");
        FTFZero::s_formatstrings.push_back("Text Time/Value (*.f0.bpf)");
        FTFZero::s_formatstrings.push_back("SDIF 1FQ0/1FQ0 (*.sdif)");
    }
}
FTFZero::ClassConstructor FTFZero::s_class_constructor;

void FTFZero::constructor_common(){
    m_fileformat = FFNotSpecified;
    m_src_snd = NULL;

    m_aspec_txt = new QGraphicsSimpleTextItem("unset");
    gMW->m_gvAmplitudeSpectrum->m_scene->addItem(m_aspec_txt);
    setColor(getColor()); // Indirectly set the proper color to the m_aspec_txt

    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels (overwrite the file !)"));
    m_actionSave->setShortcut(gMW->ui->actionSelectedFilesSave->shortcut());
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
    m_actionSaveAs = new QAction("Save as...", this);
    m_actionSaveAs->setStatusTip(tr("Save the f0 curve in a given file..."));
    connect(m_actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));

    m_actionAnalysisFZero = new QAction("Recompute F0", this);
    m_actionAnalysisFZero->setStatusTip(tr("Re-estimate the fundamental frequency (F0)"));
    m_actionAnalysisFZero->setShortcut(gMW->ui->actionEstimationF0->shortcut());
//    connect(m_actionAnalysisFZero, SIGNAL(triggered()), this, SLOT(estimateFZero()));

    gFL->ftfzeros.push_back(this);
}

FTFZero::FTFZero(const QString& _fileName, QObject* parent, FileType::FileContainer container, FileFormat fileformat)
    : QObject(parent)
    , FileType(FTFZERO, _fileName, this)
{
    Q_UNUSED(parent)

    if(fileFullPath.isEmpty())
        throw QString("This ctor is for existing files. Use the empty ctor for empty F0 object.");

    FTFZero::constructor_common();

    m_fileformat = fileformat;
    if(container==FileType::FCSDIF)
        m_fileformat = FFSDIF;
    else if(container==FileType::FCASCII)
        m_fileformat = FFAsciiAutoDetect;

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        load();
    }
}

FTFZero::FTFZero(const FTFZero& ft)
    : QObject(ft.parent())
    , FileType(FTFZERO, ft.fileFullPath, this)
{
    FTFZero::constructor_common();

    ts = ft.ts;
    f0s = ft.f0s;

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    updateTextsGeometry();
}

FileType* FTFZero::duplicate(){
    return new FTFZero(*this);
}

void FTFZero::load() {

    if(m_fileformat==FFNotSpecified)
        m_fileformat = FFAutoDetect;

    #ifdef SUPPORT_SDIF
    if(m_fileformat==FFAutoDetect)
        if(FileType::isFileSDIF(fileFullPath))
            m_fileformat = FFSDIF;
    #endif

    // Check for text/ascii formats
    if(m_fileformat==FFAutoDetect || m_fileformat==FFAsciiAutoDetect){
        // Find the format using grammar check

        std::ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFAutoDetect: Cannot open the file.");
        double t;
        string line, text;
        // Check the first line only (Assuming it is enough)
        if(!std::getline(fin, line))
            throw QString("FTFZero:FFAutoDetect: There is not a single line in this file.");

        // Check: <number> <number>
        std::istringstream iss(line);
        if((iss >> t >> t) && iss.eof())
            m_fileformat = FFAsciiTimeValue;
    }

    if(m_fileformat==FFAutoDetect)
        throw QString("Cannot detect the file format of this F0 file");

    // Load the data given the format found or the one given
    if(m_fileformat==FFAsciiTimeValue){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFAsciiTimeValue: Cannot open file");

        double t, value;
        string line, text;
        while(std::getline(fin, line)) {
            std::istringstream(line) >> t >> value;
            ts.push_back(t);
            f0s.push_back(value);
        }
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

        readentity.ChangeSelection("/1FQ0.1_1"); // Select directly the f0 values

        SDIFFrame frame;
        try{
            while (1) {

                /* reading the next frame of the EntityRead, return the number of
                bytes read, return 0 if the frame is not selected
                and produces an exception if eof is reached */
                if(!readentity.ReadNextFrame(frame))
                    continue;

                double t = frame.GetTime();

                for (unsigned int i=0 ; i < frame.GetNbMatrix() ; i++) {
                    /*take the matrix number "i" and put it in tmpMatrix */
                    SDIFMatrix tmpMatrix = frame.GetMatrix(i);

                    if(tmpMatrix.GetNbCols()<1 || tmpMatrix.GetNbRows()<1)
                        throw QString("F0 value is missing in a 1FQ0 frame at time ")+QString::number(t);

                    ts.push_back(t);
                    f0s.push_back(tmpMatrix.GetDouble(0, 0));
    //                cout << "VALUES " << *(ts.end()) << ":" << *(f0s.end()) << endl;

    //                /* if you want to access to the data : an example, if we want
    //                 to multiply with 2 the last column of a matrix : */
    //                double dou;
    //                int ncols = tmpMatrix.GetNbCols();
    //                int nrows = tmpMatrix.GetNbRows();
    //                cout << "nrows=" << nrows << " ncols=" << ncols << endl;
    //                for(int i = 0 ; i < nrows ; i++) {
    //                    for(int j = 0 ; j < ncols ; j++) {
    //                        dou = tmpMatrix.GetDouble(i, j);
    //                        cout << dou << " ";
    //                    }
    //                }
    //                cout << endl;
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
        #else
            throw QString("SDIF file support is not compiled in this distribution of DFasma.");
        #endif
    }
    else
        throw QString("File format not recognized for loading this F0 file.");

    updateTextsGeometry();

    m_lastreadtime = QDateTime::currentDateTime();
    setStatus();
}

bool FTFZero::reload() {
//    cout << "FTFZero::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return false;

    // Reset everything ...
    ts.clear();
    f0s.clear();

    // ... and reload the data from the file
    load();

    return true;
}

void FTFZero::saveAs() {
    if(ts.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    // Build the filter string
    QString filters;
    filters += s_formatstrings[FFAsciiTimeValue];
    #ifdef SUPPORT_SDIF
        filters += ";;"+s_formatstrings[FFSDIF];
    #endif
    QString selectedFilter;
    if(m_fileformat==FFNotSpecified) {
        if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue<int(s_formatstrings.size()))
            selectedFilter = s_formatstrings[gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue];
    }
    else
        selectedFilter = s_formatstrings[m_fileformat];

//    COUTD << fileFullPath.toLatin1().constData() << endl;

    QString fp = QFileDialog::getSaveFileName(gMW, "Save F0 file as...", fileFullPath, filters, &selectedFilter, QFileDialog::DontUseNativeDialog);

    if(!fp.isEmpty()){
        try{
            setFullPath(fp);
            if(selectedFilter==s_formatstrings[FFAsciiTimeValue])
                m_fileformat = FFAsciiTimeValue;
            #ifdef SUPPORT_SDIF
            else if(selectedFilter==s_formatstrings[FFSDIF])
                m_fileformat = FFSDIF;
            #endif

            if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
                m_fileformat = FFAsciiTimeValue;

            save();
        }
        catch(QString &e) {
            QMessageBox::critical(NULL, "Cannot save under this file path.", e);
        }
    }
}

void FTFZero::save() {
    if(ts.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
        m_fileformat = FFAsciiTimeValue;

    if(m_fileformat==FFAsciiTimeValue){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTZero: Cannot open file");

        QTextStream stream(&data);
        stream.setRealNumberPrecision(12);
        stream.setRealNumberNotation(QTextStream::ScientificNotation);
        stream.setCodec("ASCII");
        for(size_t li=0; li<ts.size(); li++)
            stream << ts[li] << " " << f0s[li] << endl;
    }
    else if(m_fileformat==FFSDIF){
        #ifdef SUPPORT_SDIF
//            SdifFileT* filew = SdifFOpen(fileFullPath.toLatin1().constData(), eWriteFile);

//            if (!filew)
//                throw QString("SDIF: Cannot save the data in the specified file (permission denied?)");

//            size_t generalHeaderw = SdifFWriteGeneralHeader(filew);
//            Q_UNUSED(generalHeaderw)
//            size_t asciiChunksw = SdifFWriteAllASCIIChunks(filew);
//            Q_UNUSED(asciiChunksw)

//            for(size_t li=0; li<starts.size(); li++) {
//                // cout << labels[li].toLatin1().constData() << ": " << starts[li] << ":" << ends[li] << endl;

//                // Prepare the frame
//                SDIFFrame frameToWrite;
//                /*set the header of the frame*/
//                frameToWrite.SetStreamID(0); // TODO Ok ??
//                frameToWrite.SetTime(starts[li]);
//                frameToWrite.SetSignature("1MRK");

//                // Fill the matrix
//                SDIFMatrix tmpMatrix("1LAB");
//                tmpMatrix.Set(std::string(waveform_labels[li]->toPlainText().toLatin1().constData()));
//                frameToWrite.AddMatrix(tmpMatrix);

//                frameToWrite.Write(filew);
//                frameToWrite.ClearData();
//            }

//            //    // Write a last empty frame for the last time
//            //    SDIFFrame frameToWrite;
//            //    frameToWrite.SetStreamID(0); // TODO Ok ??
//            //    frameToWrite.SetTime(ends.back());
//            //    frameToWrite.SetSignature("1MRK");
//            //    SDIFMatrix tmpMatrix("1LAB", 0, 0);
//            //    frameToWrite.AddMatrix(tmpMatrix);
//            //    frameToWrite.Write(filew);

//            SdifFClose(filew);

        #else
            throw QString("SDIF file support is not compiled in this distribution of DFasma.");
        #endif
    }
    else
        throw QString("File format not recognized for writing this F0 file.");

    m_lastreadtime = QDateTime::currentDateTime();
    m_is_edited = false;
    checkFileStatus(CFSMEXCEPTION);
    gFL->fileInfoUpdate();
    gMW->statusBar()->showMessage(fileFullPath+" saved.", 10000);
}

QString FTFZero::info() const {
    QString str = FileType::info();
    str += "Number of F0 values: " + QString::number(ts.size()) + "<br/>";
    if(ts.size()>0){
        // TODO Should be done once
        double meandts = 0.0;
        double meanf0 = 0.0;
        int nbnonzerovalues = 0;
        int nbzerovalues = 0;
        double f0min = std::numeric_limits<double>::infinity();
        double f0max = -std::numeric_limits<double>::infinity();
        for(size_t i=0; i<ts.size(); ++i){
            if(i>0)
                meandts += ts[i]-ts[i-1];
            if(f0s[i]>std::numeric_limits<float>::epsilon()){
//                COUTD << f0s[i] << endl;
                f0min = std::min(f0min, f0s[i]);
                f0max = std::max(f0max, f0s[i]);
                meanf0 += f0s[i];
                nbnonzerovalues++;
            }
            else
                nbzerovalues++;
        }
        meandts /= ts.size();
        meanf0 /= nbnonzerovalues;
        str += "Average sampling: " + QString("%1").arg(meandts, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()) + "s<br/>";
        str += QString("F0 in [%1,%2]Hz").arg(f0min, 0,'g',3).arg(f0max, 0,'g',5);
        if(nbzerovalues>0)
            str += QString(" with zero values");
        else
            str += QString(" without zero values");
        str += QString("<br/>Mean F0=%1Hz").arg(meanf0, 0,'g',5);
    }
    return str;
}

void FTFZero::fillContextMenu(QMenu& contextmenu) {
    FileType::fillContextMenu(contextmenu);

    contextmenu.setTitle("F0");

    contextmenu.addAction(m_actionSave);
    contextmenu.addAction(m_actionSaveAs);
}

void FTFZero::updateTextsGeometry(){
    if(!m_actionShow->isChecked())
        return;

    QRectF aspec_viewrect = gMW->m_gvAmplitudeSpectrum->mapToScene(gMW->m_gvAmplitudeSpectrum->viewport()->rect()).boundingRect();
    QTransform aspec_trans = gMW->m_gvAmplitudeSpectrum->transform();

    QTransform mat1;
    mat1.translate(1.0/aspec_trans.m11(), aspec_viewrect.top()+10/aspec_trans.m22());
    mat1.scale(1.0/aspec_trans.m11(), 1.0/aspec_trans.m22());
    m_aspec_txt->setTransform(mat1);
}
void FTFZero::setVisible(bool shown){
    FileType::setVisible(shown);

    if(shown)
        updateTextsGeometry();

    m_aspec_txt->setVisible(shown);
}

void FTFZero::setColor(const QColor& color) {
    FileType::setColor(color);

    QPen pen(getColor());
    pen.setWidth(0);
    QBrush brush(getColor());

    m_aspec_txt->setPen(pen);
    m_aspec_txt->setBrush(brush);
}


double FTFZero::getLastSampleTime() const {
    if(ts.empty())
        return 0.0;
    else
        return *((ts.end()-1));
}

FTFZero::~FTFZero() {
    delete m_aspec_txt;

    gFL->ftfzeros.erase(std::find(gFL->ftfzeros.begin(), gFL->ftfzeros.end(), this));
}


// Analysis --------------------------------------------------------------------

#include "../external/REAPER/epoch_tracker/epoch_tracker.h"

FTFZero::FTFZero(QObject *parent, FTSound *ftsnd, double f0min, double f0max, double tstart, double tend)
    : QObject(parent)
    , FileType(FTFZERO, DropFileExtension(ftsnd->fileFullPath)+".f0.txt", this, ftsnd->getColor())
{
    FTFZero::constructor_common();

    estimate(ftsnd, f0min, f0max, tstart, tend);
}

void FTFZero::estimate(FTSound *ftsnd, double f0min, double f0max, double tstart, double tend) {

    if(ftsnd)
        m_src_snd = ftsnd;

    f0min = std::max(f0min, 20.0); // Fix hard-coded minimum for f0
    f0max = std::min(f0max, gFL->getFs()/2.0); // Fix hard-coded minimum for f0
    if(tstart!=-1) tstart = std::max(tstart, 0.0);
    if(tend!=-1) tend = std::min(tend, m_src_snd->getLastSampleTime());

//    COUTD << "FTFZero::estimate src=" << m_src_snd->visibleName << " [" << f0min << "," << f0max << "]Hz [" << tstart << "," << tend << "]s" << endl;


//    if(_fileName==""){
////        if(gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText==FFSDIF)
////            setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");
////        m_fileformat = FFSDIF;
////        else
//        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.txt");
//        m_fileformat = FFAsciiTimeValue;
//    }

    // Compute the f0 from the given sound file

    gMW->globalWaitingBarMessage("Estimating F0 of "+fileFullPath+" in ["+QString::number(f0min)+","+QString::number(f0max)+"]Hz ...", 8);

    double timestepsize = gMW->m_dlgSettings->ui->sbEstimationStepSize->value();

    EpochTracker et; // TODO to put in FTSound because other features can be extracted from it (ex. GCIs, voicing)

    gMW->globalWaitingBarSetValue(1);

    // Initialize with the given input
    // Start with a dirty copy in the necessary format
    vector<int16_t> data(m_src_snd->wav.size());
    for(size_t i=0; i<m_src_snd->wav.size(); ++i)
        data[i] = 32768*m_src_snd->wav[i];
    int16_t* wave_datap = data.data();
    int32_t n_samples = m_src_snd->wav.size();
    float sample_rate = gFL->getFs();
    if(sample_rate<6000.0)
        QMessageBox::warning(gMW, "Problem during estimation of F0", "Sampling rate is smaller than 6kHz, which may create substantial estimation errors.");

    gMW->globalWaitingBarSetValue(2);

    if (!et.Init(wave_datap, n_samples, sample_rate,
          f0min, f0max, true, true))
        throw QString("EpochTracker initialisation failed");

    gMW->globalWaitingBarSetValue(3);

    // Compute f0 and pitchmarks.
    if (!et.ComputeFeatures())
        throw QString("Failed to compute features");

    gMW->globalWaitingBarSetValue(4);

    // et.TrackEpochs()
    et.CreatePeriodLattice();
    gMW->globalWaitingBarSetValue(5);
    et.DoDynamicProgramming();
    gMW->globalWaitingBarSetValue(6);
    if (!et.BacktrackAndSaveOutput())
        throw QString("Failed to track epochs");

    gMW->globalWaitingBarSetValue(7);

    std::vector<float> f0; // TODO Drop this temporary variable
    std::vector<float> corr; // Currently unused
    if (!et.ResampleAndReturnResults(timestepsize, &f0, &corr))
        throw QString("Cannot resample the results");

    // Everything seemed to go well, let's fill/replace the f0 curve
    if(tstart==-1 && tend==-1){
        ts.clear();
        f0s.clear();
        for (size_t i=0; i<f0.size(); ++i) {
            ts.push_back(timestepsize*i);
            f0s.push_back(f0[i]);
        }
    }
    else{

//        FLAG
//        // Start by erasing the values within the time segment
//        std::deque<double>::iterator itlb = std::lower_bound(ts.begin(), ts.end(), tstart);
//        std::deque<double>::iterator ithb = std::upper_bound(ts.begin(), ts.end(), tend);
//        FLAG
////        COUTD << it << endl;
//        COUTD << *itlb << "," << *ithb << endl;
//        COUTD << itlb-ts.begin() << "," << ithb-ts.begin() << endl;
//        COUTD << ithb-itlb << endl;

//        ts.erase(itlb, ithb);
//        FLAG
//        f0s.erase(f0s.begin()+(itlb-ts.begin()), f0s.begin()+(ithb-ts.begin()));
//        COUTD << ts.size() << " " << f0s.size() << endl;

        int first = -1;
        int last = -1;
        for(size_t i=0; i<ts.size(); ++i){
            if(ts[i]<tstart)
                first = i;
            if(ts[i]<tend)
                last = i;
        }
        first++;
//        COUTD << first << "," << last << " size=" << ts.size() << endl;
//        COUTD << ts[first] << "," << ts[last] << endl;
//        COUTD << last-first+1 << endl;

        std::deque<double>::iterator tsl = ts.erase(ts.begin()+first, ts.begin()+last+1);
        std::deque<double>::iterator f0sl = f0s.erase(f0s.begin()+first, f0s.begin()+last+1);

//        for(size_t i=first; i<=last; ++i)
//            f0s[i] = 0.0;

//        FLAG

        // And insert the new values of the newly computed values

        // Find the elements in the new values
        int nitlb = std::ceil(tstart/timestepsize);
        int nithb = std::floor(tend/timestepsize);

//        COUTD << nitlb << "," << nithb << endl;
//        COUTD << timestepsize*nitlb << "," << timestepsize*nithb << endl;
//        COUTD << nithb-nitlb << endl;

        // Add the new times ...
        std::vector<double> nts(nithb-nitlb);
        COUTD << nts.size() << endl;
        for(size_t i=0; i<nts.size(); ++i)
            nts[i] = timestepsize*(nitlb+i);
//        FLAG
        ts.insert(tsl, nts.begin(), nts.end());

//        FLAG
        // ... and the new f0 values.
        f0s.insert(f0sl, f0.begin()+nitlb, f0.begin()+nithb);
//        FLAG
    }

    gMW->globalWaitingBarDone();

    updateTextsGeometry();

    setStatus();

//    COUTD << ts.size() << " " << f0s.size() << endl;
}
