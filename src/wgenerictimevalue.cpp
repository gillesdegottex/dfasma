#include "wgenerictimevalue.h"
#include "ui_wgenerictimevalue.h"

#include <QToolBar>

#include "gvgenerictimevalue.h"
#include "ftgenerictimevalue.h"

#include "qaehelpers.h"

WidgetGenericTimeValue::WidgetGenericTimeValue(QWidget *parent)
    : QWidget(parent)
    , m_gview(NULL)
    , ui(new Ui::WGenericTimeValue)
{
    ui->setupUi(this);

    m_aRemoveView = new QAction(tr("Remove this view"), this);
    m_aRemoveView->setStatusTip(tr("Remove this view"));
    m_aRemoveView->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(m_aRemoveView, SIGNAL(triggered()), this, SLOT(removeThisGenericTimeValue()));

    m_gview = new GVGenericTimeValue(this);
    ui->lGraphicsView->addWidget(m_gview);
}

void WidgetGenericTimeValue::removeThisGenericTimeValue(){
    gMW->removeWidgetGenericTimeValue(this);
}

WidgetGenericTimeValue::~WidgetGenericTimeValue(){

    while(m_gview->m_ftgenerictimevalues.size()>0)
        delete m_gview->m_ftgenerictimevalues.front(); // The destructor removes the item from the list

    delete m_gview;
    m_gview = NULL;

    delete ui;
}

