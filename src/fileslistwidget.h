#ifndef FILESLISTWIDGET_H
#define FILESLISTWIDGET_H

#include <QListWidget>
#include <QMainWindow>
class QProgressDialog;

#include "filetype.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "ftlabels.h"

class FilesListWidget;
extern FilesListWidget* gFL; // Global accessor of the Main Window

class FilesListWidget : public QListWidget
{
    Q_OBJECT

    friend FileType;

    // Store which file exists in the list in a tree
    // TODO Cannot find a way to do it already from the Qt5 library.
    // (FilesListWidget::hasItem returns NULL)
    std::map<FileType*,bool> m_present_items;

    FTSound* m_lastSelectedSound;

    void addExistingFilesRecursive(const QStringList& files, FileType::FType type=FileType::FTUNSET);

    // The progress dialog when loading a lot of files
    QProgressDialog* m_prgdlg;
    void stopFileProgressDialog();

public:
    explicit FilesListWidget(QMainWindow *parent = 0);

    std::deque<FTSound*> ftsnds;
    std::deque<FTFZero*> ftfzeros;
    std::deque<FTLabels*> ftlabels;
    void addFile(FileType* ft);

    void addExistingFiles(const QStringList& files, FileType::FType type=FileType::FTUNSET);
    void addExistingFile(const QString& filepath, FileType::FType type=FileType::FTUNSET);

    bool hasItem(FileType *ft) const;

    FileType* currentFile() const;
    FTSound* getCurrentFTSound(bool forceselect=false);
    FTLabels* getCurrentFTLabels(bool forceselect=false);

    double getFs() const;
    unsigned int getMaxWavSize();
    double getMaxDuration();
    double getMaxLastSampleTime();


    void closeEditor(QWidget * editor, QAbstractItemDelegate::EndEditHint hint); // Wrong: it belongs to qlistview

signals:

public slots:
    void changeFileListItemsSize();
    void checkFileModifications();
    void fileInfoUpdate();

    void showFileContextMenu(const QPoint&);
    void resetAmpScale();
    void resetDelay();
    void colorSelected(const QColor& color);

    void fileSelectionChanged();
    void selectedFilesClose();
    void selectedFilesReload();
    void selectedFilesToggleShown();
    void selectedFilesDuplicate();
    void selectedFilesSave();

    void selectedFilesEstimateF0();
};

#endif // FILESLISTWIDGET_H
