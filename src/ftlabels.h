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

#ifndef FTLABELS_H
#define FTLABELS_H

#include <deque>
#include <vector>

#include <QString>
#include <QColor>
#include <QAction>
#include <QGraphicsTextItem>

#include "filetype.h"

class QGraphicsSimpleTextItem;
class QGraphicsLineItem;
class FTLabels;

class FTGraphicsLabelItem : public QGraphicsTextItem
{
    FTLabels* m_ftl;
    static bool sm_isEditing;
    QString m_prevText;

public:
    FTGraphicsLabelItem(FTLabels* ftl, const QString & text);
    FTGraphicsLabelItem(FTGraphicsLabelItem* ftgi, FTLabels* ftl); // copy ctor

    virtual void keyPressEvent(QKeyEvent * event);
    virtual void focusInEvent(QFocusEvent * event);
    virtual void focusOutEvent(QFocusEvent * event);

    virtual void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,QWidget *widget);

    static bool isEditing() {return sm_isEditing;}
};


class FTLabels : public QObject, public FileType
{
    Q_OBJECT

    void init(); // Has to be called only once, at object ctor
    void load();
    void sort(); // For writing files in ascending order

    QAction* m_actionSave;
    QAction* m_actionSaveAs;

    bool m_isedited;

    enum FileFormat {FFNotSpecified, FFAutoDetect, FFAsciiTimeText, FFAsciiSegments, FFAsciiSegmentsHTK, FFSDIF};
    int m_fileformat;

public:
    FTLabels(QObject *parent);
    FTLabels(const FTLabels& ft);
    FTLabels(const QString& _fileName, QObject* parent);
    virtual FileType* duplicate();

    std::deque<double> starts; // TODO Get ride of ?
    std::deque<FTGraphicsLabelItem*> waveform_labels;
    std::deque<QGraphicsSimpleTextItem*> spectrogram_labels;
    std::deque<QGraphicsLineItem*> waveform_lines;
    std::deque<QGraphicsLineItem*> spectrogram_lines;

    virtual QString info() const;
    virtual double getLastSampleTime() const;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);
    virtual bool isModified() {return m_isedited;}
    void updateTextsGeometry();

    int getNbLabels() const {return int(starts.size());}
    void moveLabel(int index, double position);
    void changeText(int index, const QString& text);

    ~FTLabels();

public slots:
    void reload();
    void save();
    void saveAs();
    void clear();
    void removeLabel(int index);
    void addLabel(double position, const QString& text);
    void setVisible(bool shown);
};

#endif // FTLABELS_H
