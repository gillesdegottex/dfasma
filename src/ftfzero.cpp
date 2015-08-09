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
#include <qmath.h>
#include <qendian.h>

#include "wmainwindow.h"
#include "ui_wdialogsettings.h"
#include "gvamplitudespectrum.h"

#include "qthelper.h"

void FTFZero::init(){
    m_fileformat = FFNotSpecified;
    m_aspec_txt = new QGraphicsSimpleTextItem("unset");
    gMW->m_gvAmplitudeSpectrum->m_scene->addItem(m_aspec_txt);
    setColor(color);
}

FTFZero::FTFZero(const QString& _fileName, QObject* parent, FileType::FileContainer container, FileFormat fileformat)
    : FileType(FTFZERO, _fileName, this)
{
    Q_UNUSED(parent)

    if(fileFullPath.isEmpty())
        throw QString("This ctor is for existing files. Use the empty ctor for empty f0 object.");

    init();

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
    init();

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
        throw QString("Cannot detect the file format of this f0 file");

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
                        throw QString("f0 value is missing in a 1FQ0 frame at time ")+QString::number(t);

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
        throw QString("File format not recognized for loading this f0 file.");

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

QString FTFZero::info() const {
    QString str = FileType::info();
    str += "Number of f0 values: " + QString::number(ts.size()) + "<br/>";
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
        str += QString("<br/>Mean f0=%3Hz").arg(meanf0, 0,'g',5);
    }
    return str;
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

void FTFZero::setColor(const QColor& _color) {
    FileType::setColor(_color);

    QPen pen(color);
    pen.setWidth(0);
    QBrush brush(color);

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
}


// Analysis --------------------------------------------------------------------

#include "external/REAPER/epoch_tracker/epoch_tracker.h"

FTFZero::FTFZero(FTSound *ftsnd, double f0min, double f0max, QObject *parent)
    : FileType(FTFZERO, ftsnd->fileFullPath+".f0.txt", this)
{
    Q_UNUSED(parent);

    init();

//    if(_fileName==""){
////        if(gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText==FFSDIF)
////            setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");
////        m_fileformat = FFSDIF;
////        else
//        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.txt");
//        m_fileformat = FFAsciiTimeValue;
//    }

    // Compute the f0 from the given sound file

    double timestepsize = gMW->m_dlgSettings->ui->sbEstimationStepSize->value();

    EpochTracker et; // TODO to put in FTSound

    // Initialize with the given input
    // Start with a dirty copy in the necessary format
    vector<int16_t> data(ftsnd->wav.size());
    for(size_t i=0; i<ftsnd->wav.size(); ++i)
        data[i] = 32768*ftsnd->wav[i];
    int16_t* wave_datap = data.data();
    int32_t n_samples = ftsnd->wav.size();
    float sample_rate = gFL->getFs();
    if (!et.Init(wave_datap, n_samples, sample_rate,
          f0min, f0max, true, true))
        throw QString("EpochTracker initialisation failed");

    // Compute f0 and pitchmarks.
    if (!et.ComputeFeatures())
        throw QString("Failed to compute features");

    if (!et.TrackEpochs())
        throw QString("Failed to track epochs");

    std::vector<float> f0; // TODO Drop this temporary variable
    std::vector<float> corr;
    if (!et.ResampleAndReturnResults(timestepsize, &f0, &corr))
        throw QString("Cannot resample the results");

    // Everything seemed to go well, let's built the new file
    for (size_t i = 0; i<f0.size(); ++i) {
        ts.push_back(timestepsize*i);
        f0s.push_back(f0[i]);
    }

    updateTextsGeometry();
}
