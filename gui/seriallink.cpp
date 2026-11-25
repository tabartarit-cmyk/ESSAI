#include "seriallink.h"

#include <QDebug>

#ifndef Q_OS_ANDROID
// ---------------------------------------------------------------------------
// Windows / Linux
// ---------------------------------------------------------------------------
#include <QSerialPortInfo>

SerialLink::SerialLink(QObject* parent)
    : QObject(parent), m_port(this)
{
    m_port.setBaudRate(QSerialPort::Baud115200);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setFlowControl(QSerialPort::HardwareControl);

    QObject::connect(&m_port, &QSerialPort::readyRead, this, &SerialLink::readyRead);
    QObject::connect(&m_port, &QSerialPort::errorOccurred, this,
                     [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        if (error == QSerialPort::ResourceError) {
            if (m_port.isOpen()) {
                m_port.close();
            }
            emit disconnected();
        } else {
            qDebug() << "Serial port error: " << error;
        }
    });
}

SerialLink::~SerialLink()
{
    close();
}

bool SerialLink::tryOpen()
{
    auto const ports = QSerialPortInfo::availablePorts();
    for (auto const& p : ports) {
        qDebug() << "Serial port description: " << p.description();
        if (p.vendorIdentifier() == vendorId && p.productIdentifier() == productId) {
            m_port.setPort(p);
            if (m_port.open(QIODevice::ReadWrite)) {
                m_port.setDataTerminalReady(true);
                qDebug() << "Serial port opened to " << p.description();
                return true;
            }
        }
    }
    return false;
}

void SerialLink::close()
{
    if (m_port.isOpen()) {
        m_port.setDataTerminalReady(false);
        m_port.close();
    }
}

bool SerialLink::isOpen() const
{
    return m_port.isOpen();
}

QByteArray SerialLink::readAll()
{
    return m_port.readAll();
}

qint64 SerialLink::write(char const* data, qint64 len)
{
    return m_port.write(data, len);
}

QString SerialLink::errorString() const
{
    return m_port.errorString();
}

#else
// ---------------------------------------------------------------------------
// Android : pont JNI vers org.picoasha.UsbSerialBridge
// ---------------------------------------------------------------------------
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>
#include <QMutexLocker>

namespace
{
constexpr char const* bridgeClass = "org/picoasha/UsbSerialBridge";

QMutex g_mutex;
QByteArray g_rx;
SerialLink* g_link = nullptr;

// Appelé depuis le thread de lecture Java
void JNICALL nativeOnData(JNIEnv* env, jclass, jbyteArray data, jint len)
{
    QByteArray chunk(len, Qt::Uninitialized);
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(chunk.data()));
    SerialLink* link = nullptr;
    {
        QMutexLocker lock(&g_mutex);
        g_rx.append(chunk);
        link = g_link;
    }
    if (link) {
        QMetaObject::invokeMethod(link, &SerialLink::readyRead, Qt::QueuedConnection);
    }
}

void JNICALL nativeOnClosed(JNIEnv*, jclass)
{
    SerialLink* link = nullptr;
    {
        QMutexLocker lock(&g_mutex);
        link = g_link;
    }
    if (link) {
        QMetaObject::invokeMethod(link, [link]() { link->onBridgeClosed(); }, Qt::QueuedConnection);
    }
}
} // namespace

SerialLink::SerialLink(QObject* parent)
    : QObject(parent)
{
    {
        QMutexLocker lock(&g_mutex);
        g_link = this;
    }
    static bool registered = false;
    if (!registered) {
        QJniEnvironment env;
        JNINativeMethod const methods[] = {
            {"nativeOnData", "([BI)V", reinterpret_cast<void*>(nativeOnData)},
            {"nativeOnClosed", "()V", reinterpret_cast<void*>(nativeOnClosed)},
        };
        registered = env.registerNativeMethods(bridgeClass, methods, 2);
        if (!registered) {
            qWarning() << "Cannot register the USB bridge native methods";
        }
    }
}

SerialLink::~SerialLink()
{
    close();
    QMutexLocker lock(&g_mutex);
    if (g_link == this) {
        g_link = nullptr;
    }
}

bool SerialLink::tryOpen()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    jint const r = QJniObject::callStaticMethod<jint>(
        bridgeClass, "open", "(Landroid/content/Context;)I", context.object());
    if (r == 1) {
        {
            QMutexLocker lock(&g_mutex);
            g_rx.clear();
        }
        m_open = true;
        return true;
    }
    if (r < 0) {
        m_error = QStringLiteral("USB: cannot open the Pico");
    }
    return false;   // 0 : pas de Pico, ou autorisation USB en attente
}

void SerialLink::close()
{
    if (m_open) {
        m_open = false;
        QJniObject::callStaticMethod<void>(bridgeClass, "close", "()V");
        QMutexLocker lock(&g_mutex);
        g_rx.clear();
    }
}

bool SerialLink::isOpen() const
{
    return m_open;
}

QByteArray SerialLink::readAll()
{
    QMutexLocker lock(&g_mutex);
    QByteArray out;
    out.swap(g_rx);
    return out;
}

qint64 SerialLink::write(char const* data, qint64 len)
{
    if (!m_open) {
        m_error = QStringLiteral("USB: not connected");
        return -1;
    }
    QJniEnvironment env;
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(len));
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(len), reinterpret_cast<jbyte const*>(data));
    jint const n = QJniObject::callStaticMethod<jint>(bridgeClass, "write", "([BI)I", arr,
                                                      static_cast<jint>(len));
    env->DeleteLocalRef(arr);
    if (n < 0) {
        m_error = QStringLiteral("USB: write failed");
        return -1;
    }
    return n;
}

QString SerialLink::errorString() const
{
    return m_error;
}

void SerialLink::onBridgeClosed()
{
    if (m_open) {
        m_open = false;
        emit disconnected();
    }
}
#endif
