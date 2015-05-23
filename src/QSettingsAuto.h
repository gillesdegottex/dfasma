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

#ifndef _QSettingsAuto_h_
#define _QSettingsAuto_h_

#include <assert.h>
#include <list>
#include <qsettings.h>
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QGroupBox;
class QRadioButton;
class QAction;
class QSlider;

class QSettingsAuto : public QSettings
{
	std::list<QCheckBox*> m_elements_checkbox;
	std::list<QSpinBox*> m_elements_spinbox;
    std::list<QDoubleSpinBox*> m_elements_doublespinbox;
    std::list<QLineEdit*> m_elements_lineedit;
	std::list<QComboBox*> m_elements_combobox;
    std::map<QComboBox*,bool> m_elements_combobox_usetext;
    std::list<QGroupBox*> m_elements_qgroupbox;
	std::list<QRadioButton*> m_elements_qradiobutton;
    std::list<QSlider*> m_elements_qslider;
    std::list<QAction*> m_elements_qaction;

  public:
    QSettingsAuto();
    QSettingsAuto(const QString& domain, const QString& product);

	void add(QCheckBox* el);
    void save(QCheckBox* el);
    void load(QCheckBox* el);
    void add(QSpinBox* el);
    void save(QSpinBox* el);
    void load(QSpinBox* el);
    void add(QDoubleSpinBox* el);
    void save(QDoubleSpinBox* el);
    void load(QDoubleSpinBox* el);
    void add(QLineEdit* el);
    void save(QLineEdit* el);
    void load(QLineEdit* el);

    void add(QComboBox* el, bool usetext=false);
    void save(QComboBox* el);
    void load(QComboBox* el);

    void add(QGroupBox* el);
    void save(QGroupBox* el);
    void load(QGroupBox* el);
    void add(QRadioButton* el);
    void save(QRadioButton* el);
    void load(QRadioButton* el);
    void add(QSlider* el);
    void save(QSlider* el);
    void load(QSlider* el);
    void add(QAction* el);
    void save(QAction* el);
    void load(QAction* el);

    bool contains(const QString & key);

    void saveAll();
    void loadAll();
    void clearAll();
};

#endif // _QSettingsAuto_h_
