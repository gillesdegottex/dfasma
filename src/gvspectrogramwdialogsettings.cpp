#include "gvspectrogramwdialogsettings.h"
#include "ui_gvspectrogramwdialogsettings.h"

#include "gvspectrogram.h"

#include <QSettings>

GVSpectrogramWDialogSettings::GVSpectrogramWDialogSettings(QGVSpectrogram* parent) :
    QDialog((QWidget*)parent),
    ui(new Ui::GVSpectrogramWDialogSettings)
{
    ui->setupUi(this);

    m_spectrogram = parent;

    gMW->m_settings.add(ui->cbSpectrogramWindowSizeForcedOdd);
    gMW->m_settings.add(ui->cbSpectrogramWindowType);
    gMW->m_settings.add(ui->spSpectrogramWindowNormPower);
    gMW->m_settings.add(ui->spSpectrogramWindowNormSigma);
    gMW->m_settings.add(ui->spSpectrogramWindowExpDecay);
    ui->lblWindowNormSigma->hide();
    ui->spSpectrogramWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spSpectrogramWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spSpectrogramWindowExpDecay->hide();
    gMW->m_settings.add(ui->sbSpectrogramStepSize);
    gMW->m_settings.add(ui->sbSpectrogramWindowSize);
    gMW->m_settings.add(ui->sbSpectrogramOversamplingFactor);
    gMW->m_settings.add(ui->gbSpectrogramCepstralLiftering);
    gMW->m_settings.add(ui->sbSpectrogramCepstralLifteringOrder);
    gMW->m_settings.add(ui->cbSpectrogramCepstralLifteringPreserveDC);
    QStringList colormaps = ColorMap::getAvailableColorMaps();
    for(QStringList::Iterator it=colormaps.begin(); it!=colormaps.end(); ++it)
        ui->cbSpectrogramColorMaps->addItem(*it);
    gMW->m_settings.add(ui->cbSpectrogramColorMaps);
    gMW->m_settings.add(ui->cbSpectrogramColorMapReversed);

    checkImageSize();
    adjustSize();

    connect(ui->cbSpectrogramWindowType, SIGNAL(currentIndexChanged(QString)), this, SLOT(CBSpectrumWindowTypeCurrentIndexChanged(QString)));
}

void GVSpectrogramWDialogSettings::checkImageSize() {

    int maxsampleindex = int(gMW->getMaxWavSize())-1;

    int stepsize = std::floor(0.5+gMW->getFs()*ui->sbSpectrogramStepSize->value());//[samples]
    int winlen = std::floor(0.5+gMW->getFs()*ui->sbSpectrogramWindowSize->value());
    if(winlen%2==0 && ui->cbSpectrogramWindowSizeForcedOdd->isChecked())
        winlen++;
    int dftlen = pow(2, std::ceil(log2(float(winlen)))+ui->sbSpectrogramOversamplingFactor->value());//[samples]

    ui->lblActualWindowLength->setText(QString("%2s(%1)").arg(winlen).arg(double(winlen)/gMW->getFs()));
    ui->lblDFTSize->setText(QString("DFT size=%1").arg(dftlen));
    ui->lblActualStepSize->setText(QString("%2s(%1)").arg(stepsize).arg(double(stepsize)/gMW->getFs()));

    int imgheight = dftlen/2+1;
    int imgwidth = int(1+double(maxsampleindex+1-winlen)/stepsize);

    long int size = double(imgwidth)*imgheight*sizeof(QImage::Format_RGB32)/(1024.0*1024.0);

    QString text = "<html><head/><body>";
    text += QString("Image size: %1x%2 = %3Mb").arg(imgwidth).arg(imgheight).arg(size);

    if(imgwidth>32768 || imgheight>32768){
        text += "<br/><font color=\"red\">Image dimensions need to be smaller than 32768.<br/>You can: increase step size, reduce window length, reduce oversampling factor,<br/>or try it! (visual artefacts expected)</font>";
    }
    text += "<br/></body></html>";
    ui->lblImgSizeWarning->setText(text);
}

void GVSpectrogramWDialogSettings::CBSpectrumWindowTypeCurrentIndexChanged(QString txt) {
    ui->lblWindowNormSigma->hide();
    ui->spSpectrogramWindowNormSigma->hide();
    ui->lblWindowNormPower->hide();
    ui->spSpectrogramWindowNormPower->hide();
    ui->lblWindowExpDecay->hide();
    ui->spSpectrogramWindowExpDecay->hide();
    ui->lblWindowNormSigma->setToolTip("");
    ui->spSpectrogramWindowNormSigma->setToolTip("");

    if(txt=="Generalized Normal") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("sigma=");
        ui->lblWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->spSpectrogramWindowNormSigma->show();
        ui->spSpectrogramWindowNormSigma->setToolTip("Warning! If using the Generalized Normal window, sigma=sqrt(2)*std, thus, not equivalent to the standard-deviation of the Gaussian window.");
        ui->lblWindowNormPower->show();
        ui->spSpectrogramWindowNormPower->show();
    }
    else if(txt=="Gaussian") {
        ui->lblWindowNormSigma->show();
        ui->lblWindowNormSigma->setText("standard-deviation=");
        ui->lblWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
        ui->spSpectrogramWindowNormSigma->show();
        ui->spSpectrogramWindowNormSigma->setToolTip("The standard-deviation relative to the half window size");
    }
    else if(txt=="Exponential") {
        ui->lblWindowExpDecay->show();
        ui->spSpectrogramWindowExpDecay->show();
    }

    adjustSize();
}

GVSpectrogramWDialogSettings::~GVSpectrogramWDialogSettings()
{
    delete ui;
}
