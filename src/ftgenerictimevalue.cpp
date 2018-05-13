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

#include "ftgenerictimevalue.h"

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
#include "gvgenerictimevalue.h"
#include "wgenerictimevalue.h"

#include "qaegisampledsignal.h"
#include "qaehelpers.h"

std::deque<QString> FTGenericTimeValue::s_formatstrings;
FTGenericTimeValue::ClassConstructor::ClassConstructor(){
    // Attention ! It has to correspond to FType definition's order.
    // Map FileFormat with corresponding strings
    if(FTGenericTimeValue::s_formatstrings.empty()){
        FTGenericTimeValue::s_formatstrings.push_back("Unspecified");
        FTGenericTimeValue::s_formatstrings.push_back("Auto");
        FTGenericTimeValue::s_formatstrings.push_back("Text - Auto");
        FTGenericTimeValue::s_formatstrings.push_back("Text - [Time, Value] (*.txt)");
        FTGenericTimeValue::s_formatstrings.push_back("Text - [Value] (*.txt)");
        FTGenericTimeValue::s_formatstrings.push_back("Binary - Auto (*.*)");
        FTGenericTimeValue::s_formatstrings.push_back("Binary - float 32bits (*.*)");
        FTGenericTimeValue::s_formatstrings.push_back("Binary - float 64bits (*.*)");
        FTGenericTimeValue::s_formatstrings.push_back("SDIF - 1FQ0/1FQ0 (*.sdif)"); // TODO
    }
}
FTGenericTimeValue::ClassConstructor FTGenericTimeValue::s_class_constructor;

//QString FTGenericTimeValue::createFileNameFromSound(const QString& sndfilename){
//    QString fileName = dropFileExtension(sndfilename);

//    if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFSDIF)
//        return fileName+".sdif";
//    else if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiAutoDetect
//            || gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue==FFAsciiTimeValue)
//        return fileName+".f0.txt";

//    return fileName+".f0.txt";
//}

void FTGenericTimeValue::constructor_internal(GVGenericTimeValue* view){
    m_fileformat = FFNotSpecified;
    m_view = view;
    m_giGenericTimeValue = NULL;
    m_values_min = -1000;
    m_values_max = +1000;

    connect(m_actionShow, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));

//    m_aspec_txt = new QGraphicsSimpleTextItem("unset"); // TODO delete ?
//    gMW->m_gvSpectrumAmplitude->m_scene->addItem(m_aspec_txt);
    setColor(getColor()); // Indirectly set the proper color to the m_aspec_txt
}

void FTGenericTimeValue::updateMinMaxValues(){
    m_values_min = +std::numeric_limits<double>::infinity();
    m_values_max = -std::numeric_limits<double>::infinity();
    for(size_t i=0; i<values.size(); ++i){
        double value = values[i];
        if(!std::isinf(value)){
            m_values_min = std::min(m_values_min, value);
            m_values_max = std::max(m_values_max, value);
        }
    }
}

void FTGenericTimeValue::constructor_external(){
    FileType::constructor_external();

    gFL->ftgenerictimevalues.push_back(this);

    QPen pen(getColor());
    pen.setWidth(0);

    m_giGenericTimeValue = new QAEGISampledSignal(&ts, &values, m_view);
    QPen spectro_pen(getColor());
    spectro_pen.setCosmetic(true);
    spectro_pen.setWidth(1);
    m_giGenericTimeValue->setPen(spectro_pen);

    m_view->m_scene->addItem(m_giGenericTimeValue);
    m_view->m_ftgenerictimevalues.append(this);
    m_view->updateSceneRect();
    m_view->viewSet();
}

FTGenericTimeValue::FTGenericTimeValue(const QString& _fileName, WidgetGenericTimeValue *parent, FileType::FileContainer container, FileFormat fileformat)
    : QObject(gFL)
    , FileType(FTGENTIMEVALUE, FileType::removeDataSelectors(_fileName), this)
{
    if(fileFullPath.isEmpty())
        throw QString("This ctor is for existing files. Use the empty ctor for empty FTGenericTimeValue object.");

    m_dataselectors = FileType::getDataSelectors(_fileName);
//    DCOUT << '"' << m_dataselectors << '"' << std::endl;

    FTGenericTimeValue::constructor_internal(parent->gview());

    m_fileformat = fileformat;
    if(container==FileType::FCSDIF)
        m_fileformat = FFSDIF;
    else if(container==FileType::FCASCII && (fileformat==FFNotSpecified || fileformat==FFAutoDetect))
        m_fileformat = FFAsciiAutoDetect;
    else if(container==FileType::FCBINARY && (fileformat==FFNotSpecified || fileformat==FFAutoDetect))
        m_fileformat = FFBinaryAutoDetect;

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        load();
    }

    FTGenericTimeValue::constructor_external();
}

FTGenericTimeValue::FTGenericTimeValue(const FTGenericTimeValue& ft)
    : QObject(ft.parent())
    , FileType(FTGENTIMEVALUE, ft.fileFullPath, this)
{
    FTGenericTimeValue::constructor_internal(ft.m_view);

    ts = ft.ts;
    values = ft.values;
    m_values_min = ft.m_values_min;
    m_values_max = ft.m_values_max;

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    FTGenericTimeValue::constructor_external();
}

FileType* FTGenericTimeValue::duplicate(){
    return new FTGenericTimeValue(*this);
}

void FTGenericTimeValue::load(){

    if(m_fileformat==FFNotSpecified)
        m_fileformat = FFAutoDetect;

    // If not already specified, auto-detect the format
    #ifdef SUPPORT_SDIF
    if(m_fileformat==FFAutoDetect)
        if(FileType::isFileSDIF(fileFullPath))
            m_fileformat = FFSDIF;
    #endif
    // Check for text/ascii formats
    if(m_fileformat==FFAutoDetect || m_fileformat==FFAsciiAutoDetect || m_fileformat==FFBinaryAutoDetect){

        if(m_fileformat==FFAsciiAutoDetect || (m_fileformat!=FFAsciiAutoDetect && isFileASCII(fileFullPath))){
            // Find the format using grammar check

            std::ifstream fin(fileFullPath.toLatin1().constData());
            if(!fin.is_open())
                throw QString("FTGenericTimeValue:FFAutoDetect: Cannot open the file.");
            double t;
            string line, text;
            // Check the first line only (Assuming it is enough)
            if(!std::getline(fin, line))
                throw QString("FTGenericTimeValue:FFAutoDetect: There is not a single line in this file.");

            // Check: <number> <number>
            std::istringstream iss(line);
            if((iss >> t >> t) && iss.eof())
                m_fileformat = FFAsciiTimeValue;
            else{
                std::istringstream iss(line);
                if((iss >> t) && iss.eof())
                    m_fileformat = FFAsciiValue;
            }
        }
        else if(m_fileformat!=FFAsciiAutoDetect || m_fileformat==FFBinaryAutoDetect){
            // It should be a binary file

            // Guess the format from the file size in bytes
            // It should be multiple of 4 for float32 and multiple of 8 for double64
            QFileInfo fileinfo(fileFullPath);
            qint64 filesize = fileinfo.size();
            if(filesize%8==0)       m_fileformat=FFBinaryFloat64;
            else if(filesize%4==0)  m_fileformat=FFBinaryFloat32;
        }
    }

    if(m_fileformat==FFAutoDetect)
        throw QString("Cannot detect the file format of this generic time/value file");

    // Load the data given the format found or the one given
    if(m_fileformat==FFAsciiTimeValue){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTGenericTimeValue:FFAsciiTimeValue: Cannot open file");

        double t, value;
        m_values_min = +std::numeric_limits<double>::infinity();
        m_values_max = -std::numeric_limits<double>::infinity();
        string line, text;
        while(std::getline(fin, line)) {
            std::istringstream(line) >> t >> value;
            ts.push_back(t);
            if(!std::isinf(value)){
                m_values_min = std::min(m_values_min, value);
                m_values_max = std::max(m_values_max, value);
            }
            values.push_back(value);
        }
    }
    else if(m_fileformat==FFAsciiValue){
        ifstream fin(fileFullPath.toLatin1().constData());
        if(!fin.is_open())
            throw QString("FTGenericTimeValue:FFAsciiValue: Cannot open file");

        double t=0.0;
        double value;
        m_values_min = +std::numeric_limits<double>::infinity();
        m_values_max = -std::numeric_limits<double>::infinity();
        string line, text;
        while(std::getline(fin, line)) {
            std::istringstream(line) >> value;
            ts.push_back(t);
            if(!std::isinf(value)){
                m_values_min = std::min(m_values_min, value);
                m_values_max = std::max(m_values_max, value);
            }
            values.push_back(value);
            t += gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();
        }
    }
    else if(m_fileformat==FFBinaryFloat32){

        QFileInfo fileinfo(fileFullPath);
        qint64 filesize = fileinfo.size();
        if(filesize%4>0)    throw QString("FTGenericTimeValue:FFBinaryFloat32: The file size indicates that the file is not made of 32bits float binary values.");

        ifstream fin(fileFullPath.toLatin1().constData(), std::ifstream::binary);
        if(!fin.is_open())
            throw QString("FTGenericTimeValue:FFBinaryFloat32: Cannot open file");

        double t=0.0;
        float value; // TODO a definition that ensure sizeof(float)=32
        if(sizeof(value)!=4) throw QString("The float representation is not 32bits on this machine, the loaded data is likely to be wrong.");
        m_values_min = +std::numeric_limits<double>::infinity();
        m_values_max = -std::numeric_limits<double>::infinity();

        while (fin.read(reinterpret_cast<char*>(&value), sizeof(value))){
            ts.push_back(t);
            values.push_back(value);
            if(!std::isinf(value)){
                m_values_min = std::min(m_values_min, double(value));
                m_values_max = std::max(m_values_max, double(value));
            }
            t += gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();
        }
    }
    else if(m_fileformat==FFBinaryFloat64){
        QFileInfo fileinfo(fileFullPath);
        qint64 filesize = fileinfo.size();
        if(filesize%8>0)    throw QString("FTGenericTimeValue:FFBinaryFloat64: The file size indicates that the file is not made of 64bits float binary values.");

        ifstream fin(fileFullPath.toLatin1().constData(), std::ifstream::binary);
        if(!fin.is_open())
            throw QString("FTGenericTimeValue:FFBinaryFloat64: Cannot open file");

        double t=0.0;
        double value; // TODO a definition that ensure sizeof(float)=32
        if(sizeof(value)!=8) throw QString("The double representation is not 64bits on this machine, the loaded data is likely to be wrong.");
        m_values_min = +std::numeric_limits<double>::infinity();
        m_values_max = -std::numeric_limits<double>::infinity();

        while (fin.read(reinterpret_cast<char*>(&value), sizeof(value))){
            ts.push_back(t);
            values.push_back(value);
            if(!std::isinf(value)){
                m_values_min = std::min(m_values_min, value);
                m_values_max = std::max(m_values_max, value);
            }
            t += gMW->m_dlgSettings->ui->sbF0DefaultStepSize->value();
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

//        DCOUT << '"' << m_dataselectors << '"' << std::endl;
        if(!m_dataselectors.isEmpty())
            readentity.ChangeSelection(m_dataselectors.toLatin1().constData()); // Select directly the f0 values

        SDIFFrame frame;
        try{
            double value;
            m_values_min = +std::numeric_limits<double>::infinity();
            m_values_max = -std::numeric_limits<double>::infinity();
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
                    value = tmpMatrix.GetDouble(0, 0);
                    if(!std::isinf(value)){
                        m_values_min = std::min(m_values_min, value);
                        m_values_max = std::max(m_values_max, value);
                    }
                    values.push_back(value);
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

    updateStatistics();

    updateTextsGeometry();

    m_lastreadtime = QDateTime::currentDateTime();
    setStatus();
}

bool FTGenericTimeValue::reload() {
//    cout << "FTFZero::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return false;

    // Reset everything ...
    ts.clear();
    values.clear();

    // ... and reload the data from the file
    load();

    m_giGenericTimeValue->update();

    return true;
}

//void FTGenericTimeValue::saveAs() {
//    if(ts.size()==0){
//        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
//        return;
//    }

//    // Build the filter string
//    QString filters;
//    filters += s_formatstrings[FFAsciiTimeValue];
//    #ifdef SUPPORT_SDIF
//        filters += ";;"+s_formatstrings[FFSDIF];
//    #endif
//    QString selectedFilter;
//    if(m_fileformat==FFNotSpecified) {
//        if(gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue<int(s_formatstrings.size()))
//            selectedFilter = s_formatstrings[gMW->m_dlgSettings->ui->cbF0DefaultFormat->currentIndex()+FFAsciiTimeValue];
//    }
//    else
//        selectedFilter = s_formatstrings[m_fileformat];

////    COUTD << fileFullPath.toLatin1().constData() << endl;

//    QString fp = QFileDialog::getSaveFileName(gMW, "Save F0 file as...", fileFullPath, filters, &selectedFilter, QFileDialog::DontUseNativeDialog);

//    if(!fp.isEmpty()){
//        try{
//            setFullPath(fp);
//            if(selectedFilter==s_formatstrings[FFAsciiTimeValue])
//                m_fileformat = FFAsciiTimeValue;
//            #ifdef SUPPORT_SDIF
//            else if(selectedFilter==s_formatstrings[FFSDIF])
//                m_fileformat = FFSDIF;
//            #endif

//            if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
//                m_fileformat = FFAsciiTimeValue;

//            save();
//        }
//        catch(QString &e) {
//            QMessageBox::critical(NULL, "Cannot save under this file path.", e);
//        }
//    }
//}

//void FTGenericTimeValue::save() {
//    if(ts.size()==0){
//        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
//        return;
//    }

//    if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
//        m_fileformat = FFAsciiTimeValue;

//    if(m_fileformat==FFAsciiTimeValue){
//        QFile data(fileFullPath);
//        if(!data.open(QFile::WriteOnly))
//            throw QString("FTZero: Cannot open file");

//        QTextStream stream(&data);
//        stream.setRealNumberPrecision(12);
//        stream.setRealNumberNotation(QTextStream::ScientificNotation);
//        stream.setCodec("ASCII");
//        for(size_t li=0; li<ts.size(); li++)
//            stream << ts[li] << " " << f0s[li] << endl;
//    }
//    else if(m_fileformat==FFSDIF){
//        #ifdef SUPPORT_SDIF
//            SdifFileT* filew = SdifFOpen(fileFullPath.toLatin1().constData(), eWriteFile);

//            if (!filew)
//                throw QString("SDIF: Cannot save the data in the specified file (permission denied?)");

//            size_t generalHeaderw = SdifFWriteGeneralHeader(filew);
//            Q_UNUSED(generalHeaderw)
//            size_t asciiChunksw = SdifFWriteAllASCIIChunks(filew);
//            Q_UNUSED(asciiChunksw)

//            // Save information
//            SDIFFrame frameToWrite;
//            /*set the header of the frame*/
//            frameToWrite.SetStreamID(0); // TODO Ok ??
//            frameToWrite.SetSignature("1NVT");
//            SDIFMatrix tmpMatrix("1NVT");
//            QString info = "";
//            info += "SampleRate\t"+QString::number(gFL->getFs())+"\n";
//            info += "NumChannels\t"+QString::number(1)+"\n";
//            if(gFL->hasFile(m_src_snd))
//                info += "Soundfile\t"+m_src_snd->fileFullPath+"\n";
//            info += "Version\t"+gMW->version().mid(8)+"\n";
//            info += "Creator\tDFasma\n";
//            tmpMatrix.Set(info.toLatin1().constData());
//            frameToWrite.AddMatrix(tmpMatrix);
//            frameToWrite.Write(filew);

//            for(size_t li=0; li<ts.size(); li++) {
//                // cout << labels[li].toLatin1().constData() << ": " << starts[li] << ":" << ends[li] << endl;

//                // Prepare the frame
//                SDIFFrame frameToWrite;
//                /*set the header of the frame*/
//                frameToWrite.SetStreamID(0); // TODO Ok ??
//                frameToWrite.SetTime(ts[li]);
//                frameToWrite.SetSignature("1FQ0");

//                // Fill the matrix
//                SDIFMatrix tmpMatrix("1FQ0", 1, 1);
//                // SDIFMatrix tmpMatrix("1FQ0", 1, 4);
//                tmpMatrix.Set(0, 0, f0s[li]); // Frequency
//                // tmpMatrix.Set(0, 1, 1.0);  // Confidence
//                // tmpMatrix.Set(0, 2, 1.0);  // Score
//                // tmpMatrix.Set(0, 3, 1.0);  // RealAmplitude
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

//        #else
//            throw QString("SDIF file support is not compiled in this distribution of DFasma.");
//        #endif
//    }
//    else
//        throw QString("File format not recognized for writing this F0 file.");

//    m_lastreadtime = QDateTime::currentDateTime();
//    m_is_edited = false;
//    checkFileStatus(CFSMEXCEPTION);
//    gFL->fileInfoUpdate();
//    gMW->statusBar()->showMessage(fileFullPath+" saved.", 3000);
//}

void FTGenericTimeValue::updateStatistics(){
    m_meandts = 0.0;
    m_meanvalue = 0.0;
    int nbnoninfvalues = 0;
    m_valuemin = std::numeric_limits<double>::infinity();
    m_valuemax = -std::numeric_limits<double>::infinity();
//        DCOUT << ts.size() << " " << values.size() << std::endl;
    for(size_t i=0; i<ts.size(); ++i){
        if(i>0)
            m_meandts += ts[i]-ts[i-1];
        double value = values[i];
        m_valuemin = std::min(m_valuemin, value);
        m_valuemax = std::max(m_valuemax, value);
        if(!std::isinf(value)){
            m_meanvalue += value;
            nbnoninfvalues++;
        }
    }
    m_meandts /= ts.size();
    m_meanvalue /= nbnoninfvalues;

    gFL->fileInfoUpdate();
}

QString FTGenericTimeValue::info() const {
    QString str = FileType::info();
    str += "Number of values: " + QString::number(ts.size()) + "<br/>";

    str += "Format: " + s_formatstrings[m_fileformat].remove(QRegExp("\\(.+\\)")) + "<br/>";

    if(ts.size()>0){
        str += "Average sampling: " + QString("%1").arg(m_meandts, 0,'f',gMW->m_dlgSettings->ui->sbViewsTimeDecimals->value()) + "s<br/>";
        str += QString("Values in [%1, %2]").arg(m_valuemin, 0,'g',3).arg(m_valuemax, 0,'g',5);
        str += QString("<br/>Mean Value=%1").arg(m_meanvalue, 0,'g',5);

        if(!m_dataselectors.isEmpty())
            str += QString("<br/>Data selector: ")+m_dataselectors;
    }
    return str;
}

void FTGenericTimeValue::fillContextMenu(QMenu& contextmenu) {
    FileType::fillContextMenu(contextmenu);

    contextmenu.setTitle("F0");
}

void FTGenericTimeValue::updateTextsGeometry(){
    if(!m_actionShow->isChecked())
        return;

//    QRectF aspec_viewrect = gMW->m_gvSpectrumAmplitude->mapToScene(gMW->m_gvSpectrumAmplitude->viewport()->rect()).boundingRect();
//    QTransform aspec_trans = gMW->m_gvSpectrumAmplitude->transform();

//    QTransform mat1;
//    mat1.translate(1.0/aspec_trans.m11(), aspec_viewrect.top()+10/aspec_trans.m22());
//    mat1.scale(1.0/aspec_trans.m11(), 1.0/aspec_trans.m22());
//    m_aspec_txt->setTransform(mat1);
}
void FTGenericTimeValue::setVisible(bool shown){
    FileType::setVisible(shown);

    if(shown)
        updateTextsGeometry();

//    m_aspec_txt->setVisible(shown);
    m_giGenericTimeValue->setVisible(shown);
}

void FTGenericTimeValue::setColor(const QColor& color){
    if(color==getColor())
        return;

    FileType::setColor(color);

    QPen pen(getColor());
    pen.setWidth(0);
    QBrush brush(getColor());

//    m_aspec_txt->setPen(pen);
//    m_aspec_txt->setBrush(brush);

    pen = m_giGenericTimeValue->getPen();
    pen.setColor(color);
    m_giGenericTimeValue->setPen(pen);
}

void FTGenericTimeValue::zposReset(){
    m_giGenericTimeValue->setZValue(0.0);
}

void FTGenericTimeValue::zposBringForward(){
    m_giGenericTimeValue->setZValue(1.0);
}

double FTGenericTimeValue::getLastSampleTime() const {
    if(ts.empty())
        return 0.0;
    else
        return *((ts.end()-1));
}

//void FTGenericTimeValue::edit(double t, double f0){
//    if(f0<0)
//        f0 = 0.0;

////    COUTD << "FTFZero::edit " << t << " " << f0 << endl;

//    double step = qae::median(qae::diff(ts)); // TODO Speed up: Pre-compute

//    std::vector<double>::iterator itlb = std::lower_bound(ts.begin(), ts.end(), t+step/2);
//    int ri = itlb-ts.begin()-1;

//    if(ri>=0 && ri<int(ts.size())){
//        f0s[ri] = f0;
//        if(m_giF0ForSpectrogram)
//            m_giF0ForSpectrogram->updateGeometry();
//    }

//    m_is_edited = true;
//    setStatus();
//}

FTGenericTimeValue::~FTGenericTimeValue() {

    delete m_giGenericTimeValue;
//    delete m_aspec_txt;

    if(m_view){
        m_view->m_ftgenerictimevalues.erase(std::find(gview()->m_ftgenerictimevalues.begin(), gview()->m_ftgenerictimevalues.end(), this));
        gview()->updateSceneRect(); // This one is light and can be called often
    }

    gFL->ftgenerictimevalues.erase(std::find(gFL->ftgenerictimevalues.begin(), gFL->ftgenerictimevalues.end(), this));
}
