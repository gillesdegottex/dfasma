#ifndef WDIALOGSELECTCHANNEL_H
#define WDIALOGSELECTCHANNEL_H

#include <QDialog>

namespace Ui {
class WDialogSelectChannel;
}

class WDialogSelectChannel : public QDialog
{
    Q_OBJECT

public:
    explicit WDialogSelectChannel(const QString& fileName, int nchan, QWidget *parent = 0);
    ~WDialogSelectChannel();

    Ui::WDialogSelectChannel *ui;
private:
};

#endif // WDIALOGSELECTCHANNEL_H
