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
extern FilesListWidget* gFL; // Global accessor of the file list

class FilesListWidget : public QListWidget
{
    Q_OBJECT

    friend class FileType;

    // Store which file exists in the list in a tree
    // I cannot find a way to do it already from the Qt5 library.
    // (FilesListWidget::hasItem returns NULL)
    std::map<FileType*,bool> m_present_files;
    FileType* m_prevSelectedFile;
    FTSound* m_prevSelectedSound;
    void addExistingFilesRecursive(const QStringList& files, FileType::FType type=FileType::FTUNSET);

    std::deque<FileType*> m_current_sourced;

    // The progress dialog when loading a lot of files
    QProgressDialog* m_prgdlg;
    void stopFileProgressDialog();


    enum CurrentAction {CANothing, CASetSource};
    CurrentAction m_currentAction;

    virtual void keyPressEvent(QKeyEvent * event);

public:
    explicit FilesListWidget(QMainWindow *parent = 0);

    std::deque<FTSound*> ftsnds;
    int m_nb_snds_in_selection;
    std::deque<FTFZero*> ftfzeros;
    int m_nb_fzeros_in_selection;
    std::deque<FTLabels*> ftlabels;
    int m_nb_labels_in_selection;
    bool hasFile(FileType *ft) const;

    void addExistingFiles(const QStringList& files, FileType::FType type=FileType::FTUNSET);
    void addExistingFile(const QString& filepath, FileType::FType type=FileType::FTUNSET);

    FileType* currentFile() const;
    FTSound* getCurrentFTSound(bool forceselect=false);
    FTLabels* getCurrentFTLabels(bool forceselect=false);
    FTFZero* getCurrentFTFZero(bool forceselect=false);

    double getFs() const;
    unsigned int getMaxWavSize();
    double getMaxDuration();
    double getMaxLastSampleTime();
    WAVTYPE getMaxSQNR() const; // Get the maximum QSNR among all sound files

    void openEditor(QWidget * editor);
    void closeEditor(QWidget * editor, QAbstractItemDelegate::EndEditHint hint);

public slots:
    void changeFileListItemsSize();
    void checkFileModifications();
    void fileInfoUpdate();
    void setLabelsEditable(bool editable);

    void showFileContextMenu(const QPoint&);
    void resetAmpScale();
    void resetDelay();
    void colorSelected(const QColor& color);
    void setSource(FileType *file=NULL);

    void fileSelectionChanged();
    void selectedFilesClose();
    void selectedFilesReload();
    void selectedFilesToggleShown();
    void selectedFilesDuplicate();
    void selectedFilesSave();

    void selectedFilesEstimateF0();
    void selectedFilesEstimateVoicedUnvoicedMarkers();
};

#endif // FILESLISTWIDGET_H
