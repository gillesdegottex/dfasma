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
#include "wmainwindow.h"
#include "ui_wmainwindow.h"

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

FileType::FileType(FILETYPE _type, const QString& _fileName, QObject * child)
    : type(_type)
    , fileName(_fileName)
    , color(GetNextColor())
{
//    cout << "FileType::FileType: " << _fileName.toLocal8Bit().constData() << endl;

    m_actionShow = new QAction("Show", child);
    m_actionShow->setStatusTip("Show the sound in the views");
//    m_actionShow->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_H));
    m_actionShow->setCheckable(true);
    m_actionShow->setChecked(true);

//    QIODevice::open(QIODevice::ReadOnly);
}

void FileType::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {
    contextmenu.addAction(m_actionShow);
    mainwindow->connect(m_actionShow, SIGNAL(toggled(bool)), mainwindow, SLOT(setSoundShown(bool)));
    contextmenu.addAction(mainwindow->ui->actionCloseFile);
    contextmenu.addSeparator();
}

FileType::~FileType() {
//    delete m_actionShow; // Make the closure crash (??)

    sg_colors.push_front(color);
}
