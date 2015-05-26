#include "aboutbox.h"
#include "ui_aboutbox.h"

#include <QFile>
#include <QTextStream>
#include "qthelper.h"

#ifdef SUPPORT_SDIF
    #include <easdif/easdif.h>
#endif

AboutBox::AboutBox(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutBox)
{
    ui->setupUi(this);

    QString dfasmaversiongit(STR(DFASMAVERSIONGIT));

    QTextStream(stdout) << "'" << dfasmaversiongit << "'" << endl;

    QString	dfasmaversion;
    if(dfasmaversiongit.length()>0) {
        dfasmaversion = QString("Version ") + dfasmaversiongit;
    }
    else {
        QFile readmefile(":/README.txt");
        readmefile.open(QFile::ReadOnly | QFile::Text);
        QTextStream readmefilestream(&readmefile);
        readmefilestream.readLine();
        readmefilestream.readLine();
        dfasmaversion = readmefilestream.readLine().simplified();
    }
    QString txt = dfasmaversion;

    txt += " (compiled by "+QString(COMPILER)+" for ";
    #ifdef Q_PROCESSOR_X86_32
      txt += "32bits";
    #endif
    #ifdef Q_PROCESSOR_X86_64
      txt += "64bits";
    #endif
    txt += " on "+QString(__DATE__)+")";

    ui->lblVersion->setText(txt);

    txt = "";

    txt += "<h4>Purpose</h4>";
    txt += "<i>DFasma</i> is an open-source software whose main purpose is to compare waveforms in time and spectral domains. "; //  Even though there are a few scaling functionalities, DFasma is basically not an audio editor
    txt += "Its design is inspired by the <i>Xspect</i> software which was developed at <a href='http://www.ircam.fr'>Ircam</a>.</p>";
    // <a href='http://recherche.ircam.fr/equipes/analyse-synthese/DOCUMENTATIONS/xspect/xsintro1.2.html'>Xspect software</a>

    txt += "<p>To suggest a new functionality or report a bug, do not hesitate to <a href='https://github.com/gillesdegottex/dfasma/issues'>raise an issue on GitHub.</a></p>";

    txt += "<h4>Legal</h4>\
            Copyright &copy; 2014 Gilles Degottex <a href='mailto:gilles.degottex@gmail.com'>&lt;gilles.degottex@gmail.com&gt;</a><br/>\
            <i>DFasma</i> is coded in C++/<a href='http://qt-project.org'>Qt</a> under the <a href='http://www.gnu.org/licenses/gpl.html'>GPL (v3) License</a>.\
            The source code is hosted on <a href='https://github.com/gillesdegottex/dfasma'>GitHub</a>.";

    txt += "<h4>Disclaimer</h4>\
            ALL THE FUNCTIONALITIES OF <I>DFASMA</I> AND ITS CODE ARE PROVIDED WITHOUT ANY WARRANTY \
            (E.G. THERE IS NO WARRANTY OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE). \
            ALSO, THE COPYRIGHT HOLDERS AND CONTRIBUTORS DO NOT TAKE ANY LEGAL RESPONSIBILITY \
            REGARDING THE IMPLEMENTATIONS OF THE PROCESSING TECHNIQUES OR ALGORITHMS \
            (E.G. CONSEQUENCES OF BUGS OR ERRONEOUS IMPLEMENTATIONS).<br/> \
            Please see the README.txt file for additional information.";

    ui->txtAbout->setText(txt);


    // Libraries

    // FFT
    QString fftinfostr = "";
    fftinfostr += "<i>Fast Fourier Transform (FFT):</i> "+sigproc::FFTwrapper::getLibraryInfo();
    fftinfostr += " ("+QString::number(sizeof(FFTTYPE)*8)+"bits(";
    if(sizeof(FFTTYPE)==4)  fftinfostr += "single";
    if(sizeof(FFTTYPE)==8)  fftinfostr += "double";
    if(sizeof(FFTTYPE)==16)  fftinfostr += "quadruple";
    fftinfostr += "); smallest: "+QString::number(20*log10(std::numeric_limits<FFTTYPE>::min()))+"dB)";
    ui->vlLibraries->addWidget(new QLabel(fftinfostr));

    // SDIF
    QString sdifinfostr = "";
    #ifdef SUPPORT_SDIF
        sdifinfostr = "<i>For SDIF file format:</i> <a href=\"http://sdif.cvs.sourceforge.net/viewvc/sdif/Easdif/\">Easdif</a> version "+QString(EASDIF_VERSION_STRING);
    #else
        sdifinfostr = "<i>No support for SDIF file format</i>";
    #endif
    ui->vlLibraries->addWidget(new QLabel(sdifinfostr));

    ui->vlLibraries->addWidget(new QLabel("<i>For reading Audio files:</i> "+FTSound::getAudioFileReadingDescription()));
    QStringList list = FTSound::getAudioFileReadingSupportedFormats();
    for(QStringList::Iterator it=list.begin(); it!=list.end(); ++it)
        ui->listSupportedFormats->addItem(*it);

    //    QMessageBox::aboutQt(this, "About this software");
}

AboutBox::~AboutBox()
{
    delete ui;
}
