#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#ifndef Q_OS_ANDROID
#include <QSerialPort>
#endif

// Liaison série USB avec la Pico (VID 0xCAFE, PID 0x4011).
//  - Windows et Linux : port série (QSerialPort)
//  - Android          : API USB Host du système, via org.picoasha.UsbSerialBridge (Java)
class SerialLink : public QObject
{
    Q_OBJECT
public:
    static constexpr quint16 vendorId = 0xCAFE;
    static constexpr quint16 productId = 16401;

    explicit SerialLink(QObject* parent = nullptr);
    ~SerialLink() override;

    // Cherche la Pico et ouvre la liaison. Renvoie true si elle est ouverte.
    // (Android : renvoie false tant que l'utilisateur n'a pas accepté l'accès USB.)
    bool tryOpen();
    void close();
    bool isOpen() const;
    QByteArray readAll();
    qint64 write(char const* data, qint64 len);
    QString errorString() const;

#ifdef Q_OS_ANDROID
    void onBridgeClosed();   // appelé quand le câble est débranché
#endif

signals:
    void readyRead();
    void disconnected();     // liaison perdue (câble débranché, Pico redémarrée...)

private:
#ifdef Q_OS_ANDROID
    bool m_open = false;
    QString m_error;
#else
    QSerialPort m_port;
#endif
};
