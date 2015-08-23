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

#include <qmath.h>
#include <qendian.h>
#include <Qt>
#include <QKeyEvent>
#include <QMenu>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsObject>
#include <QGraphicsTextItem>
#include <QTextCursor>
#include <QTextStream>
#include <QTextCodec>

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ui_wdialogsettings.h"
#include "gvwaveform.h"
#include "gvspectrogram.h"
#include "qthelper.h"


bool FTGraphicsLabelItem::s_isEditing = false;

FTGraphicsLabelItem::FTGraphicsLabelItem(FTLabels* ftl, const QString & text)
    : QGraphicsTextItem(text)
    , m_ftl(ftl)
{
}

FTGraphicsLabelItem::FTGraphicsLabelItem(FTGraphicsLabelItem* ftgi, FTLabels* ftl)
    : QGraphicsTextItem(ftgi->toPlainText())
    , m_ftl(ftl)
{
}

void FTGraphicsLabelItem::focusInEvent(QFocusEvent * event){
    Q_UNUSED(event)
//    COUTD << "FTGraphicsLabelItem::focusInEvent " << endl;
    m_prevText = toPlainText();
    s_isEditing = true;
    setTextCursor(QTextCursor(document()));
    update();
}
void FTGraphicsLabelItem::focusOutEvent(QFocusEvent * event){
    Q_UNUSED(event)
//    COUTD << "FTGraphicsLabelItem::focusOutEvent " << endl;
    s_isEditing = false;

    // Replace the text in the spectrogram
    std::deque<double>::iterator p = std::find(m_ftl->starts.begin(), m_ftl->starts.end(), pos().x());
    if(p!=m_ftl->starts.end()){
        int index = std::distance(m_ftl->starts.begin(), p);
        m_ftl->spectrogram_labels[index]->setText(toPlainText());
    }
    else
        qWarning("FTGraphicsLabelItem::focusOutEvent Cannot replace the text in the spectrogram");

    setTextCursor(QTextCursor());
    update();
}
void FTGraphicsLabelItem::keyPressEvent(QKeyEvent * event){
//    COUTD << "FTGraphicsLabelItem::keyPressEvent " << event->key() << " enter=" << Qt::Key_Enter << endl;

    if(event->key()==Qt::Key_Escape) {
        setPlainText(m_prevText);
        clearFocus();
    }
    else if((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)) {
        clearFocus();
    }
    else
        QGraphicsTextItem::keyPressEvent(event);
}

void FTGraphicsLabelItem::paint(QPainter *painter,const QStyleOptionGraphicsItem *option,QWidget *widget){
    painter->setPen(m_ftl->getColor());
    QRectF br = boundingRect();
    br.adjust(+2, +2, -2, -2);
    painter->drawRect(br);
    QGraphicsTextItem::paint(painter, option, widget);
}

std::deque<QString> FTLabels::s_formatstrings;
FTLabels::ClassConstructor::ClassConstructor(){
    // Attention ! It has to correspond to FType definition's order.
    // Map Labels FileFormat with corresponding strings
    if(FTLabels::s_formatstrings.empty()){
        FTLabels::s_formatstrings.push_back("Unspecified");
        FTLabels::s_formatstrings.push_back("Auto");
        FTLabels::s_formatstrings.push_back("Text - Auto");
        FTLabels::s_formatstrings.push_back("Text - Time Text (*.lab)");
        FTLabels::s_formatstrings.push_back("Text - Segment [float] (*.lab)");
        FTLabels::s_formatstrings.push_back("Text - Segment [samples] (*.lab)");
        FTLabels::s_formatstrings.push_back("Text - HTK Label File (*.lab)");
        FTLabels::s_formatstrings.push_back("SDIF - 1MRK/1LAB (*.sdif)");
    }
}
FTLabels::ClassConstructor FTLabels::s_class_constructor;

void FTLabels::constructor_internal(){
    m_fileformat = FFNotSpecified;

    m_actionSave = new QAction("Save", this);
    m_actionSave->setStatusTip(tr("Save the labels times (overwrite the file !)"));
    m_actionSave->setShortcut(gMW->ui->actionSelectedFilesSave->shortcut());
    connect(m_actionSave, SIGNAL(triggered()), this, SLOT(save()));
    m_actionSaveAs = new QAction("Save as...", this);
    m_actionSaveAs->setStatusTip(tr("Save the labels times in a given file..."));
    connect(m_actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));
}

void FTLabels::constructor_external(){
    FileType::constructor_external();

    gFL->ftlabels.push_back(this);
}

// Construct an empty label object
FTLabels::FTLabels(QObject *parent)
    : QObject(parent)
    , FileType(FTLABELS, "", this)
{
    Q_UNUSED(parent);

    FTLabels::constructor_internal();

    if(gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText==FFSDIF)
        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.sdif");
    else
        setFullPath(QDir::currentPath()+QDir::separator()+"unnamed.lab");
    m_fileformat = FFNotSpecified;

    FTLabels::constructor_external();
}

// Construct from an existing file name
FTLabels::FTLabels(const QString& _fileName, QObject* parent, FileType::FileContainer container, FileFormat fileformat)
    : QObject(parent)
    , FileType(FTLABELS, _fileName, this)
{
    Q_UNUSED(parent);
//    COUTD << "FTLabels::FTLabels " << _fileName.toLatin1().constData() << endl;

    if(fileFullPath.isEmpty())
        throw QString("This ctor is for existing files. Use the empty ctor for empty label object.");

    FTLabels::constructor_internal();

    m_fileformat = fileformat;
    if(container==FileType::FCSDIF)
        m_fileformat = FFSDIF;
    else if(container==FileType::FCASCII)
        m_fileformat = FFTEXTAutoDetect;

    checkFileStatus(CFSMEXCEPTION);
    load();

    FTLabels::constructor_external();
}

// Copy constructor
FTLabels::FTLabels(const FTLabels& ft)
    : QObject(ft.parent())
    , FileType(FTLABELS, ft.fileFullPath, this)
{
    FTLabels::constructor_internal();

    int starti=0;
    for(std::deque<FTGraphicsLabelItem*>::const_iterator it=ft.waveform_labels.begin(); it!=ft.waveform_labels.end(); ++it, ++starti)
        addLabel(ft.starts[starti], (*it)->toPlainText());

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    updateTextsGeometry();

    FTLabels::constructor_external();
}

FileType* FTLabels::duplicate(){
    return new FTLabels(*this);
}

void FTLabels::load() {
//    COUTD << "FTLabels::load " << m_fileformat << " m_fileformat=" << m_fileformat << endl;

    clear(); // First, ensure there is no leftover

    if(m_fileformat==FFNotSpecified)
        m_fileformat = FFAutoDetect;

    #ifdef SUPPORT_SDIF
    if(m_fileformat==FFAutoDetect)
        if(FileType::isFileSDIF(fileFullPath))
            m_fileformat = FFSDIF;
    #endif

    // Check for text formats
    if(m_fileformat==FFAutoDetect || m_fileformat==FFTEXTAutoDetect) {
        // Find the format using language check

        QFile data(fileFullPath);
        if(!data.open(QFile::ReadOnly))
            throw QString("FTLabel:FFAutoDetect: Cannot open the file.");

        double t;
        QString line, text;
        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        line = stream.readLine();

        // Check the first line only (Assuming it is enough ...)
        if(line.isNull() || line.isEmpty())
            throw QString("FTLabel:FFAutoDetect: There is not a single line in this file.");

//        COUTD << '"' << line.toLatin1().constData() << '"' << endl;

        // Check: <number> <text>
        QTextStream linestr(&line);
        linestr >> t >> text;
        if(linestr.atEnd())
            m_fileformat = FFTEXTTimeText;
        else {
            // Check simple HTK Label: <integer> <integer> <text>
            // No multiple levels or multiple alternatives managed
            // http://www.ee.columbia.edu/ln/LabROSA/doc/HTKBook21/node82.html
            int i;
            QTextStream linestr(&line);
            linestr >> i >> i >> text;
            if(linestr.atEnd()){
                QRegExp rx(".*[0-9]+$"); // If the extension ends with a number...
                if(rx.indexIn(fileFullPath)!=-1)
                    m_fileformat = FFTEXTSegmentsSample; // ... it is samples
                else
                    m_fileformat = FFTEXTSegmentsHTK;     // ... otherwise it is 100[ns]
            }
            else {
                // Check: <number> <number> <text>
                QTextStream linestr(&line);
                linestr >> t >> t >> text;
                if(linestr.atEnd()){
                    m_fileformat = FFTEXTSegmentsFloat;
                }
            }
        }
    }

//    COUTD << "Detected format=" << m_fileformat << endl;

    if(m_fileformat==FFAutoDetect)
        throw QString("Cannot detect the file format of this label file");

    // Load the data given the format found or the one given
    if(m_fileformat==FFTEXTTimeText){
        QFile data(fileFullPath);
        if(!data.open(QFile::ReadOnly))
            throw QString("FTLabel: Cannot open file");

        double t;
        QString line, text;
        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        line = stream.readLine();
        do {
            QTextStream(&line) >> t >> text;
            addLabel(t, text);

            line = stream.readLine();
        } while (!line.isNull());
    }
    else if(m_fileformat==FFTEXTSegmentsFloat){
        QFile data(fileFullPath);
        if(!data.open(QFile::ReadOnly))
            throw QString("FTLabel: Cannot open file");

        double startt, endt;
        QString line, text;
        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        line = stream.readLine();
        do {
            QTextStream(&line) >> startt >> endt >> text;
            addLabel(startt, text);

            line = stream.readLine();
        } while (!line.isNull());

        if(text.size()>0 && (char)(text.toLatin1()[0])!=char(31))
            addLabel(endt, "");
    }
    else if(m_fileformat==FFTEXTSegmentsSample){
        QFile data(fileFullPath);
        if(!data.open(QFile::ReadOnly))
            throw QString("FTLabel: Cannot open file");

        double fs = gFL->getFs(); // Use the sampling frequency from the loaded files
        double startt, endt;
        QString line, text;
        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        line = stream.readLine();
        do {
//                COUTD << '"' << line << '"' << endl;
            QTextStream(&line) >> startt >> endt >> text;
            startt /= fs;
            endt /= fs;
//                COUTD << startt << " " << '"' << text << '"' << endl;
            addLabel(startt, text);

            line = stream.readLine();
        } while (!line.isNull());

//            QByteArray ba = text.toLatin1();
//            for(size_t ci=0; ci<ba.size(); ++ci)
//                std::cout << int((unsigned char)(ba[ci])) << std::endl;

        if(text.size()>0 && (char)(text.toLatin1()[0])!=char(31))
            addLabel(endt, "");
    }
    else if(m_fileformat==FFTEXTSegmentsHTK){
        QFile data(fileFullPath);
        if(!data.open(QFile::ReadOnly))
            throw QString("FTLabel: Cannot open file");

        double startt, endt;
        QString line, text;
        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        line = stream.readLine();
        do {
            QTextStream(&line) >> startt >> endt >> text;
            startt *= 1e-7;
            endt *= 1e-7;
            addLabel(startt, text);

            line = stream.readLine();
        } while (!line.isNull());

        if(text.size()>0 && (char)(text.toLatin1()[0])!=char(31))
            addLabel(endt, "");
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

    updateTextsGeometry();

    m_lastreadtime = QDateTime::currentDateTime();
    m_is_edited = false;
    setStatus();
}

void FTLabels::clear() {
//    COUTD << "FTLabels::clear" << endl;
    if(getNbLabels()>0) {
        starts.clear();
        for(size_t li=0; li<waveform_labels.size(); li++)
            delete waveform_labels[li];
        waveform_labels.clear();
        for(size_t li=0; li<spectrogram_labels.size(); li++)
            delete spectrogram_labels[li];
        spectrogram_labels.clear();
        for(size_t li=0; li<waveform_lines.size(); li++)
            delete waveform_lines[li];
        waveform_lines.clear();
        for(size_t li=0; li<spectrogram_lines.size(); li++)
            delete spectrogram_lines[li];
        spectrogram_lines.clear();

        m_is_edited = true;
        setStatus();
    }
//    COUTD << "FTLabels::~clear" << endl;
}

bool FTLabels::reload() {
    // cout << "FTLabels::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return false;

    // Reload the data from the file
    load();

    return true;
}

void FTLabels::saveAs() {
    if(starts.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    // Build the filter string
    QString filters;
    filters += s_formatstrings[FFTEXTTimeText];
    filters += ";;"+s_formatstrings[FFTEXTSegmentsFloat];
    filters += ";;"+s_formatstrings[FFTEXTSegmentsSample];
    filters += ";;"+s_formatstrings[FFTEXTSegmentsHTK];
    #ifdef SUPPORT_SDIF
        filters += ";;"+s_formatstrings[FFSDIF];
    #endif
    QString selectedFilter;
    if(m_fileformat==FFNotSpecified) {
        if(gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText<int(s_formatstrings.size()))
            selectedFilter = s_formatstrings[gMW->m_dlgSettings->ui->cbLabelsDefaultFormat->currentIndex()+FFTEXTTimeText];
    }
    else
        selectedFilter = s_formatstrings[m_fileformat];

//    COUTD << fileFullPath.toLatin1().constData() << endl;

    QString fp = QFileDialog::getSaveFileName(gMW, "Save label file as...", fileFullPath, filters, &selectedFilter, QFileDialog::DontUseNativeDialog);

    if(!fp.isEmpty()){
        try{
            setFullPath(fp);
            if(selectedFilter==s_formatstrings[FFTEXTTimeText])
                m_fileformat = FFTEXTTimeText;
            else if(selectedFilter==s_formatstrings[FFTEXTSegmentsFloat])
                m_fileformat = FFTEXTSegmentsFloat;
            else if(selectedFilter==s_formatstrings[FFTEXTSegmentsSample])
                m_fileformat = FFTEXTSegmentsSample;
            else if(selectedFilter==s_formatstrings[FFTEXTSegmentsHTK])
                m_fileformat = FFTEXTSegmentsHTK;
            #ifdef SUPPORT_SDIF
            else if(selectedFilter==s_formatstrings[FFSDIF])
                m_fileformat = FFSDIF;
            #endif

            if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
                m_fileformat = FFTEXTTimeText;

            save();
        }
        catch(QString &e) {
            QMessageBox::critical(NULL, "Cannot save under this file path.", e);
        }
    }
}

void FTLabels::save() {
    if(starts.size()==0){
        QMessageBox::warning(NULL, "Nothing to save!", "There is no content to save from this file. No file will be saved.");
        return;
    }

    sort(); // In the file, we want the labels' time in ascending order

    if(m_fileformat==FFNotSpecified || m_fileformat==FFAutoDetect)
        m_fileformat = FFTEXTTimeText;

    if(m_fileformat==FFTEXTTimeText){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTLabel: Cannot open file");

        QTextStream stream(&data);
        stream.setRealNumberPrecision(12);
        stream.setRealNumberNotation(QTextStream::ScientificNotation);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++)
            stream << starts[li] << " " << waveform_labels[li]->toPlainText() << endl;
    }
    else if(m_fileformat==FFTEXTSegmentsFloat){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTLabel: Cannot open file");

        QTextStream stream(&data);
        stream.setRealNumberPrecision(12);
        stream.setRealNumberNotation(QTextStream::ScientificNotation);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++) {
            // If this is the last one AND its text is empty, skip it.
            // (thus, manage the read/write of segments files)
            if(li==starts.size()-1 && waveform_labels[li]->toPlainText()=="")
                continue;

            double last = starts[li] + 1.0/gFL->getFs(); // If not end, add a single sample to start
            if(li<starts.size()-1)
                last = starts[li+1]; // Otherwise use next start, if not the last label
            else {
                if(gFL->ftsnds.size()>0)
                    last = gFL->getCurrentFTSound(true)->getLastSampleTime(); // Or last wav's sample time
            }
            stream << starts[li] << " " << last << " " << waveform_labels[li]->toPlainText() << endl;
        }
    }
    else if(m_fileformat==FFTEXTSegmentsSample){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTLabel: Cannot open file");

        double fs = gFL->getFs();

        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++) {
            // If this is the last one AND its text is empty, skip it.
            // (thus, manage the read/write of segments files)
            if(li==starts.size()-1 && waveform_labels[li]->toPlainText()=="")
                continue;

            double last = starts[li] + 1.0/gFL->getFs(); // If not end, add a single sample to start
            if(li<starts.size()-1)
                last = starts[li+1]; // Otherwise use next start, if not the last label
            else {
                if(gFL->ftsnds.size()>0)
                    last = gFL->getCurrentFTSound(true)->getLastSampleTime(); // Or last wav's sample time
            }
            stream << int(fs*starts[li]) << " " << int(fs*last) << " " << waveform_labels[li]->toPlainText() << endl;
        }
    }
    else if(m_fileformat==FFTEXTSegmentsHTK){
        QFile data(fileFullPath);
        if(!data.open(QFile::WriteOnly))
            throw QString("FTLabel: Cannot open file");

        QTextStream stream(&data);
        stream.setCodec(gMW->m_dlgSettings->ui->cbLabelsDefaultTextEncoding->currentText().toLatin1().constData());
        for(size_t li=0; li<starts.size(); li++) {
            // If this is the last one AND its text is empty, skip it.
            // (thus, manage the read/write of segments files)
            if(li==starts.size()-1 && waveform_labels[li]->toPlainText()=="")
                continue;

            double last = starts[li] + 1.0/gFL->getFs(); // If not end, add a single sample to start
            if(li<starts.size()-1)
                last = starts[li+1]; // Otherwise use next start, if not the last label
            else {
                if(gFL->ftsnds.size()>0)
                    last = gFL->getCurrentFTSound(true)->getLastSampleTime(); // Or last wav's sample time
            }
            stream << int(0.5+1e7*starts[li]) << " " << int(0.5+1e7*last) << " " << waveform_labels[li]->toPlainText() << endl;
        }
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
                tmpMatrix.Set(std::string(waveform_labels[li]->toPlainText().toLatin1().constData()));
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
        throw QString("File format not recognized for writing this label file.");

    m_lastreadtime = QDateTime::currentDateTime();
    m_is_edited = false;
    checkFileStatus(CFSMEXCEPTION);
    gFL->fileInfoUpdate();
    gMW->statusBar()->showMessage(fileFullPath+" saved.", 10000);
}


QString FTLabels::info() const {
    QString str = FileType::info();
    str += "Number of labels: " + QString::number(starts.size()) + "<br/>";
    return str;
}

void FTLabels::fillContextMenu(QMenu& contextmenu) {
    FileType::fillContextMenu(contextmenu);

    contextmenu.setTitle("Labels");

    contextmenu.addAction(m_actionSave);
    contextmenu.addAction(m_actionSaveAs);
}

double FTLabels::getLastSampleTime() const {

    if(starts.empty())
        return 0.0;
    else
        return starts.back();
}

void FTLabels::updateTextsGeometry(){

    if(!m_actionShow->isChecked())
        return;

    QRectF waveform_viewrect = gMW->m_gvWaveform->mapToScene(gMW->m_gvWaveform->viewport()->rect()).boundingRect();
    QTransform waveform_trans = gMW->m_gvWaveform->transform();

    QRectF spectrogram_viewrect = gMW->m_gvSpectrogram->mapToScene(gMW->m_gvSpectrogram->viewport()->rect()).boundingRect();
    QTransform spectrogram_trans = gMW->m_gvSpectrogram->transform();

    for(size_t u=0; u<starts.size(); ++u){

        double x = 0.0;
        if(starts[u]>gFL->getMaxLastSampleTime()-24.0/waveform_trans.m11())
            x = -(waveform_labels[u]->boundingRect().width()-4)/waveform_trans.m11();

        QTransform mat1;
        mat1.translate(x-2.0/waveform_trans.m11(), waveform_viewrect.top()+10/waveform_trans.m22());
        mat1.scale(1.0/waveform_trans.m11(), 1.0/waveform_trans.m22());
        waveform_labels[u]->setTransform(mat1);

        QTransform mat2;
        mat2.translate(x+2.0/spectrogram_trans.m11(), spectrogram_viewrect.top()+10/spectrogram_trans.m22());
        mat2.scale(1.0/spectrogram_trans.m11(), 1.0/spectrogram_trans.m22());
        spectrogram_labels[u]->setTransform(mat2);
    }
}

void FTLabels::addLabel(double position, const QString& text){

    QPen pen(getColor());
    pen.setWidth(0);
    QBrush brush(getColor());
    QPen whitepen(Qt::white);
    whitepen.setWidth(1);

    starts.push_back(position);

    waveform_labels.push_back(new FTGraphicsLabelItem(this, text));
    waveform_labels.back()->setPos(position, 0);
    waveform_labels.back()->setDefaultTextColor(getColor());
    if(gMW->ui->actionEditMode->isChecked())
        waveform_labels.back()->setTextInteractionFlags(Qt::TextEditable);
    else
        waveform_labels.back()->setTextInteractionFlags(Qt::NoTextInteraction);
    gMW->m_gvWaveform->m_scene->addItem(waveform_labels.back());

    spectrogram_labels.push_back(new QGraphicsSimpleTextItem(text));
    spectrogram_labels.back()->setPos(position, 0);
    spectrogram_labels.back()->setBrush(brush);
    spectrogram_labels.back()->setPen(whitepen);
    gMW->m_gvSpectrogram->m_scene->addItem(spectrogram_labels.back());
    // TODO set Brush and pen for the outline!

    waveform_lines.push_back(new QGraphicsLineItem(0, -1, 0, 1));
    waveform_lines.back()->setPos(position, 0);
    waveform_lines.back()->setPen(pen);
    gMW->m_gvWaveform->m_scene->addItem(waveform_lines.back());

    spectrogram_lines.push_back(new QGraphicsLineItem(0, 0, 0, gFL->getFs()));
    spectrogram_lines.back()->setPos(position, 0);
    spectrogram_lines.back()->setPen(pen);
    gMW->m_gvSpectrogram->m_scene->addItem(spectrogram_lines.back());    

    sort();

    m_is_edited = true;
    setStatus();
}

void FTLabels::moveLabel(int index, double position){
    starts[index] = position;
    waveform_lines[index]->setPos(position, 0);
    waveform_labels[index]->setPos(position, 0);
    spectrogram_lines[index]->setPos(position, 0);
    spectrogram_labels[index]->setPos(position, 0);

    sort();

    m_is_edited = true;
    setStatus();
}
void FTLabels::moveAllLabel(double delay){
//    COUTD << "FTLabels::moveAllLabel " << delay << endl;
    for(size_t u=0; u<starts.size(); ++u){
        starts[u] += delay;
        waveform_lines[u]->setPos(waveform_lines[u]->pos().x()+delay, 0);
        waveform_labels[u]->setPos(waveform_labels[u]->pos().x()+delay, 0);
        spectrogram_lines[u]->setPos(spectrogram_lines[u]->pos().x()+delay, 0);
        spectrogram_labels[u]->setPos(spectrogram_labels[u]->pos().x()+delay, 0);
    }
    m_is_edited = true;
    setStatus();
}


void FTLabels::changeText(int index, const QString& text){
    waveform_labels[index]->setPlainText(text);
    spectrogram_labels[index]->setText(text);

    m_is_edited = true;
    setStatus();
}

void FTLabels::setVisible(bool shown){
//    cout << "FTLabels::setVisible" << endl;
    FileType::setVisible(shown);

    if(shown)
        updateTextsGeometry();

    for(size_t u=0; u<starts.size(); ++u){
        waveform_labels[u]->setVisible(shown);
        spectrogram_labels[u]->setVisible(shown);
        waveform_lines[u]->setVisible(shown);
        spectrogram_lines[u]->setVisible(shown);
    }
}
void FTLabels::setColor(const QColor& color) {
    FileType::setColor(color);

    QPen pen(getColor());
    pen.setWidth(0);
    QBrush brush(getColor());
    QPen whitepen(Qt::white);
    whitepen.setWidth(1);

    for(size_t u=0; u<starts.size(); ++u){
        waveform_labels[u]->setDefaultTextColor(getColor());
        spectrogram_labels[u]->setBrush(brush);
        waveform_lines[u]->setPen(pen);
        spectrogram_lines[u]->setPen(pen);
    }
}

void FTLabels::removeLabel(int index){

    starts.erase(starts.begin()+index);

    QGraphicsTextItem* labeltoremove = *(waveform_labels.begin()+index);
    waveform_labels.erase(waveform_labels.begin()+index);
    delete labeltoremove;
    QGraphicsSimpleTextItem* labelsimpletoremove = *(spectrogram_labels.begin()+index);
    spectrogram_labels.erase(spectrogram_labels.begin()+index);
    delete labelsimpletoremove;

    QGraphicsLineItem* linetoremove = *(waveform_lines.begin()+index);
    waveform_lines.erase(waveform_lines.begin()+index);
    delete linetoremove;
    linetoremove = *(spectrogram_lines.begin()+index);
    spectrogram_lines.erase(spectrogram_lines.begin()+index);
    delete linetoremove;

    m_is_edited = true;
    setStatus();
}


// Sorting functions
// (used for writing the files with ascending times)

template <typename Container>
struct compare_indirect_index
{
    const Container& container;
    compare_indirect_index( const Container& container ): container( container ) { }
    bool operator () ( size_t lindex, size_t rindex ) const {
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
    std::deque<FTGraphicsLabelItem*> sorted_waveform_labels(waveform_labels.size());
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

    gFL->ftlabels.erase(std::find(gFL->ftlabels.begin(), gFL->ftlabels.end(), this));
}
