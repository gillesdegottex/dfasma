// Copyright 2007 "Gilles Degottex"

// This file is part of "fmit"

// "Music" is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// "Music" is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <iostream>
using namespace std;
#include <qsettings.h>
#include "QSettingsAuto.h"
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qaction.h>

QSettingsAuto::QSettingsAuto(const QString& domain, const QString& product, const QString& setting_version)
	: QSettings(QSettings::UserScope, domain, product)
{
//	beginGroup(QString("/")+product+setting_version+"/");
    cout << "INFO: QSettingsAuto: " << fileName().toStdString() << endl;
}

QSettingsAuto::QSettingsAuto()
{
//    beginGroup(QString("/")+product+setting_version+"/");
    cout << "INFO: QSettingsAuto: " << fileName().toStdString() << endl;
}

void QSettingsAuto::add(QCheckBox* el)
{
	assert(el->objectName()!="");
	m_elements_checkbox.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QCheckBox* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->isChecked());
    endGroup();
}
void QSettingsAuto::load(QCheckBox* el)
{
    beginGroup("QSettingsAuto/");
    el->setChecked(value(el->objectName(), el->isChecked()).toBool());
    endGroup();
}

void QSettingsAuto::add(QSpinBox* el)
{
	assert(el->objectName()!="");
	m_elements_spinbox.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QSpinBox* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->value());
    endGroup();
}
void QSettingsAuto::load(QSpinBox* el)
{
    beginGroup("QSettingsAuto/");
    el->setValue(value(el->objectName(), el->value()).toInt());
    endGroup();
}

void QSettingsAuto::add(QDoubleSpinBox* el)
{
    assert(el->objectName()!="");
    m_elements_doublespinbox.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QDoubleSpinBox* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->value());
    endGroup();
}
void QSettingsAuto::load(QDoubleSpinBox* el)
{
    beginGroup("QSettingsAuto/");
    el->setValue(value(el->objectName(), el->value()).toDouble());
    endGroup();
}

void QSettingsAuto::add(QLineEdit* el)
{
    assert(el->objectName()!="");
    m_elements_lineedit.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QLineEdit* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->text());
    endGroup();
}
void QSettingsAuto::load(QLineEdit* el)
{
    beginGroup("QSettingsAuto/");
    el->setText(value(el->objectName(), (el->text())).toString());
    endGroup();
}

void QSettingsAuto::add(QComboBox* el)
{
    assert(el->objectName()!="");
    m_elements_combobox.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QComboBox* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->currentIndex());
    endGroup();
}
void QSettingsAuto::load(QComboBox* el)
{
    beginGroup("QSettingsAuto/");
    el->setCurrentIndex(value(el->objectName(), el->currentIndex()).toInt());
    endGroup();
}

void QSettingsAuto::add(QGroupBox* el)
{
    assert(el->objectName()!="");
    m_elements_qgroupbox.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QGroupBox* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->isChecked());
    endGroup();
}
void QSettingsAuto::load(QGroupBox* el)
{
    beginGroup("QSettingsAuto/");
    el->setChecked(value(el->objectName(), el->isChecked()).toBool());
    endGroup();
}

void QSettingsAuto::add(QRadioButton* el)
{
    assert(el->objectName()!="");
    m_elements_qradiobutton.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QRadioButton* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->isChecked());
    endGroup();
}
void QSettingsAuto::load(QRadioButton* el)
{
    beginGroup("QSettingsAuto/");
    el->setChecked(value(el->objectName(), el->isChecked()).toBool());
    endGroup();
}

void QSettingsAuto::add(QSlider* el)
{
    assert(el->objectName()!="");
    m_elements_qslider.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
void QSettingsAuto::save(QSlider* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->value());
    endGroup();
}
void QSettingsAuto::load(QSlider* el)
{
    beginGroup("QSettingsAuto/");
    el->setValue(value(el->objectName(), el->value()).toInt());
    endGroup();
}

void QSettingsAuto::add(QAction* el)
{
    assert(el->objectName()!="");
    m_elements_qaction.push_back(el);
    if(contains(el->objectName()))
        load(el);
}
bool QSettingsAuto::contains(const QString & key) {
    bool ret;

    beginGroup("QSettingsAuto/");
    ret = QSettings::contains(key);
    endGroup();

    return ret;
}
void QSettingsAuto::save(QAction* el)
{
    beginGroup("QSettingsAuto/");
    setValue(el->objectName(), el->isChecked());
    endGroup();
}
void QSettingsAuto::load(QAction* el)
{
    beginGroup("QSettingsAuto/");
    el->setChecked(value(el->objectName(), el->isChecked()).toBool());
    endGroup();
}

void QSettingsAuto::saveAll()
{
	for(list<QCheckBox*>::iterator it=m_elements_checkbox.begin(); it!=m_elements_checkbox.end(); it++)
        save(*it);

	for(list<QSpinBox*>::iterator it=m_elements_spinbox.begin(); it!=m_elements_spinbox.end(); it++)
        save(*it);

    for(list<QDoubleSpinBox*>::iterator it=m_elements_doublespinbox.begin(); it!=m_elements_doublespinbox.end(); it++)
        save(*it);

	for(list<QLineEdit*>::iterator it=m_elements_lineedit.begin(); it!=m_elements_lineedit.end(); it++)
        save(*it);

	for(list<QComboBox*>::iterator it=m_elements_combobox.begin(); it!=m_elements_combobox.end(); it++)
        save(*it);

	for(list<QGroupBox*>::iterator it=m_elements_qgroupbox.begin(); it!=m_elements_qgroupbox.end(); it++)
        save(*it);

	for(list<QRadioButton*>::iterator it=m_elements_qradiobutton.begin(); it!=m_elements_qradiobutton.end(); it++)
        save(*it);

    for(list<QSlider*>::iterator it=m_elements_qslider.begin(); it!=m_elements_qslider.end(); it++)
        save(*it);

    for(list<QAction*>::iterator it=m_elements_qaction.begin(); it!=m_elements_qaction.end(); it++)
        save(*it);

    sync();
}
void QSettingsAuto::loadAll()
{
	for(list<QCheckBox*>::iterator it=m_elements_checkbox.begin(); it!=m_elements_checkbox.end(); it++)
        load(*it);

	for(list<QSpinBox*>::iterator it=m_elements_spinbox.begin(); it!=m_elements_spinbox.end(); it++)
        load(*it);

    for(list<QDoubleSpinBox*>::iterator it=m_elements_doublespinbox.begin(); it!=m_elements_doublespinbox.end(); it++)
        load(*it);

	for(list<QLineEdit*>::iterator it=m_elements_lineedit.begin(); it!=m_elements_lineedit.end(); it++)
        load(*it);

	for(list<QComboBox*>::iterator it=m_elements_combobox.begin(); it!=m_elements_combobox.end(); it++)
        load(*it);

	for(list<QGroupBox*>::iterator it=m_elements_qgroupbox.begin(); it!=m_elements_qgroupbox.end(); it++)
        load(*it);

	for(list<QRadioButton*>::iterator it=m_elements_qradiobutton.begin(); it!=m_elements_qradiobutton.end(); it++)
        load(*it);

    for(list<QSlider*>::iterator it=m_elements_qslider.begin(); it!=m_elements_qslider.end(); it++)
        load(*it);

    for(list<QAction*>::iterator it=m_elements_qaction.begin(); it!=m_elements_qaction.end(); it++)
        load(*it);

    sync();
}
void QSettingsAuto::clearAll()
{
    QSettingsAuto::clear();

    sync();
}


//    QStringList	l = allKeys();
//    for(int f=0; f<l.size(); f++){
//        cout << f << ": " << l.at(f).toLatin1().constData() << endl;
//    }
