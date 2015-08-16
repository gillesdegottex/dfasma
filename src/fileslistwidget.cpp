#include "fileslistwidget.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ui_wdialogsettings.h"
#include "wdialogselectchannel.h"
#include "ui_wdialogselectchannel.h"
#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "gvspectrogram.h"
#include "filetype.h"
#include "../external/audioengine/audioengine.h"

#include <QFileInfo>
#include <QProgressDialog>
#include <QDir>
#include <QMessageBox>

#include <fstream>

#include "qthelper.h"

FilesListWidget* gFL = NULL;

FilesListWidget::FilesListWidget(QMainWindow *parent)
    : QListWidget(parent)
    , m_lastSelectedSound(NULL)
    , m_prgdlg(NULL)
{
    gFL = this;

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

void FilesListWidget::changeFileListItemsSize() {
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
// TODO Check if this is a distant file and avoid checking if it is ?
void FilesListWidget::checkFileModifications(){
//    cout << "GET FOCUS " << QDateTime::currentMSecsSinceEpoch() << endl;
    for(size_t fi=0; fi<ftsnds.size(); fi++)
        ftsnds[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftfzeros.size(); fi++)
        ftfzeros[fi]->checkFileStatus();
    for(size_t fi=0; fi<ftlabels.size(); fi++)
        ftlabels[fi]->checkFileStatus();

    gFL->fileInfoUpdate();
}

void FilesListWidget::stopFileProgressDialog() {
//    COUTD << "WMainWindow::stopFileProgressDialog " << m_prgdlg << endl;
    if(m_prgdlg){
        m_prgdlg->setValue(m_prgdlg->maximum());// Stop the loading bar
        m_prgdlg->hide();
    }
    QCoreApplication::processEvents(); // To show the progress
}

void FilesListWidget::addFile(FileType* ft) {

    if(ft->is(FileType::FTSOUND))
        ftsnds.push_back((FTSound*)ft);
    else if(ft->is(FileType::FTFZERO))
        ftfzeros.push_back((FTFZero*)ft);
    else if(ft->is(FileType::FTLABELS))
        ftlabels.push_back((FTLabels*)ft);

    addItem(ft);
}

void FilesListWidget::addExistingFilesRecursive(const QStringList& files, FileType::FType type) {
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

void FilesListWidget::addExistingFiles(const QStringList& files, FileType::FType type) {
//    COUTD << "WMainWindow::addFiles " << files.size() << endl;

    // These progress dialogs HAVE to be built on the stack otherwise ghost dialogs appear.
    QProgressDialog prgdlg("Opening files...", "Abort", 0, files.size(), this);
    prgdlg.setMinimumDuration(500);
    m_prgdlg = &prgdlg;

    addExistingFilesRecursive(files, type);

    stopFileProgressDialog();
    m_prgdlg = NULL;
}

void FilesListWidget::addExistingFile(const QString& filepath, FileType::FType type) {
//    cout << "INFO: Add file: " << filepath.toLocal8Bit().constData() << endl;

    if(QFileInfo(filepath).isDir())
        throw QString("Shoudn't use WMainWindow::addFile for directories, use WMainWindow::addFiles instead (with an 's')");

    try{
        bool isfirsts = ftsnds.size()==0;

        // Attention: There is the type of data stored in DFasma (FILETYPE) (ex. FileType::FTSOUND)
        //  and the format of the file (ex. FFSOUND)

        // This should be always "guessable"
        FileType::FileContainer container = FileType::guessContainer(filepath);

        // can be replaced by an autodetect or "forced mode" in the future

        // Then, guess the type of the data in the file, if no specified yet
        if(type==FileType::FTUNSET){
        // The format and the DFasma's type have to correspond
            if(container==FileType::FCANYSOUND)
                type = FileType::FTSOUND; // These two have to match

            #ifdef SUPPORT_SDIF
            if(container==FileType::FCSDIF) {
                if(FileType::SDIF_hasFrame(filepath, "1FQ0"))
                    type = FileType::FTFZERO;
                else if (FileType::SDIF_hasFrame(filepath, "1MRK"))
                    type = FileType::FTLABELS;
                else
                    throw QString("Unsupported SDIF data.");
            }
            #endif
            if(container==FileType::FCTEXT) {
                // Distinguish between f0, labels and future VUF files (and futur others ...)
                // Do a grammar check (But this won't help to diff F0 and VUF files)
                std::ifstream fin(filepath.toLatin1().constData());
                if(!fin.is_open())
                    throw QString("Cannot open this file");
                double t;
                std::string line, text;
                // Check the first line only (Assuming it is enough)
                // TODO May have to skip commented lines depending on the text format
                if(!std::getline(fin, line))
                    throw QString("There is not a single line in this file");
                // Check: <number> <number>
                std::istringstream iss(line);
                // Check if first two values are real numbers
                // TODO This will NOT work to distinguish F0 or VUF !!
                if((iss >> t >> t) && iss.eof())
                    type = FileType::FTFZERO;
                else // Otherwise, assume this is a label
                    type = FileType::FTLABELS;
            }
        }

        if(type==FileType::FTUNSET)
            throw QString("Cannot find any data or audio channel in this file that is handled by this distribution of DFasma.");

        // Finally, load the data knowing the type and the container
        if(type==FileType::FTSOUND){
            int nchan = FTSound::getNumberOfChannels(filepath);
            if(nchan==1){
                // If there is only one channel, just load it
                addFile(new FTSound(filepath, this));
            }
            else{
                // If more than one channel, ask what to do
                stopFileProgressDialog();
                WDialogSelectChannel dlg(filepath, nchan, this);
                if(dlg.exec()) {
                    if(dlg.ui->rdbImportEachChannel->isChecked()){
                        for(int ci=1; ci<=nchan; ci++){
                            addFile(new FTSound(filepath, this, ci));
                        }
                    }
                    else if(dlg.ui->rdbImportOnlyOneChannel->isChecked()){
                        addFile(new FTSound(filepath, this, dlg.ui->sbChannelID->value()));
                    }
                    else if(dlg.ui->rdbMergeAllChannels->isChecked()){
                        addFile(new FTSound(filepath, this, -2));// -2 is a code for merging the channels
                    }
                }
            }

            if(ftsnds.size()>0){
                // The first sound determines the common sampling frequency for the audio output
                if(isfirsts)
                    gMW->initializeSoundSystem(ftsnds[0]->fs);

                gMW->m_gvWaveform->fitViewToSoundsAmplitude();
            }
        }
        else if(type == FileType::FTFZERO)
            addFile(new FTFZero(filepath, this, container));
        else if(type == FileType::FTLABELS)
            addFile(new FTLabels(filepath, this, container));
    }
    catch (QString err)
    {
        stopFileProgressDialog();
        QMessageBox::StandardButton ret=QMessageBox::warning(this, "Failed to load file ...", "Data from the following file can't be loaded:\n"+filepath+"'\n\nReason:\n"+err, QMessageBox::Ok | QMessageBox::Abort, QMessageBox::Ok);
        if(ret==QMessageBox::Abort)
            if(m_prgdlg)
                m_prgdlg->cancel();
    }
}

FileType* FilesListWidget::currentFile() const {
    return (FileType*)(currentItem());
}

FTSound* FilesListWidget::getCurrentFTSound(bool forceselect) {

    if(ftsnds.empty())
        return NULL;

    FileType* currenItem = currentFile();

    if(currenItem && currenItem->is(FileType::FTSOUND))
        return (FTSound*)currenItem;

    if(forceselect){
        if(m_lastSelectedSound)
            return m_lastSelectedSound;
        else
            return ftsnds[0];
    }

    return NULL;
}

FTLabels* FilesListWidget::getCurrentFTLabels(bool forceselect) {

    if(ftlabels.empty())
        return NULL;

    FileType* currenItem = currentFile();

    if(currenItem && currenItem->is(FileType::FTLABELS))
        return (FTLabels*)currenItem;

    if(forceselect)
        return ftlabels[0];

    return NULL;
}


void FilesListWidget::showFileContextMenu(const QPoint& pos) {

    QMenu contextmenu(this);

    QList<QListWidgetItem*> l = selectedItems();
    if(l.size()==1) {
        FileType* currenItem = currentFile();
        currenItem->fillContextMenu(contextmenu);
    }
    else {
        contextmenu.addAction(gMW->ui->actionSelectedFilesToggleShown);
        contextmenu.addAction(gMW->ui->actionSelectedFilesReload);
        contextmenu.addAction(gMW->ui->actionSelectedFilesClose);
    }

    int contextmenuheight = contextmenu.sizeHint().height();
    QPoint posglobal = mapToGlobal(pos)+QPoint(24,contextmenuheight/2);
    contextmenu.exec(posglobal);
}

void FilesListWidget::fileSelectionChanged() {
//    COUTD << "WMainWindow::fileSelectionChanged" << endl;

    QList<QListWidgetItem*> list = selectedItems();
    int nb_snd_in_selection = 0;
    int nb_labels_in_selection = 0;
    int nb_f0_in_selection = 0;

    for(int i=0; i<list.size(); i++) {
        FileType* ft = ((FileType*)list.at(i));
        if(ft->is(FileType::FTSOUND)){
            nb_snd_in_selection++;
            m_lastSelectedSound = (FTSound*)ft;
        }

        if(ft->is(FileType::FTLABELS))
            nb_labels_in_selection++;

        if(ft->is(FileType::FTFZERO))
            nb_f0_in_selection++;
    }

    // Update the spectrogram to current selected signal
    if(nb_snd_in_selection>0){
        if(gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop){
            gMW->m_gvWaveform->m_scene->update();
            gMW->m_gvAmplitudeSpectrum->m_scene->update();
            gMW->m_gvPhaseSpectrum->m_scene->update();
        }
        gMW->m_gvSpectrogram->updateSTFTPlot();
        gMW->m_gvSpectrogram->m_scene->update();
    }

    gMW->ui->actionSelectedFilesSave->setEnabled(nb_labels_in_selection>0 || nb_f0_in_selection>0);

    fileInfoUpdate();

//    COUTD << "WMainWindow::~fileSelectionChanged" << endl;
}
void FilesListWidget::fileInfoUpdate() {
    QList<QListWidgetItem*> list = selectedItems();

    // If only one file selected
    // Display Basic information of it
    if(list.size()==1) {
        gMW->ui->lblFileInfo->setText(((FileType*)list.at(0))->info());
        gMW->ui->lblFileInfo->show();
    }
    else
        gMW->ui->lblFileInfo->hide();
}

// FileType is not an qobject, thus, need to forward the message manually (i.e. without signal system).
void FilesListWidget::colorSelected(const QColor& color) {
    FileType* currenItem = (FileType*)(currentItem());
    if(currenItem)
        currenItem->setColor(color);
}
void FilesListWidget::resetAmpScale(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_ampscale = 1.0;

        currentftsound->setStatus();

        gMW->allSoundsChanged();
    }
}
void FilesListWidget::resetDelay(){
    FTSound* currentftsound = getCurrentFTSound();
    if(currentftsound) {
        currentftsound->m_delay = 0.0;

        currentftsound->setStatus();

        gMW->allSoundsChanged();
    }
}

void FilesListWidget::selectedFilesToggleShown() {
    QList<QListWidgetItem*> list = selectedItems();
    for(int i=0; i<list.size(); i++){
        ((FileType*)list.at(i))->setVisible(!((FileType*)list.at(i))->m_actionShow->isChecked());
        if(((FileType*)list.at(i))==m_lastSelectedSound)
            gMW->m_gvSpectrogram->updateSTFTPlot();
    }
    gMW->m_gvWaveform->m_scene->update();
    gMW->m_gvSpectrogram->m_scene->update();
    gMW->m_gvAmplitudeSpectrum->updateDFTs();
    gMW->m_gvAmplitudeSpectrum->m_scene->update();
    gMW->m_gvPhaseSpectrum->m_scene->update();
}

void FilesListWidget::selectedFilesClose() {
    gMW->m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = selectedItems();
    clearSelection();

    bool removeSelectedSound = false;

    for(int i=0; i<l.size(); i++){

        FileType* ft = (FileType*)l.at(i);

//        cout << "INFO: Closing file: \"" << ft->fileFullPath.toLocal8Bit().constData() << "\"" << endl;

        delete ft; // Remove it from the listview

        // Remove it from its own type-related list
        if(ft->is(FileType::FTSOUND)){
            if(ft==m_lastSelectedSound){
                removeSelectedSound = true;
                m_lastSelectedSound = NULL;
            }
            ftsnds.erase(std::find(ftsnds.begin(), ftsnds.end(), (FTSound*)ft));
        }
        else if(ft->is(FileType::FTFZERO))
            ftfzeros.erase(std::find(ftfzeros.begin(), ftfzeros.end(), (FTFZero*)ft));
        else if(ft->is(FileType::FTLABELS))
            ftlabels.erase(std::find(ftlabels.begin(), ftlabels.end(), (FTLabels*)ft));
    }

    gMW->updateWindowTitle();

    if(gMW->m_gvWaveform->m_scene->sceneRect().right()>gFL->getMaxDuration()+1.0/gFL->getFs()) {
        gMW->m_gvWaveform->updateSceneRect();
        gMW->m_gvSpectrogram->updateSceneRect();
        gMW->m_gvWaveform->setMouseCursorPosition(-1, true);
        gMW->m_gvWaveform->viewSet(gMW->m_gvWaveform->m_scene->sceneRect(), true);
    }

    gMW->m_gvSpectrogram->m_scene->update();

    if(removeSelectedSound)
        gMW->m_gvSpectrogram->clearSTFTPlot();

    // If there is no more files, put the interface in a waiting-for-file state.
    if(count()==0)
        gMW->setInWaitingForFileState();
    else
        gMW->allSoundsChanged();
}

void FilesListWidget::selectedFilesReload() {
//    COUTD << "WMainWindow::selectedFileReload" << endl;

    gMW->m_audioengine->stopPlayback();

    QList<QListWidgetItem*> l = selectedItems();

    bool reloadSelectedSound = false;
    bool didanysucceed = false;

    for(int i=0; i<l.size(); i++) {

        FileType* ft = (FileType*)l.at(i);

        if(ft->reload())
            didanysucceed = true;

        if(ft==m_lastSelectedSound)
            reloadSelectedSound = true;
    }

    fileInfoUpdate();

    if(didanysucceed && reloadSelectedSound) {
        gMW->m_gvWaveform->m_scene->update();
        gMW->m_gvAmplitudeSpectrum->updateDFTs();
        gMW->m_gvSpectrogram->updateSTFTPlot(true); // Force the STFT computation
    }

//    COUTD << "WMainWindow::~selectedFileReload" << endl;
}

void FilesListWidget::selectedFilesDuplicate() {
    QList<QListWidgetItem*> l = selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        FileType* ft = currentfile->duplicate();
        if(ft){
            addFile(ft);
            gMW->m_gvWaveform->updateSceneRect();
            gMW->allSoundsChanged();
            gMW->ui->actionSelectedFilesClose->setEnabled(true);
            gMW->ui->actionSelectedFilesReload->setEnabled(true);
            gMW->ui->splitterViews->show();
            gMW->updateWindowTitle();
        }
    }
}

void FilesListWidget::selectedFilesSave() {
    QList<QListWidgetItem*> l = selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        if(currentfile->is(FileType::FTLABELS))
            ((FTLabels*)currentfile)->save();

        if(currentfile->is(FileType::FTFZERO))
            ((FTFZero*)currentfile)->save();
    }
}

void FilesListWidget::selectedFilesEstimateF0() {
    QList<QListWidgetItem*> l = selectedItems();

    for(int i=0; i<l.size(); i++) {
        FileType* currentfile = (FileType*)l.at(i);

        if(currentfile->is(FileType::FTSOUND))
            ((FTSound*)currentfile)->estimateFZero();
    }
}

double FilesListWidget::getFs() const {
    if(ftsnds.size()>0)
        return ftsnds[0]->fs;
    else
        return 44100.0;   // Fake a ghost sound using 44.1kHz sampling rate
}
unsigned int FilesListWidget::getMaxWavSize(){

    if(ftsnds.empty())
        return 44100;      // Fake a one second ghost sound of 44.1kHz sampling rate

    unsigned int s = 0;

    for(unsigned int fi=0; fi<ftsnds.size(); fi++)
        s = max(s, (unsigned int)(ftsnds[fi]->wav.size()));

    return s;
}
double FilesListWidget::getMaxDuration(){

    double dur = getMaxWavSize()/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        dur = std::max(dur, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        dur = std::max(dur, ftlabels[fi]->getLastSampleTime());

    return dur;
}
double FilesListWidget::getMaxLastSampleTime(){

    double lst = getMaxDuration()-1.0/getFs();

    for(unsigned int fi=0; fi<ftfzeros.size(); fi++)
        lst = std::max(lst, ftfzeros[fi]->getLastSampleTime());

    for(unsigned int fi=0; fi<ftlabels.size(); fi++)
        lst = std::max(lst, ftlabels[fi]->getLastSampleTime());

    return lst;
}
