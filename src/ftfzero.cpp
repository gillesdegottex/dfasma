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
#include "gvwaveform.h"
#include "gvspectrumamplitude.h"
#include "gvspectrogram.h"

#include "qaegisampledsignal.h"
#include "qaehelpers.h"

extern QString DFasmaVersion();

std::deque<QString> FTFZero::s_formatstrings;
FTFZero::ClassConstructor::ClassConstructor(){
    // Attention ! It has to correspond to FType definition's order.
    // Map F0 FileFormat with corresponding strings
    if(FTFZero::s_formatstrings.empty()){
        FTFZero::s_formatstrings.push_back("Unspecified");
        FTFZero::s_formatstrings.push_back("Auto");
        FTFZero::s_formatstrings.push_back("Text - Auto");
        FTFZero::s_formatstrings.push_back("Text - Time Value (*.f0.txt)");
        FTFZero::s_formatstrings.push_back("Text - Value (single column) (*.f0.txt)");
        FTFZero::s_formatstrings.push_back("SDIF - 1FQ0/1FQ0 (*.sdif)");
    }
}
FTFZero::ClassConstructor FTFZero::s_class_constructor;

QString FTFZero::createFileNameFromSound(const QString& sndfilename){
    QString fileName = dropFileExtension(sndfilename);

    if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFSDIF)
        return fileName+".sdif";
    else if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiAutoDetect
            || gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiTimeValue)
        return fileName+".f0.txt";

    return fileName+".f0.txt";
}


void FTFZero::constructor_internal(){
    m_fileformat = FFNotSpecified;
    m_src_snd = NULL;
    m_giF0ForSpectrogram = NULL;
    m_giHarmonicForSpectrogram = NULL;

    connect(m_actionShow, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));

    m_aspec_txt = new QGraphicsSimpleTextItem("unset"); // TODO delete done ?
    gMW->m_gvSpectrumAmplitude->m_scene->addItem(m_aspec_txt);
    setColor(getColor()); // Indirectly set the proper color to the m_aspec_txt

    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels (overwrite the file !)"));
    m_actionSave->setShortcut(gMW->ui->actionSelectedFilesSave->shortcut());
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
    m_actionSaveAs = new QAction("Save as...", this);
    m_actionSaveAs->setStatusTip(tr("Save the f0 curve in a given file..."));
    connect(m_actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));
    m_actionSetSource = new QAction("Set corresponding waveform...", this);
    m_actionSetSource->setStatusTip(tr("Set the waveform this F0 should correspond to."));
    connect(m_actionSetSource, SIGNAL(triggered()), gFL, SLOT(setSource()));
}

void FTFZero::constructor_external(){
    FileType::constructor_external();

    gFL->ftfzeros.push_back(this);

    QPen pen(getColor());
    pen.setWidth(0);

    m_giF0ForSpectrogram = new QAEGISampledSignal(&ts, &f0s, gMW->m_gvSpectrogram);
    m_giF0ForSpectrogram->setShowZeroValues(false);
    QPen spectro_pen(getColor());
    spectro_pen.setCosmetic(true);
    spectro_pen.setWidth(3);
    m_giF0ForSpectrogram->setPen(spectro_pen);
    gMW->m_gvSpectrogram->m_scene->addItem(m_giF0ForSpectrogram);

    m_giHarmonicForSpectrogram = new QAEGISampledSignal(&ts, &f0s, gMW->m_gvSpectrogram);
    m_giHarmonicForSpectrogram->setShowZeroValues(false);
    QColor harmcolor = getColor();
//    harmcolor.setAlphaF(0.5);
    QPen harmspectro_pen(harmcolor);
    harmspectro_pen.setCosmetic(true);
    harmspectro_pen.setWidth(3);
    m_giHarmonicForSpectrogram->setPen(harmspectro_pen);
//    m_giHarmonicForSpectrogram->setVisible(true);
    m_giHarmonicForSpectrogram->setVisible(gMW->m_gvSpectrogram->m_aSpectrogramShowHarmonics->isChecked());
//    m_giHarmonicForSpectrogram->setGain(1.0);
    gMW->m_gvSpectrogram->m_scene->addItem(m_giHarmonicForSpectrogram);
}

// Construct an empty FZero object
FTFZero::FTFZero(QObject *parent)
    : QObject(parent)
    , FileType(FTFZERO, "", this)
{
    Q_UNUSED(parent);

    FTFZero::constructor_internal();

    if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFSDIF)
        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.f0.sdif");
    else
        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.f0.txt");
    m_fileformat = FFNotSpecified;

    FTFZero::constructor_external();
}

// Construct from an existing file
FTFZero::FTFZero(const QString& _fileName, QObject* parent, FileType::FileContainer container, FileFormat fileformat)
    : QObject(parent)
    , FileType(FTFZERO, _fileName, this)
{
    Q_UNUSED(parent)

    if(fileFullPath.isEmpty())
        throw QString("This ctor is for existing files. Use the empty ctor for empty F0 object.");

    FTFZero::constructor_internal();

    m_fileformat = fileformat;
    if(container==FileType::FCSDIF)
        m_fileformat = FFSDIF;
    else if(container==FileType::FCASCII)
        m_fileformat = FFAsciiAutoDetect;

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        load();
    }

    FTFZero::constructor_external();
}

FTFZero::FTFZero(const FTFZero& ft)
    : QObject(ft.parent())
    , FileType(FTFZERO, ft.fileFullPath, this)
{
    FTFZero::constructor_internal();

    ts = ft.ts;
    f0s = ft.f0s;

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    updateTextsGeometry();

    FTFZero::constructor_external();
}

FileType* FTFZero::duplicate(){
    return new FTFZero(*this);
}

void FTFZero::load() {

    if(m_fileformat==FFNotSpecified)
        m_fileformat = FFAutoDetect;

    #ifdef SUPPORT_SDIF
    if(m_fileformat==FFAutoDetect){
        if(FileType::isFileSDIF(fileFullPath))
            m_fileformat = FFSDIF;
        else if(FileType::isFileEST(fileFullPath))
            m_fileformat = FFEST;
    }
    #endif

    // Check for text/ascii formats
    if(m_fileformat==FFAutoDetect || m_fileformat==FFAsciiAutoDetect){

        std::ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFAutoDetect: Cannot open the file.");
        string line;
        // Check the first line only (Assuming it is enough)
        if(!std::getline(fin, line))
            throw QString("FTFZero:FFAutoDetect: There is not a single line in this file.");

        // Guess the format using magic words
        if(line.find("EST_File")==0)
            m_fileformat = FFEST;
        else{
            // Guess the format using grammar check

            double t;
            // Check: <number> <number>
            std::istringstream iss(line);
            if((iss >> t >> t) && iss.eof())
                m_fileformat = FFAsciiTimeValue;
            else{
                // Check: <number>
                std::istringstream iss(line);
                if((iss >> t) && iss.eof())
                    m_fileformat = FFAsciiValue;
            }
        }
    }

    if(m_fileformat==FFAutoDetect)
        throw QString("Cannot detect the file format of this F0 file");

    // Load the data given the format found or the one given
    if(m_fileformat==FFAsciiTimeValue){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFAsciiTimeValue: Cannot open file");

        double t, value;
        string line;
        while(std::getline(fin, line)) {
            std::istringstream(line) >> t >> value;
            ts.push_back(t);
            f0s.push_back(value);
        }
    }
    else if(m_fileformat==FFAsciiValue){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFAsciiValue: Cannot open file");

        double t=0.0;
        double value;
        string line;
        while(std::getline(fin, line)) {
            std::istringstream(line) >> value;
            ts.push_back(t);
            f0s.push_back(value);
            t += gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();
        }
    }
    else if(m_fileformat==FFEST){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTFZero:FFEST: Cannot open file");

        double t = 0.0;
        double voiced = false;
        double value;
        string line;
        enum DataType {ESTDTunknown, ESTDTASCII, ESTDTBinary1, ESTDTBinary2};
        DataType datatype = ESTDTunknown;
        int nbframes = -1;

        bool skippingheader = true;
        while(skippingheader && std::getline(fin, line)) {
            if(line.find("EST_Header_End")!=string::npos)
                skippingheader = false;
            else if(line.find("DataType")!=string::npos){
                if(line.find("binary2")!=string::npos)
                    datatype = ESTDTBinary2;
                else if(line.find("ascii")!=string::npos)
                    datatype = ESTDTASCII;
            }
            else if(line.find("NumFrames")!=string::npos){
                std::string numframeskeyword;
                std::istringstream strstr(line);
                strstr >> numframeskeyword;
                strstr >> nbframes;
            }
        }

        if(datatype==ESTDTASCII){
            while(std::getline(fin, line)) {
                std::istringstream(line) >> t >> voiced >> value;
                ts.push_back(t);
                if(!voiced || value<0.0)
                    value = 0.0;
                f0s.push_back(value);
            }
        }
        else if(datatype==ESTDTBinary2){
            float tf;
            for(int l=0; l<nbframes; ++l){
                fin.read((char*)&tf, sizeof(tf));
                ts.push_back(tf);
            }
            char vuc;
            for(int l=0; l<nbframes; ++l){
                fin.read((char*)&vuc, sizeof(vuc));
                // DCOUT << vuc << endl; // Don't use it
            }
            float valuef;
            for(int l=0; l<nbframes; ++l){
                fin.read((char*)&valuef, sizeof(valuef));
                if(valuef<0.0)
                    valuef = 0.0;
                f0s.push_back(valuef);
            }
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
    if(m_giF0ForSpectrogram) m_giF0ForSpectrogram->update();
    if(m_giHarmonicForSpectrogram) m_giHarmonicForSpectrogram->update();

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
            throw QString("FTZero: Cannot open file for writting");

        QTextStream stream(&data);
        stream.setRealNumberPrecision(12);
        stream.setRealNumberNotation(QTextStream::ScientificNotation);
        stream.setCodec("ASCII");
        for(size_t li=0; li<ts.size(); li++)
            stream << ts[li] << " " << f0s[li] << endl;
    }
    else if(m_fileformat==FFAsciiValue){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTZero: Cannot open file for writting");

        QTextStream stream(&data);
        stream.setRealNumberPrecision(12);
        stream.setRealNumberNotation(QTextStream::ScientificNotation);
        stream.setCodec("ASCII");
        for(size_t li=0; li<ts.size(); li++)
            stream << f0s[li] << endl;
    }
    else if(m_fileformat==FFSDIF){
        #ifdef SUPPORT_SDIF
            SdifFileT* filew = SdifFOpen(fileFullPath.toLatin1().constData(), eWriteFile);

            if (!filew)
                throw QString("SDIF: Cannot save the data in the specified file (permission denied?)");

            size_t generalHeaderw = SdifFWriteGeneralHeader(filew);
            Q_UNUSED(generalHeaderw)
            size_t asciiChunksw = SdifFWriteAllASCIIChunks(filew);
            Q_UNUSED(asciiChunksw)

            // Save information
            SDIFFrame frameToWrite;
            /*set the header of the frame*/
            frameToWrite.SetStreamID(0); // TODO Ok ??
            frameToWrite.SetSignature("1NVT");
            SDIFMatrix tmpMatrix("1NVT");
            QString info = "";
            info += "SampleRate\t"+QString::number(gFL->getFs())+"\n";
            info += "NumChannels\t"+QString::number(1)+"\n";
            if(gFL->hasFile(m_src_snd))
                info += "Soundfile\t"+m_src_snd->fileFullPath+"\n";
            info += "Version\t"+DFasmaVersion()+"\n";
            info += "Creator\tDFasma\n";
            tmpMatrix.Set(info.toLatin1().constData());
            frameToWrite.AddMatrix(tmpMatrix);
            frameToWrite.Write(filew);

            for(size_t li=0; li<ts.size(); li++) {
                // cout << labels[li].toLatin1().constData() << ": " << starts[li] << ":" << ends[li] << endl;

                // Prepare the frame
                SDIFFrame frameToWrite;
                /*set the header of the frame*/
                frameToWrite.SetStreamID(0); // TODO Ok ??
                frameToWrite.SetTime(ts[li]);
                frameToWrite.SetSignature("1FQ0");

                // Fill the matrix
                SDIFMatrix tmpMatrix("1FQ0", 1, 1);
                // SDIFMatrix tmpMatrix("1FQ0", 1, 4);
                tmpMatrix.Set(0, 0, f0s[li]); // Frequency
                // tmpMatrix.Set(0, 1, 1.0);  // Confidence
                // tmpMatrix.Set(0, 2, 1.0);  // Score
                // tmpMatrix.Set(0, 3, 1.0);  // RealAmplitude
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
    else
        throw QString("File format not recognized for writing this F0 file.");

    m_lastreadtime = QDateTime::currentDateTime();
    m_is_edited = false;
    checkFileStatus(CFSMEXCEPTION);
    gFL->fileInfoUpdate();
    gMW->statusBar()->showMessage(fileFullPath+" saved.", 3000);
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
    contextmenu.addSeparator();
    contextmenu.addAction(gMW->ui->actionEstimationF0);
    contextmenu.addAction(gMW->ui->actionEstimationVoicedUnvoicedMarkers);
    contextmenu.addAction(m_actionSetSource);
}

void FTFZero::updateTextsGeometry(){
    if(!m_actionShow->isChecked())
        return;

    QRectF aspec_viewrect = gMW->m_gvSpectrumAmplitude->mapToScene(gMW->m_gvSpectrumAmplitude->viewport()->rect()).boundingRect();
    QTransform aspec_trans = gMW->m_gvSpectrumAmplitude->transform();

    QTransform mat1;
    mat1.translate(1.0/aspec_trans.m11(), aspec_viewrect.top()+10/aspec_trans.m22());
    mat1.scale(1.0/aspec_trans.m11(), 1.0/aspec_trans.m22());
    m_aspec_txt->setTransform(mat1);
}
void FTFZero::setVisible(bool shown){
    FileType::setVisible(shown);

    if(shown)
        updateTextsGeometry();

    if(m_aspec_txt) m_aspec_txt->setVisible(shown);
    if(m_giF0ForSpectrogram) m_giF0ForSpectrogram->setVisible(shown);
    if(m_giHarmonicForSpectrogram) m_giHarmonicForSpectrogram->setVisible(shown);
}

void FTFZero::setSource(FileType *src){
    if(src->is(FTSOUND)){
        m_src_snd = (FTSound*)src;
        ((FTSound*)src)->m_f0 = this;
    }
}

void FTFZero::setColor(const QColor& color){
    if(color==getColor())
        return;

    FileType::setColor(color);

    QPen pen(getColor());
    pen.setWidth(0);
    QBrush brush(getColor());

    if(m_aspec_txt){
        m_aspec_txt->setPen(pen);
        m_aspec_txt->setBrush(brush);
    }

    pen = m_giF0ForSpectrogram->getPen();
    pen.setColor(color);
    m_giF0ForSpectrogram->setPen(pen);
    m_giHarmonicForSpectrogram->setPen(pen);
}

void FTFZero::zposReset(){
    m_giF0ForSpectrogram->setZValue(0.0);
    m_giHarmonicForSpectrogram->setZValue(0.0);
    m_aspec_txt->setZValue(0.0);
}

void FTFZero::zposBringForward(){
    m_giF0ForSpectrogram->setZValue(1.0);
    m_giHarmonicForSpectrogram->setZValue(1.0);
    m_aspec_txt->setZValue(1.0);
}

double FTFZero::getLastSampleTime() const {
    if(ts.empty())
        return 0.0;
    else
        return *((ts.end()-1));
}

void FTFZero::edit(double t, double f0){
    if(t<0.0)
        return;
    if(f0<0)
        f0 = 0.0;

    double step = -1.0;
    // step from current data is far from reliable because there might be
    //      gaps from one value to the next.
    //      Also, The smallest can be far too small.
    //      Thus, use the default value
    //    if(ts.size()>1)
    //        step = qae::median(qae::diff(ts));
    //    else
    step = gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();

    if(step==-1.0 || step==0.0)
        step = gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();

    int ri = -1; // Index to the closest values wrt time
    double smallestdistance = std::numeric_limits<double>::infinity();
    if(!ts.empty()){
        ri = 0;
        smallestdistance = std::abs(ts[ri]-t);
        for(size_t n=1; n<ts.size(); ++n){
            if(std::abs(ts[n]-t)<smallestdistance){
                ri = n;
                smallestdistance = std::abs(ts[n]-t);
            }
        }
    }

//    DCOUT << "FTFZero::edit " << t << " " << f0 << " " << step << endl;

    if(ts.empty()
        || (t<ts.front()-step/2 || t>ts.back()+step/2)
        || (smallestdistance>step/2)){
        // Add a new value
        double tt = step*int(t/step+0.5);
        if(ts.empty()){
            ts.push_back(tt);
            f0s.push_back(f0);
        }
        else if(t<ts.front()-step/2){
            ts.insert(ts.begin(), tt);
            f0s.insert(f0s.begin(), f0);
        }
        else if(t>ts.back()+step/2){
            ts.push_back(tt);
            f0s.push_back(f0);
        }
        else if(smallestdistance>step/2){
            if(t-ts[ri]>0){
                ts.insert(ts.begin()+ri+1, tt);
                f0s.insert(f0s.begin()+ri+1, f0);
            }
            if(t-ts[ri]<0){
                ts.insert(ts.begin()+ri, tt);
                f0s.insert(f0s.begin()+ri, f0);
            }
        }
//        DCOUT << "FTFZero::edit add t=" << ts.back() << " f0=" << f0s.back() << " step=" << step << endl;
    }
    else{
        // Modify an existing value (i.e. keeping the same times ts)
        if(ri>=0 && ri<int(ts.size()))
            f0s[ri] = f0;
    }

    if(m_giF0ForSpectrogram){
        m_giF0ForSpectrogram->updateMinMaxValues();
        m_giF0ForSpectrogram->updateGeometry();
    }

    m_is_edited = true;
    setStatus();
}

FTFZero::~FTFZero() {
    if(gFL->m_prevSelectedFZero==this)
        gFL->m_prevSelectedFZero = NULL;

    if(m_src_snd)
        m_src_snd->m_f0 = NULL;

    delete m_giF0ForSpectrogram;
    delete m_giHarmonicForSpectrogram;
    delete m_aspec_txt;

    gFL->ftfzeros.erase(std::find(gFL->ftfzeros.begin(), gFL->ftfzeros.end(), this));

    delete m_actionSave;
    delete m_actionSaveAs;
    delete m_actionSetSource;
}

// Drawing ---------------------------------------------------------------------

void FTFZero::draw_freq_amp(QPainter* painter, const QRectF& rect){
    Q_UNUSED(rect)

    if(!m_actionShow->isChecked()
       || ts.size()<1)
        return;

    //            QPen outlinePen(gMW->ftfzeros[fi]->color);
    //            outlinePen.setWidth(0);
    //            painter->setPen(outlinePen);
    //            painter->setBrush(QBrush(gMW->ftfzeros[fi]->color));

    double ny = gFL->getFs()/2;

    double ct = 0.0; // The time where the f0 curve has to be sampled
    if(gMW->m_gvWaveform->hasSelection())
        ct = 0.5*(gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.nl+gMW->m_gvSpectrumAmplitude->m_trgDFTParameters.nr)/gFL->getFs();
    else
        ct = gMW->m_gvWaveform->getPlayCursorPosition();

    double cf0 = qae::interp_stepatzeros<double>(ts, f0s, ct);

    // Update the f0 text
    // TODO Should be moved to setWindowRange (need to move the cf0 computation there too)
    m_aspec_txt->setPos(cf0, 0.0);
    m_aspec_txt->setText(QString("%1Hz").arg(cf0));

    if(cf0<=0.0)
        return;

    // Draw the f0 vertical line
    QColor c = getColor();
    c.setAlphaF(1.0);
    QPen outlinePen(c);
    outlinePen.setWidth(0);
    painter->setPen(outlinePen);
    painter->drawLine(QLineF(cf0, -3000, cf0, 3000));

    // Draw harmonics up to Nyquist
    c.setAlphaF(0.5);
    outlinePen.setColor(c);
    painter->setPen(outlinePen);
    for(int h=2; h<int(ny/cf0)+1; h++)
        painter->drawLine(QLineF(h*cf0, -3000, h*cf0, 3000));
}

// Analysis --------------------------------------------------------------------

#include "../external/REAPER/epoch_tracker/epoch_tracker.h"

FTFZero::FTFZero(QObject *parent, FTSound *ftsnd, double f0min, double f0max, double tstart, double tend, bool force)
    : QObject(parent)
    , FileType(FTFZERO, createFileNameFromSound(ftsnd->fileFullPath), this, ftsnd->getColor())
{
    FTFZero::constructor_internal();

    if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFSDIF)
        m_fileformat = FFSDIF;
    else if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiAutoDetect
            || gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiTimeValue)
        m_fileformat = FFAsciiTimeValue;

    estimate(ftsnd, f0min, f0max, tstart, tend, force);

    FTFZero::constructor_external();
}

void FTFZero::estimate(FTSound *ftsnd, double f0min, double f0max, double tstart, double tend, bool force) {

    if(ftsnd)
        m_src_snd = ftsnd;

    if(!gFL->hasFile(m_src_snd)){
        QMessageBox::warning(gMW, "Missing Source file", "The source file used for updating the F0 is not listed in the application anymore.");
        return;
    }

    if(m_src_snd)
        m_src_snd->m_f0 = this;

    double fs = gFL->getFs();

    f0min = std::max(f0min, gMW->m_dlgSettings->ui->dsbEstimationF0Min->minimum()); // Fix hard-coded minimum for f0
    f0max = std::min(f0max, fs/2.0); // Fix hard-coded minimum for f0
    if(tstart!=-1)
        tstart = std::max(tstart, 0.0);
    if(tend!=-1)
        tend = std::min(tend, m_src_snd->getLastSampleTime());

//    COUTD << "FTFZero::estimate src=" << m_src_snd->visibleName << " [" << f0min << "," << f0max << "]Hz [" << tstart << "," << tend << "]s " << force << endl;

//    if(_fileName==""){
////        if(gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText==FFSDIF)
////            setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");
////        m_fileformat = FFSDIF;
////        else
//        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.txt");
//        m_fileformat = FFAsciiTimeValue;
//    }

    // Compute the f0 from the given sound file
    QString msg = "Estimating F0 of "+fileFullPath+" in ["+QString::number(f0min)+","+QString::number(f0max)+"]Hz ";
    if(force)
        msg += " without voiced/unvoiced decision ";
    gMW->globalWaitingBarMessage(msg+"...", 8);

    double timestepsize = gMW->m_dlgSettings->ui->sbEstimationStepSize->value();

    EpochTracker et; // TODO to put in FTSound because other features can be extracted from it (ex. GCIs, voicing)
    et.set_external_frame_interval(timestepsize);
    et.set_unvoiced_pulse_interval(timestepsize);
    if(force) et.set_unvoiced_cost(100); // Set arbitray huge cost for avoiding unvoiced segments

    gMW->globalWaitingBarSetValue(1);

    // Initialize with the given input
    // Start with a dirty copy in the necessary format
    // #388: Compute only the necessary values (and not all of the file)
    //       Doing so, the dynamic prog result is not the same.
    int64_t iskipfirst = std::min(int64_t(m_src_snd->wav.size()),std::max(int64_t(0),int64_t(fs*(tstart-10*timestepsize))));
    int64_t iskiplast;
    if(tend!=-1)
        iskiplast = std::min(int64_t(m_src_snd->wav.size()),std::max(int64_t(0),int64_t(fs*(tend+10*timestepsize))));
    else
        iskiplast = m_src_snd->wav.size()-1;
    double tiskipfirst = iskipfirst/fs; // First index of signal's segment which is analyzed.
    vector<int16_t> data(iskiplast-iskipfirst+1);
    for(size_t i=0; i<data.size(); ++i){
        int64_t idx = i+iskipfirst-m_src_snd->m_giWavForWaveform->delay();
        if(idx>=0 && idx<int64_t(m_src_snd->wav.size()))
            data[i] = 32768*m_src_snd->wav[idx];
        else{
            data[i] = 0.0;
        }
    }
//    COUTD << iskipfirst << " " << data.size() << endl;
    if(fs<6000.0)
        QMessageBox::warning(gMW, "Problem during estimation of F0", "Sampling rate is smaller than 6kHz, which may create substantial estimation errors.");

    gMW->globalWaitingBarSetValue(2);

    if (!et.Init(data.data(), data.size(), fs, f0min, f0max, true, true))
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

    // Force clip the f0 values
    for (size_t i=0; i<f0.size(); ++i)
        if(f0[i]>0.0)
            f0[i] = std::max(float(f0min),std::min(float(f0max),f0[i]));

    // Estimation is done, let's fill/replace the f0 curve
    if(tstart==-1 && tend==-1){
        // If time segment is undefined, replace everything
        ts.resize(f0.size());
        for (size_t i=0; i<ts.size(); ++i)
            ts[i] = timestepsize*i;
        f0s.clear();
        f0s.insert(f0s.end(), f0.begin(), f0.end());
    }
    else{
        // Find where the former values are
        // (using log2-time search)
        std::vector<double>::iterator itlb = std::lower_bound(ts.begin(), ts.end(), tstart);
        std::vector<double>::iterator ithb = std::upper_bound(ts.begin(), ts.end(), tend);
        int erasefirst = itlb-ts.begin();
        int eraselast = ithb-ts.begin()-1;

        // Erase the former values
        std::vector<double>::iterator tsl = ts.erase(ts.begin()+erasefirst, ts.begin()+eraselast+1);
        std::vector<double>::iterator f0sl = f0s.erase(f0s.begin()+erasefirst, f0s.begin()+eraselast+1);

        // And insert the new values

        // Find the elements in the new values
        int nitlb = std::ceil((tstart-tiskipfirst)/timestepsize);
        int nithb = std::floor((tend-tiskipfirst)/timestepsize);

        // Add the new times ...
        std::vector<double> nts(nithb-nitlb+1);
        for(size_t i=0; i<nts.size(); ++i)
            nts[i] = timestepsize*(nitlb+i) + tiskipfirst;
        ts.insert(tsl, nts.begin(), nts.end());
//        ts.clear(); ts.insert(ts.end(), nts.begin(), nts.end());

        // ... and the new f0 values.
        f0s.insert(f0sl, f0.begin()+nitlb, f0.begin()+nithb+1);
//        f0s.clear(); f0s.insert(f0s.end(), f0.begin()+nitlb, f0.begin()+nithb+1);
    }

    gMW->globalWaitingBarDone();

    if(m_giF0ForSpectrogram)
        m_giF0ForSpectrogram->updateGeometry();
    updateTextsGeometry();

    m_is_edited = true;
    setStatus();

//    COUTD << ts.size() << " " << f0s.size() << endl;
}
