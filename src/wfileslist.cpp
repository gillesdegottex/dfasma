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

#include "wfileslist.h"

#include <QFileInfo>
#include <QProgressDialog>
#include <QDir>
#include <QMessageBox>
#include <QItemDelegate>
#include <QKeyEvent>

#include "filetype.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "ftlabels.h"
#include "ftgenerictimevalue.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ui_wdialogsettings.h"
#include "wdialogselectchannel.h"
#include "ui_wdialogselectchannel.h"
#include "wdialogfiletypechoosertxt.h"
#include "gvwaveform.h"
#include "gvspectrumamplitude.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "filetype.h"
#include "../external/audioengine/audioengine.h"
#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"
#include "../external/libqxt/qxtspanslider.h"
#include "wgenerictimevalue.h"
#include "gvgenerictimevalue.h"

#include <fstream>

#include "qaehelpers.h"

WFilesList* gFL = NULL;

// Create a Delegate for calling the openEditor that is sadly missing from QAbstractItemView
class FilesListWidgetDelegate : public QItemDelegate {
    public:
    FilesListWidgetDelegate(QObject * parent = 0)
        : QItemDelegate(parent)
    {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        QWidget* editor = QItemDelegate::createEditor(parent, option, index);
        gFL->openEditor(editor);
        return editor;
    }
};

WFilesList::WFilesList(QMainWindow *parent)
    : QListWidget(parent)
    , m_prgdlg(NULL)
    , m_loadingmsgbox(NULL)
    , m_currentAction(CANothing)
    , m_prevSelectedFile(NULL)
    , m_prevSelectedSound(NULL)
{
    gFL = this;

    m_nb_snds_in_selection = 0;
    m_nb_labels_in_selection = 0;
    m_nb_fzeros_in_selection = 0;

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::InternalMove);
    setWordWrap(true);

    setItemDelegate(new FilesListWidgetDelegate(this));
}

void WFilesList::openEditor(QWidget * editor){
    Q_UNUSED(editor)
    FileType* currenItem = (FileType*)(currentItem());
    if(currenItem){
        QString txt = currenItem->text();
        if(txt.size()>0 && txt[0]=='!')
            txt = txt.mid(1);
        if(txt.size()>0 && txt[0]=='*')
            txt = txt.mid(1);
        currenItem->setText(txt);
    }
}

void WFilesList::closeEditor(QWidget * editor, QAbstractItemDelegate::EndEditHint hint){
    QListWidget::closeEditor(editor, hint);

    FileType* currenItem = (FileType*)(currentItem());
    if(currenItem){
        currenItem->visibleName = currenItem->text();
        currenItem->setStatus();
    }
}

void WFilesList::changeFileListItemsSize() {
//    COUTD << m_dlgSettings->ui->sbFileListItemSize->value() << endl;

    QFont cfont = font();
    cfont.setPixelSize(gMW->m_dlgSettings->ui->sbFileListItemSize->value());
    QSize size = iconSize();
    size.setHeight(gMW->m_dlgSettings->ui->sbFileListItemSize->value()+2);
    size.setWidth(gMW->m_dlgSettings->ui->sbFileListItemSize->value()+2);
    setIconSize(size);
    setFont(cfont);
}

// Check if a file has been modified on the disc
void WFilesList::checkFileModifications(){
//    cout << "GET FOCUS " << QDateTime::currentMSecsSinceEpoch() << endl;
    for(size_t fi=0; fi<ftsnds.size(); fi++)
        if(!ftsnds[fi]->isDistantFile())
            ftsnds[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftfzeros.size(); fi++)
        if(!ftfzeros[fi]->isDistantFile())
            ftfzeros[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftlabels.size(); fi++)
        if(!ftlabels[fi]->isDistantFile())
            ftlabels[fi]->checkFileStatus();

    gFL->fileInfoUpdate();
}

void WFilesList::stopFileProgressDialog() {
//    COUTD << "WMainWindow::stopFileProgressDialog " << m_prgdlg << endl;
    if(m_prgdlg){
        m_prgdlg->setValue(m_prgdlg->maximum());// Stop the loading bar
        m_prgdlg->hide();
    }
    QCoreApplication::processEvents(); // To show the progress
}

void WFilesList::addExistingFilesRecursive(const QStringList& files, FileType::FType type) {
    for(int fi=0; fi<files.size() && !m_prgdlg->wasCanceled(); fi++) {
        if(QFileInfo(files[fi]).isDir()) {
//            COUTD << "Add recursive" << endl;
            QDir fpd(files[fi]);

            // Recursive call on directories
            fpd.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
            if(fpd.count()>0){
                for(int fpdi=0; fpdi<int(fpd.count()) && !m_prgdlg->wasCanceled(); ++fpdi){
//                    COUTD << "Dir: " << fpd[fpdi].toLatin1().constData() << endl;
                    addExistingFilesRecursive(QStringList(fpd.filePath(fpd[fpdi])));
                }
            }

            // Add the files of the current directory
            fpd.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            if(fpd.count()>0){
                m_prgdlg->setMaximum(fpd.count());
//                COUTD << "Setmax " << fpd.count() << endl;
                for(int fpdi=0; fpdi<int(fpd.count()) && !m_prgdlg->wasCanceled(); ++fpdi){
//                    COUTD << "File: " << fpd[fpdi].toLatin1().constData() << endl;
                    m_prgdlg->setValue(fpdi);
                    QCoreApplication::processEvents(); // To show the progress
                    addExistingFile(fpd.filePath(fpd[fpdi]));
                }
            }
        }
        else {
            if(m_prgdlg) m_prgdlg->setValue(fi);
            QCoreApplication::processEvents(); // To show the progress
            addExistingFile(files[fi], type);
        }
    }
}

void WFilesList::addExistingFiles(const QStringList& files, FileType::FType type) {

    // These progress dialogs HAVE to be built on the stack otherwise ghost dialogs appear.
    QProgressDialog prgdlg("Opening files...", "Abort", 0, files.size(), this);
    prgdlg.setMinimumDuration(500);
    m_prgdlg = &prgdlg;

    addExistingFilesRecursive(files, type);

    stopFileProgressDialog();
    m_prgdlg = NULL;
}

void WFilesList::addExistingFile(const QString& filepath, FileType::FType type) {
    // Not used for directories, use addExistingFiles instead

    try{
        bool isfirsts = ftsnds.size()==0;
        int viewid=-1; // The view where the file will be added to (only for generic file types)
                       // By default, create a new view

        // Attention: There is the type of data stored in DFasma (FILETYPE) (ex. FileType::FTSOUND)
        //  and the format of the file (ex. FFSOUND)
        //  and the file container (sdif, any sound, text)

        QFileInfo fileinfo(filepath);
        int filesize = fileinfo.size()/std::pow(2.0, 20.0); // File size in [MB]
//        DCOUT << filepath << " size: " << filesize << "MB" << std::endl;

        if(filesize>50){ // If bigger than X MB
            m_loadingmsgbox = new QMessageBox(gMW);
            m_loadingmsgbox->setWindowTitle("DFasma");
            m_loadingmsgbox->setText("Loading big file ...");
            m_loadingmsgbox->setWindowModality(Qt::NonModal);
            m_loadingmsgbox->setStandardButtons(QMessageBox::NoButton);
            m_loadingmsgbox->show();
            for(int nsilly=0; nsilly<3; ++nsilly){
                QThread::msleep(10);
                QCoreApplication::processEvents(); // To show the progress
            }
        }

        // This should be always "guessable"
        FileType::FileContainer container = FileType::guessContainer(FileType::removeDataSelectors(filepath));

        // Then, guess the type of the data in the file, if not specified yet
        if(type==FileType::FTUNSET){
            // The format and the DFasma's type have to correspond
            if(container==FileType::FCANYSOUND) {
                type = FileType::FTSOUND; // These two have to match
            }
            #ifdef SUPPORT_SDIF
            else if(container==FileType::FCSDIF) {
                if(FileType::SDIF_hasFrame(filepath, "1FQ0"))
                    type = FileType::FTFZERO;
                else if (FileType::SDIF_hasFrame(filepath, "1MRK"))
                    type = FileType::FTLABELS;
                else
                    throw QString("Unsupported SDIF data.");
            }
            #endif
            else if(container==FileType::FCTEXT) {
                // Distinguish between f0, labels and future VUF files (and futur others ...)
                // Do a grammar check (But this won't help to diff F0 and VUF files)
                // TODO Switch to QTextStream
                std::ifstream fin(filepath.toLatin1().constData());
                if(!fin.is_open())
                    throw QString("Cannot open this file");
                double t;
                std::string line, text;
                // Check the first line only (Assuming it is enough)
                // TODO May have to skip commented lines depending on the text format
                if(!std::getline(fin, line))
                    throw QString("There is not a single line in this file");

                // Check for a label: <number> <number> <txt>
                std::istringstream iss(line);
                if((iss >> t >> t >> text) && iss.eof())
                    type = FileType::FTLABELS;
                else{
                    stopFileProgressDialog();
                    WDialogFileTypeChooserTxt dlg(this, filepath);
                    int ret = dlg.exec();
                    if(ret==1){
                        type = dlg.selectedFileType();
                        viewid = dlg.selectedView();
                    }
                }
            }
            else if(container==FileType::FCEST) {
                // Currently support only F0 data
                type = FileType::FTFZERO;
            }
            else if(container==FileType::FCBINARY) {
                // TODO
            }
        }

        if(type==FileType::FTUNSET)
            throw QString("Cannot find any data or audio channel in this file that is handled by DFasma.");

        // Finally, load the data knowing the file type and the container
        if(type==FileType::FTSOUND){
            int nchan = FTSound::getNumberOfChannels(filepath);
            if(nchan==1){
                // If there is only one channel, just load it
                addItem(new FTSound(filepath, this));
            }
            else{
                // If more than one channel, ask what to do
                stopFileProgressDialog();
                WDialogSelectChannel dlg(filepath, nchan, this);
                if(dlg.exec()) {
                    if(dlg.ui->rdbImportEachChannel->isChecked()){
                        for(int ci=1; ci<=nchan; ci++){
                            addItem(new FTSound(filepath, this, ci));
                        }
                    }
                    else if(dlg.ui->rdbImportOnlyOneChannel->isChecked()){
                        addItem(new FTSound(filepath, this, dlg.ui->sbChannelID->value()));
                    }
                    else if(dlg.ui->rdbMergeAllChannels->isChecked()){
                        addItem(new FTSound(filepath, this, -2));// -2 is a code for merging the channels
                    }
                }
            }

            if(ftsnds.size()>0){
                // The first sound determines the common sampling frequency for the audio output
                if(isfirsts)
                    gMW->audioInitialize(ftsnds[0]->fs);

                gMW->m_gvWaveform->fitViewToSoundsAmplitude();
            }
        }
        else if(type == FileType::FTFZERO){
            addItem(new FTFZero(filepath, this, container));
        }
        else if(type == FileType::FTLABELS){
            addItem(new FTLabels(filepath, this, container));
        }
        else if(type==FileType::FTGENTIMEVALUE){
            WidgetGenericTimeValue* widget = NULL;
//            if(viewid==-2)
//                throw QString("A view has not been selected for "+filepath);
            if(viewid==-1)
                widget = gMW->addWidgetGenericTimeValue();
            else
                widget = gMW->m_wGenericTimeValues.at(viewid);
            addItem(new FTGenericTimeValue(filepath, widget, container));
        }
    }
    catch (QString err)
    {
        stopFileProgressDialog();
        QMessageBox::StandardButton ret=QMessageBox::warning(this, "Failed to load file ...", "Data from the following file can't be loaded:\n"+filepath+"'\n\nReason:\n"+err, QMessageBox::Ok | QMessageBox::Abort, QMessageBox::Ok);
        if(ret==QMessageBox::Abort)
            if(m_prgdlg)
                m_prgdlg->cancel();
    }

    if(m_loadingmsgbox){
        m_loadingmsgbox->close();
        delete m_loadingmsgbox;
        m_loadingmsgbox = NULL;
    }
}


bool WFilesList::hasFile(FileType *ft) const {
//    COUTD << "FilesListWidget::hasItem " << ft << endl;

    if(ft==NULL) return false;

    return m_present_files.find(ft)!=m_present_files.end();
}

FileType* WFilesList::currentFile() const {
    return (FileType*)(currentItem());
}

FTSound* WFilesList::getCurrentFTSound(bool forceselect) {

    if(ftsnds.empty())
        return NULL;

    FileType* currenItem = currentFile();

    if(currenItem && currenItem->is(FileType::FTSOUND))
        return (FTSound*)currenItem;

    if(forceselect){
        if(m_prevSelectedSound)
            return m_prevSelectedSound;
        else
            return ftsnds[0];
    }

    return NULL;
}

FTFZero* WFilesList::getCurrentFTFZero(bool forceselect) {

    if(ftfzeros.empty())
        return NULL;

    FileType* currenItem = currentFile();

    if(currenItem && currenItem->is(FileType::FTFZERO))
        return (FTFZero*)currenItem;

    if(forceselect)
        return ftfzeros[0];

    return NULL;
}

FTLabels* WFilesList::getCurrentFTLabels(bool forceselect) {

    if(ftlabels.empty())
        return NULL;

    FileType* currenItem = currentFile();

    if(currenItem && currenItem->is(FileType::FTLABELS))
        return (FTLabels*)currenItem;

    if(forceselect)
        return ftlabels[0];

    return NULL;
}
void WFilesList::setLabelsEditable(bool editable){
    for(size_t fi=0; fi<ftlabels.size(); fi++){
        for(size_t li=0; li<ftlabels[fi]->waveform_labels.size(); li++){
            if(editable)
                ftlabels[fi]->waveform_labels[li]->setTextInteractionFlags(Qt::TextEditorInteraction);
            else
                ftlabels[fi]->waveform_labels[li]->setTextInteractionFlags(Qt::NoTextInteraction);
        }
    }
}

void WFilesList::showFileContextMenu(const QPoint& pos) {

    QMenu contextmenu(this);

    QList<QListWidgetItem*> l = selectedItems();
    if(l.size()==1) {
        FileType* currenItem = currentFile();
        currenItem->fillContextMenu(contextmenu);
    }
    else {
        contextmenu.addAction(gMW->ui->actionSelectedFilesToggleShown);
        contextmenu.addAction(gMW->ui->actionSelectedFilesReload);
        contextmenu.addAction(gMW->ui->actionSelectedFilesDuplicate);
        contextmenu.addAction(gMW->ui->actionSelectedFilesClose);
    }

    int contextmenuheight = contextmenu.sizeHint().height();
    QPoint posglobal = mapToGlobal(pos)+QPoint(24,contextmenuheight/2);
    contextmenu.exec(posglobal);
}

void WFilesList::fileSelectionChanged() {
//    DCOUT << "WFilesList::fileSelectionChanged " << m_currentAction << endl;

    if(m_currentAction==CASetSource){
        setSource(currentFile());
        m_currentAction = CANothing;
        setCursor(Qt::ArrowCursor);
        setCurrentItem(m_prevSelectedFile);
    }
    else{
        QList<QListWidgetItem*> list = selectedItems();
        m_nb_snds_in_selection = 0;
        m_nb_labels_in_selection = 0;
        m_nb_fzeros_in_selection = 0;

        FileType* currfile = currentFile();
        if(currfile!=m_prevSelectedFile){
            if(m_prevSelectedFile)
                m_prevSelectedFile->zposReset();
            currfile->zposBringForward();
            m_prevSelectedFile = currfile;
        }

        for(int i=0; i<list.size(); i++) {
            FileType* ft = ((FileType*)list.at(i));
            if(ft->is(FileType::FTSOUND)){
                m_nb_snds_in_selection++;
                m_prevSelectedSound = (FTSound*)ft;
            }

            if(ft->is(FileType::FTLABELS))
                m_nb_labels_in_selection++;

            if(ft->is(FileType::FTFZERO))
                m_nb_fzeros_in_selection++;
        }

        // Update the spectrogram to current selected signal
        if(m_nb_snds_in_selection>0){
            if(gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop){
                gMW->m_gvWaveform->m_scene->update();
                gMW->m_gvSpectrumAmplitude->m_scene->update();
                gMW->m_gvSpectrumPhase->m_scene->update();
                gMW->m_gvSpectrumGroupDelay->m_scene->update();
            }
            gMW->m_gvSpectrogram->updateSTFTPlot();
            gMW->m_gvSpectrogram->m_scene->update();
        }
        if(m_nb_fzeros_in_selection>0){
            gMW->m_gvSpectrumAmplitude->m_scene->update();
            gMW->m_gvSpectrogram->m_scene->update();
        }

        // Update source symbols
        // First clear
        for(size_t fi=0; fi<m_current_sourced.size(); ++fi)
            if(hasFile(m_current_sourced[fi]))
                m_current_sourced[fi]->setIsSource(false);
        m_current_sourced.clear();
        // Then show the source symbol ('S')
        if(list.size()==1){
            FileType* ft = ((FileType*)list.at(0));
            if(ft->is(FileType::FTFZERO)){
                FTSound* ftsnd = ((FTFZero*)ft)->m_src_snd;
                if(hasFile(ftsnd)){
                    ftsnd->setIsSource(true);
                    m_current_sourced.push_back(ftsnd);
                }
            }
            if(ft->is(FileType::FTLABELS)){
                FTFZero* ftfzero = ((FTLabels*)ft)->m_src_fzero;
                if(hasFile(ftfzero)){
                    ftfzero->setIsSource(true);
                    m_current_sourced.push_back(ftfzero);
                }
            }
        }

        gMW->ui->actionSelectedFilesSave->setEnabled(m_nb_labels_in_selection>0 || m_nb_fzeros_in_selection>0);
        gMW->ui->actionSelectedFilesClose->setEnabled(list.size()>0);

        fileInfoUpdate();
        gMW->updateMouseCursorState(QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier), QApplication::keyboardModifiers().testFlag(Qt::ControlModifier));
        gMW->checkEditHiddenFile();
    }

//    DCOUT << "WFilesList::~fileSelectionChanged" << endl;
}
void WFilesList::fileInfoUpdate() {
    QList<QListWidgetItem*> list = selectedItems();

    // If only one file selected
    // Display Basic information of it
    if(list.empty()) {
        gMW->ui->lblFileInfo->hide();
    }
    else if(list.size()==1) {
        gMW->ui->lblFileInfo->setText(((FileType*)list.at(0))->info());
        gMW->ui->lblFileInfo->show();
    }
    else {
        gMW->ui->lblFileInfo->setText(QString::number(list.size())+" files selected");
        gMW->ui->lblFileInfo->show();
    }
}

// FileType is not an qobject, thus, need to forward the message manually (i.e. without signal system).
void WFilesList::colorSelected(const QColor& color){
    FileType* currenItem = (FileType*)(currentItem());
    if(currenItem)
        currenItem->setColor(color);
}

void WFilesList::setSource(FileType* file){
    if(file==NULL){
        m_currentAction = CASetSource;
        setCursor(Qt::PointingHandCursor);
    }
    else{
        if(m_prevSelectedFile)
            m_prevSelectedFile->setSource(file);
    }
}

void WFilesList::keyPressEvent(QKeyEvent *event){
    if(event->key()==Qt::Key_Escape) {
        m_currentAction = CANothing;
        setCursor(Qt::ArrowCursor);
    }

    QListWidget::keyPressEvent(event);
}

void WFilesList::resetAmpScale(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_giWavForWaveform->setGain(1.0);

        currentftsound->setStatus();

        gMW->allSoundsChanged();
    }
}
void WFilesList::resetDelay(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_giWavForWaveform->setDelay(0.0);

        currentftsound->setStatus();

        gMW->allSoundsChanged();
    }
}

void WFilesList::selectedFilesToggleShown() {
    QList<QListWidgetItem*> list = selectedItems();
    for(int i=0; i<list.size(); i++){
        ((FileType*)list.at(i))->setVisible(!((FileType*)list.at(i))->m_actionShow->isChecked());
        if(((FileType*)list.at(i))==m_prevSelectedSound
            && m_prevSelectedSound->isVisible())
            gMW->m_gvSpectrogram->updateSTFTPlot();
    }
    gMW->m_gvWaveform->m_scene->update();
    gMW->m_gvSpectrogram->m_scene->update();
    gMW->m_gvSpectrumAmplitude->updateDFTs();
    gMW->m_gvSpectrumAmplitude->m_scene->update();
    gMW->m_gvSpectrumPhase->m_scene->update();
    gMW->m_gvSpectrumGroupDelay->m_scene->update();
    gMW->checkEditHiddenFile();
}

void WFilesList::selectedFilesClose() {
    gMW->m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = selectedItems();
    clearSelection();
    gMW->ui->actionSelectedFilesSave->setEnabled(false);

    for(int i=0; i<l.size(); i++){

        FileType* ft = (FileType*)l.at(i);

        // cout << "INFO: Closing file: '" << ft->fileFullPath << "'" << endl;

        delete ft; // Removes also from the listview
    }

    // If any of the generic view is empty, remove it
    for(int i=0; i<gMW->m_wGenericTimeValues.size(); ++i){
        if(gMW->m_wGenericTimeValues[i]->gview()->m_ftgenerictimevalues.size()==0)
            gMW->removeWidgetGenericTimeValue(gMW->m_wGenericTimeValues[i]);
    }

    gMW->updateWindowTitle();

    if(gMW->m_gvSpectrogram->m_dlgSettings->ui->cbSpectrogramColorRangeMode->currentIndex()==1)
        gMW->m_qxtSpectrogramSpanSlider->setMinimum(-3*getMaxSQNR());
    gMW->m_gvSpectrumAmplitude->updateAmplitudeExtent();

    if(gMW->m_gvWaveform->m_scene->sceneRect().right()>gFL->getMaxDuration()+1.0/gFL->getFs()) {
        gMW->m_gvWaveform->updateSceneRect();
        gMW->m_gvSpectrogram->updateSceneRect();
        gMW->m_gvWaveform->setMouseCursorPosition(-1, true);
        gMW->m_gvWaveform->viewSet(gMW->m_gvWaveform->m_scene->sceneRect(), true);
    }

    gMW->m_gvSpectrogram->m_scene->update();

    // If there is no more files, put the interface in a waiting-for-file state.
    if(count()==0)
        gMW->setInWaitingForFileState();
    else
        gMW->allSoundsChanged();
}

void WFilesList::selectedFilesReload() {
//    COUTD << "WMainWindow::selectedFileReload" << endl;

    gMW->m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = selectedItems();

    bool reloadSelectedSound = false;
    bool didanysucceed = false;

    for(int i=0; i<l.size(); i++) {

        FileType* ft = (FileType*)l.at(i);

        try{
            if(ft->reload())
                didanysucceed = true;
        }
        catch (QString err){
            QMessageBox::warning(this, "Failed to re-load file ...", "Data from the following file can't be re-loaded:\n"+ft->fileFullPath+"'\n\nReason:\n"+err, QMessageBox::Ok | QMessageBox::Abort, QMessageBox::Ok);
        }

        if(ft==m_prevSelectedSound)
            reloadSelectedSound = true;
    }

    fileInfoUpdate();

    if(didanysucceed && reloadSelectedSound) {
        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvSpectrumAmplitude->updateAmplitudeExtent();
        gMW->m_gvSpectrumAmplitude->updateDFTs();
        gMW->m_gvSpectrogram->updateSTFTPlot(true); // Force the STFT computation
    }

//    COUTD << "WMainWindow::~selectedFileReload" << endl;
}

void WFilesList::selectedFilesDuplicate() {
    QList<QListWidgetItem*> l = selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        FileType* ft = currentfile->duplicate();
        if(ft){
            addItem(ft);
            gMW->m_gvWaveform->updateSceneRect();
            gMW->m_gvSpectrumAmplitude->updateAmplitudeExtent();
            gMW->allSoundsChanged();
            gMW->ui->actionSelectedFilesClose->setEnabled(true);
            gMW->ui->actionSelectedFilesReload->setEnabled(true);
            gMW->ui->splitterViews->show();
            gMW->updateWindowTitle();
        }
    }
}

void WFilesList::selectedFilesSave() {
    QList<QListWidgetItem*> l = selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        if(currentfile->is(FileType::FTLABELS))
            ((FTLabels*)currentfile)->save();

        if(currentfile->is(FileType::FTFZERO))
            ((FTFZero*)currentfile)->save();
    }
}

void WFilesList::selectedFilesEstimateF0() {
    QList<QListWidgetItem*> l = selectedItems();

    // Get the f0 estimation range ...
    double f0min = gMW->m_dlgSettings->ui->dsbEstimationF0Min->value();
    double f0max = gMW->m_dlgSettings->ui->dsbEstimationF0Max->value();
    if(gMW->m_gvSpectrumAmplitude->m_selection.width()>0.0){
        f0min = gMW->m_gvSpectrumAmplitude->m_selection.left();
        f0max = gMW->m_gvSpectrumAmplitude->m_selection.right();
    }
    double tstart = -1;
    double tend = -1;
    if(gMW->m_gvWaveform->m_selection.width()>0.0){
        tstart = gMW->m_gvWaveform->m_selection.left();
        tend = gMW->m_gvWaveform->m_selection.right();
    }

    bool force = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);

    // These progress dialogs HAVE to be built on the stack otherwise ghost dialogs appear.
    QProgressDialog prgdlg("Estimating F0...", "Abort", 0, l.size(), this);
    prgdlg.setMinimumDuration(500);
    m_prgdlg = &prgdlg;

    for(int i=0; i<l.size() && !m_prgdlg->wasCanceled(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        try {
            // If from a sound, generate a new F0 file
            if(currentfile->is(FileType::FTSOUND))
                gFL->addItem(new FTFZero(gFL, (FTSound*)currentfile, f0min, f0max, tstart, tend, force));

            // If from an F0 file, update it
            if(currentfile->is(FileType::FTFZERO))
                ((FTFZero*)currentfile)->estimate(NULL, f0min, f0max, tstart, tend, force);

            gMW->m_gvSpectrogram->m_scene->update();
            gMW->m_gvSpectrumAmplitude->m_scene->update();

            m_prgdlg->setValue(i);
        }
        catch(QString err){
            gMW->globalWaitingBarDone();
            stopFileProgressDialog();
            QMessageBox::StandardButton ret=QMessageBox::warning(gMW, "Error during F0 estimation", "Estimation of the F0 of "+currentfile->visibleName+" failed for the following reason:\n"+err, QMessageBox::Ok | QMessageBox::Abort, QMessageBox::Ok);
            if(ret==QMessageBox::Abort)
                if(m_prgdlg)
                    m_prgdlg->cancel();
        }
    }

    stopFileProgressDialog();
    m_prgdlg = NULL;

    gMW->updateWindowTitle();
}

void WFilesList::selectedFilesEstimateVoicedUnvoicedMarkers() {
    QList<QListWidgetItem*> l = selectedItems();

    // Get the f0 estimation range ...
    double tstart = -1;
    double tend = -1;
    if(gMW->m_gvWaveform->m_selection.width()>0.0){
        tstart = gMW->m_gvWaveform->m_selection.left();
        tend = gMW->m_gvWaveform->m_selection.right();
    }

    // These progress dialogs HAVE to be built on the stack otherwise ghost dialogs appear.
    QProgressDialog prgdlg("Estimating Voiced/Unvoiced markers...", "Abort", 0, l.size(), this);
    prgdlg.setMinimumDuration(500);
    m_prgdlg = &prgdlg;

    for(int i=0; i<l.size() && !m_prgdlg->wasCanceled(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        try {
            // If from a sound, generate a new F0 file
            if(currentfile->is(FileType::FTFZERO))
                gFL->addItem(new FTLabels(gFL, (FTFZero*)currentfile, tstart, tend));

            m_prgdlg->setValue(i);
        }
        catch(QString err){
            gMW->globalWaitingBarDone();
            stopFileProgressDialog();
            QMessageBox::StandardButton ret=QMessageBox::warning(gMW, "Error during Voiced/Unvoiced estimation", "Estimation of the Voiced/Unvoiced markers of "+currentfile->visibleName+" failed for the following reason:\n"+err, QMessageBox::Ok | QMessageBox::Abort, QMessageBox::Ok);
            if(ret==QMessageBox::Abort)
                if(m_prgdlg)
                    m_prgdlg->cancel();
        }
    }

    stopFileProgressDialog();
    m_prgdlg = NULL;

    gMW->updateWindowTitle();
}

double WFilesList::getFs() const {
    if(ftsnds.size()>0)
        return ftsnds[0]->fs;
    else
        return 44100.0;   // Fake a ghost sound using 44.1kHz sampling rate
}
unsigned int WFilesList::getMaxWavSize(){

    if(ftsnds.empty())
        return 44100;      // Fake a one second ghost sound of 44.1kHz sampling rate

    unsigned int s = 0;

    for(unsigned int fi=0; fi<ftsnds.size(); fi++)
        s = max(s, (unsigned int)(ftsnds[fi]->wav.size()));

    return s;
}
double WFilesList::getMaxDuration(){

    double dur = getMaxWavSize()/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        dur = std::max(dur, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        dur = std::max(dur, ftlabels[fi]->getLastSampleTime());

    return dur;
}
double WFilesList::getMaxLastSampleTime(){

    double lst = getMaxDuration()-1.0/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        lst = std::max(lst, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        lst = std::max(lst, ftlabels[fi]->getLastSampleTime());

    return lst;
}

WAVTYPE WFilesList::getMaxSQNR() const {
    if(ftsnds.size()==0)
        return -20*log10(std::numeric_limits<WAVTYPE>::min()); // -Inf would create issues in defining spectrogram range slider

    WAVTYPE maxsqnr = 0.0;
    for(unsigned int si=0; si<ftsnds.size(); si++){
        if(ftsnds[si]->format().sampleSize()==-1)
            maxsqnr = std::max(maxsqnr, 20*std::log10(std::pow(2.0, int(8*sizeof(WAVTYPE)))));
        else
            maxsqnr = std::max(maxsqnr, 20*std::log10(std::pow(2.0, ftsnds[si]->format().sampleSize())));
    }
    return maxsqnr;
}
