#ifndef FILESLISTWIDGET_H
#define FILESLISTWIDGET_H

#include <QListWidget>
#include <QMainWindow>

#include "filetype.h"

class FilesListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit FilesListWidget(QMainWindow *parent = 0);

    void closeEditor(QWidget * editor, QAbstractItemDelegate::EndEditHint hint); // Wrong: it belongs to qlistview

    FileType* currentFile() const;

signals:

public slots:

};

#endif // FILESLISTWIDGET_H
