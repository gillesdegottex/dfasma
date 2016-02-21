#ifndef FORMGENERICTIMEVALUE_H
#define FORMGENERICTIMEVALUE_H

#include <QWidget>

namespace Ui {
class WGenericTimeValue;
}

class GVGenericTimeValue;

class WidgetGenericTimeValue : public QWidget
{
    Q_OBJECT

    friend class GVGenericTimeValue;

    GVGenericTimeValue* m_gview;

    QAction* m_aRemoveView;

public:
    explicit WidgetGenericTimeValue(QWidget *parent = 0);
    ~WidgetGenericTimeValue();

    GVGenericTimeValue* gview() const {return m_gview;}

public slots:
    void removeThisGenericTimeValue();

private:
    Ui::WGenericTimeValue *ui;
};

#endif // FORMGENERICTIMEVALUE_H
