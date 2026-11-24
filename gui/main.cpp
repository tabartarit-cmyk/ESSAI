#include <QApplication>
#include <QtSerialPort/QSerialPortInfo>
#include <QStringList>

#include "i18n.h"
#include "picoashacomm.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Langue de l'interface : choix de l'utilisateur, sinon langue de l'ordinateur
    i18n::install(app);

    PicoAshaComm picoASHA;

    picoASHA.showUI();
    return app.exec();
}
