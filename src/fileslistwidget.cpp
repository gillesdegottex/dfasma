#include "fileslistwidget.h"

#include "qthelper.h"


FilesListWidget::FilesListWidget(QMainWindow *parent) :
    QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::InternalMove);
    setWordWrap(true);
}

void FilesListWidget::closeEditor(QWidget * editor, QAbstractItemDelegate::EndEditHint hint){
    QListWidget::closeEditor(editor, hint);

    FileType* currenItem = (FileType*)(currentItem());
    if(currenItem)
        currenItem->visibleName = currenItem->text();
}

FileType* FilesListWidget::currentFile() const {
    return (FileType*)(currentItem());
}
