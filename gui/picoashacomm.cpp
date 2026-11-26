#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QMap>
#include <QStandardPaths>
#include <QUrl>
#include <QtEndian>

#include <nanocobs/cobs.h>

#include "bt_status_err.hpp"

#include "picoashacomm.h"

constexpr int timer_interval = 1000;

QMap<QByteArray, RemoteDevice::CachedProps> cached_remote_props;

// Windows : à côté du programme (comportement d'origine). Linux et Android : dossier de données
// de l'application (le dossier courant n'est pas toujours inscriptible, par ex. dans un AppImage).
static QString writableFilePath(QString const& name)
{
#ifdef Q_OS_WIN
    return name;
#else
    QString const dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(name);
#endif
}

PicoAshaComm::PicoAshaComm(QObject *parent)
    : QObject{parent}, m_serial(this), connect_timer(this), m_hciLoggingEnabled(false)
{
    m_ui = new PicoAshaMainWindow;
    m_ui->setHciActionBtnStart(false);

    QObject::connect(&connect_timer, &QTimer::timeout, this, &PicoAshaComm::onConnectTimer);
    QObject::connect(&m_serial, &SerialLink::disconnected, this, &PicoAshaComm::onSerialDisconnected);
    QObject::connect(&m_serial, &SerialLink::readyRead, this, &PicoAshaComm::onSerialReadyRead);

    QObject::connect(&intro_timer, &QTimer::timeout, this, &PicoAshaComm::onIntroTimer);

    QObject::connect(m_ui, &PicoAshaMainWindow::hciLogActionBtnClicked, this, &PicoAshaComm::onHciLogActionBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::cmdRestartBtnClicked, this, &PicoAshaComm::onCmdRestartBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::cmdConnAllowedBtnClicked, this, &PicoAshaComm::onCmdConnAllowedBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::cmdStreamingEnabledBtnClicked, this, &PicoAshaComm::onCmdStreamingEnabledBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::cmdRemoveBondBtnClicked, this, &PicoAshaComm::onCmdRemoveBondBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::usbSettingsBtnClicked, this, &PicoAshaComm::onUsbSettingsBtnClicked);
    QObject::connect(m_ui, &PicoAshaMainWindow::pairWithAddress, this, &PicoAshaComm::onPairWithAddress);

    connect_timer.start(timer_interval);

    m_logFile.setFileName(writableFilePath("pico-asha.log"));
    m_logOpened = m_logFile.open(QFile::WriteOnly | QFile::Truncate);
    if (m_logOpened) {
        m_logStream.setDevice(&m_logFile);
    }

}

PicoAshaComm::~PicoAshaComm()
{
    if (m_logOpened) {
        m_logOpened = false;
        m_logStream.flush();
        m_logFile.close();
    }
    closeHciLog();
    closeSerial();
    delete m_ui;
}

void PicoAshaComm::onConnectTimer()
{
    if (m_serial.tryOpen()) {
        connect_timer.stop();
        m_serialConnected = true;
        m_ui->onSerialConnected(true);
        intro_timer.start(250);
        sendCommandPacket(
            {
                .cmd = asha::comm::Command::IntroPacket,
                .cmd_status = asha::comm::CmdStatus::CmdOk,
                .data = {}
            }
        );
    }
}

void PicoAshaComm::onIntroTimer()
{
    qDebug() << "Intro packet timeout reached";
    closeSerial();
    m_serialConnected = false;
    m_ui->onSerialConnected(false);
    m_ui->removeRemotes();
    if (!m_hciLoggingEnabled) {
        m_ui->setHciActionBtnStart(false);
    }
    intro_timer.stop();
    connect_timer.start(timer_interval);
}

void PicoAshaComm::onSerialDisconnected()
{
    m_serialConnected = false;
    m_serial.close();
    m_ui->onSerialConnected(false);
    m_ui->removeRemotes();
    if (!m_hciLoggingEnabled) {
        m_ui->setHciActionBtnStart(false);
    }
    qDebug() << "Serial port disconnected";
    connect_timer.start(timer_interval);
}

void PicoAshaComm::onSerialReadyRead()
{
    QByteArray const data = m_serial.readAll();
    for (const auto& b : data) {
        if (b == '\0') {
            if (m_currPacket.size() >= 4) {
                QByteArray decoded(COBS_TINYFRAME_SAFE_BUFFER_SIZE, '\0');
                size_t dec_len;
                cobs_decode(m_currPacket.data(), m_currPacket.size(), decoded.data(), decoded.capacity(), &dec_len);
                decoded.resize(dec_len);
                handleDecodedData(decoded);
            }
            m_currPacket.clear();
        } else {
            m_currPacket.append(b);
        }
    }
}

void PicoAshaComm::onHciLogActionBtnClicked()
{
    if (m_hciLoggingEnabled) {
        stopHciLogging();
        sendHciDumpCommand(false);
        return;
    }
    // Start: use the file chosen with the Browse button, or ask for one.
    QString path = m_ui->hciLogPath();
#ifdef Q_OS_ANDROID
    if (path.isEmpty()) {
        path = writableFilePath("HCI.log");
        m_ui->setHciLogPath(path);
    }
#else
    if (path.isEmpty()) {
        path = m_ui->askHciLogPath();
    }
#endif
    if (path.isEmpty()) {
        return;     // cancelled
    }
    if (startHciLogging(path)) {
        sendHciDumpCommand(true);
    }
}

void PicoAshaComm::onCmdRestartBtnClicked()
{
    using namespace asha::comm;
    bool res = sendCommandPacket(
        {
            .cmd = Command::Restart,
            .cmd_status = CmdStatus::CmdOk,
            .data = {}
        }
        );
    if (res) {
        m_ui->onSerialConnected(false);
    }
}

void PicoAshaComm::onCmdConnAllowedBtnClicked(bool allowed)
{
    using namespace asha::comm;
    bool res = sendCommandPacket(
        {
            .cmd = Command::AllowConnect,
            .cmd_status = CmdStatus::CmdOk,
            .data = {.allow_connect = allowed}
        }
        );
    if (res) {
        m_ui->setConnectionsAllowed(allowed);
    }
}

void PicoAshaComm::onCmdStreamingEnabledBtnClicked(bool enabled)
{
    using namespace asha::comm;
    bool res = sendCommandPacket(
        {
            .cmd = Command::AudioStreaming,
            .cmd_status = CmdStatus::CmdOk,
            .data = {.audio_streaming_enabled = enabled}
        }
        );
    if (res) {
        m_ui->setAudioStreamingEnabled(enabled);
    }
}

void PicoAshaComm::onCmdRemoveBondBtnClicked()
{
    using namespace asha::comm;
    sendCommandPacket(
        {
            .cmd = Command::DeletePair,
            .cmd_status = CmdStatus::CmdOk,
            .data = {}
        }
    );
}

void PicoAshaComm::onUsbSettingsBtnClicked(const asha::comm::USBInfo &usb_info)
{
    using namespace asha::comm;
    bool res = sendCommandPacket(
        {
            .cmd = Command::USBSettings,
            .cmd_status = CmdStatus::CmdOk,
            .data = {.usb_settings = usb_info}
        }
        );
    if (res) {
        m_ui->setUSBInfo(usb_info);
        // The Pico restarts to apply the settings (if they changed). If it does
        // not restart within 30 s, forget about the resynchronisation.
        m_audioResyncAfterReboot = true;
        QTimer::singleShot(30000, this, [this]() { m_audioResyncAfterReboot = false; });
    }
}

void PicoAshaComm::onPairWithAddress(const QByteArray &addr, uint8_t addr_type)
{
    using namespace asha::comm;
    CmdPacket pkt = {
        .cmd = Command::PairBond,
        .cmd_status = CmdStatus::CmdOk,
        .data = {.pair_bond = {.addr = {}, .addr_type = addr_type}}
    };
    memcpy(pkt.data.pair_bond.addr, addr.data(), addr.size());
    sendCommandPacket(pkt);
}

template<typename T>
bool assert_packet_size(size_t dec_size, const char* pkt_type, T const& pkt)
{
    if (dec_size < sizeof(asha::comm::HeaderPacket)) {
        qDebug() << pkt_type << ": Size mismatch! Decoded size is "
                 << dec_size << ", which is too small for a header";
        return false;
    }
    size_t dec_pkt_size = dec_size - sizeof(asha::comm::HeaderPacket);
    if (dec_pkt_size < sizeof pkt) {
        qDebug() << pkt_type << ": Size mismatch! Decoded size is "
                 << dec_size << ", which is too small for the expected: " << sizeof(pkt)
                 << ", got: " << dec_pkt_size;
        return false;
    } else if (dec_pkt_size > sizeof pkt) {
        qDebug() << pkt_type << ": Warning: decoded size of: "
                 << dec_pkt_size << " is larger than the expected size of: "
                 << sizeof(pkt);
    }
    return true;
}

void PicoAshaComm::handleDecodedData(QByteArray const& decoded)
{
    using namespace asha::comm;
    HeaderPacket header(Type::Event);
    memcpy(&header, decoded.constData(), sizeof header);
    if (header.len != decoded.size()) {
        qDebug() << "Decoded packet does not match length!";
        return;
    }

    // qDebug() << "\nHeader Type: " << static_cast<uint8_t>(header.type)
    //          << "\nHeader Conn ID: " << header.conn_id
    //          << "\nHeader Len: " << header.len
    //          << "\nHeader TS: " << header.ts_ms
    //          << "\n";

    switch(header.type) {
    case Type::Cmd:
        break;
    case Type::Intro: {
        IntroPacket intro;
        if (!assert_packet_size(decoded.size(), "IntroPacket", intro)) {
            return;
        }
        intro_timer.stop();
        memcpy(&intro, decoded.constData() + sizeof header, sizeof intro);
        setPaFirmwareVers(QString("%1.%2.%3").arg(
                                                  QString::number(intro.pa_version.major),
                                                  QString::number(intro.pa_version.minor),
                                                  QString::number(intro.pa_version.patch))
                          );

        qDebug() << "Num connected devices: " << intro.num_connected;
        m_ui->setPicoAshaVerStr(paFirmwareVers());
        m_ui->setConnectionsAllowed(intro.test_flag(IntroFlags::conn_allowed));
        m_ui->setAudioStreamingEnabled(intro.test_flag(IntroFlags::streaming_enabled));
        m_ui->setCmdBtnsEnabled(true);

        // HCI logging is never started automatically: the HCI button becomes
        // available once the Pico is connected.
        if (!m_hciLoggingEnabled) {
            m_ui->setHciActionBtnStart(true);
        }

        // The Pico has just restarted after a USB settings Update: once both
        // hearing aids are connected again, restart the audio stream.
        if (m_audioResyncAfterReboot) {
            m_audioResyncAfterReboot = false;
            m_audioResyncWaiting = true;
            QTimer::singleShot(60000, this, [this]() { m_audioResyncWaiting = false; });
        }
        break;
    }
    case Type::USBInfo:
        USBInfo usb_info;
        if (!assert_packet_size(decoded.size(), "USBInfo", usb_info)) {
            return;
        }
        memcpy(&usb_info, decoded.constData() + sizeof header, sizeof usb_info);
        qDebug() << "\nGot USBInfo packet";
        m_ui->setUSBInfo(usb_info);
        m_ui->setUSBWidgetsEnabled(true);
        break;
    case Type::Event: {
        EventPacket pkt(EventType::ShortLog);
        if (!assert_packet_size(decoded.size(), "EventPacket", pkt)) {
            return;
        }
        memcpy(&pkt, decoded.constData() + sizeof header, sizeof pkt);
        handleEventPacket(header, pkt);
        break;
    }
    case Type::HCI:
        writeHciPacket(decoded.constData() + sizeof header, decoded.size() - sizeof header);
        break;
    case Type::RemInfo:
        RemoteInfo info;
        if (!assert_packet_size(decoded.size(), "RemoteInfo", info)) {
            return;
        }
        qDebug() << "\nGot RemoteInfo packet";
        memcpy(&info, decoded.constData() + sizeof header, sizeof info);
        m_ui->addRemote(info);
        maybeScheduleAudioResync();
        break;
    case Type::Advert:
        AdvertisingPacket ad;
        if (!assert_packet_size(decoded.size(), "AdvertisingPacket", ad)) {
            return;
        }
        qDebug() << "Got Advertising packet";
        memcpy(&ad, decoded.constData() + sizeof header, sizeof ad);
        m_ui->onAdPacketReceived(ad);
        break;
    }
}

void PicoAshaComm::handleEventPacket(asha::comm::HeaderPacket const header, asha::comm::EventPacket const& pkt)
{
    using namespace asha::comm;
    auto rem = m_ui->getRemote(header.conn_id);

    switch (pkt.ev_type) {

    case EventType::ShortLog:
        appendLog(QString("%1 - %2").arg(logHeader(header)).arg(pkt.data.str));
        break;
    case EventType::PicoASHAInit:
        if (checkError(header, pkt, "PicoASHAInit")) {
            return;
        }
        break;
    case EventType::DeletePair:
        if (checkError(header, pkt, "DeletePair")) {
            return;
        }
        break;
    case EventType::RemoteConnected: {
        if (checkError(header, pkt, "RemoteConnected")) {
            return;
        }
        if (rem) {
            qDebug() << "Remote with this ID already exists";
            return;
        }
        qDebug() << "Remote Connected";
        auto r = m_ui->addRemote(header.conn_id);
        if (r) {
            QByteArray addr((const char*)pkt.data.conn_info.addr, sizeof(pkt.data.conn_info.addr));
            r->setHCIHandle(pkt.data.conn_info.hci_handle);
            r->setAddr(pkt.data.conn_info.addr);
            if (cached_remote_props.contains(addr)) {
                auto const& props = cached_remote_props.value(addr);
                r->setCachedProps(props);
            }
        }
        maybeScheduleAudioResync();
        break;
    }
    case EventType::RemoteDisconnected:
        checkError(header, pkt, "RemoteDisconnected");
        qDebug() << "Removing Conn ID: " << header.conn_id;
        if (rem) {
            auto props = rem->cachedProps();
            cached_remote_props.insert(props.addr, props);
        }
        m_ui->removeRemote(header.conn_id);
        break;
    case EventType::DiscServices:
        if (checkError(header, pkt, "DiscoverServices")) {
            return;
        }
        break;
    case EventType::PairAndBond:
        if (checkError(header, pkt, "PairAndBond")) {
            return;
        }
        if (rem) rem->setPairedBonded(true);
        break;
    case EventType::DLE:
        if (checkError(header, pkt, "DataLenExtension")) {
            return;
        }
        break;
    case EventType::DiscASHAChar:
        if (checkError(header, pkt, "DiscoverASHAChar")) {
            return;
        }
        break;
    case EventType::ROPRead:
        if (checkError(header, pkt, "ROPRead")) {
            return;
        }
        if (rem) {
            rem->setROPInfo((const char*)pkt.data.rop);
            m_ui->setRemoteOrder();
        }
        break;
    case EventType::PSMRead:
        if (checkError(header, pkt, "PSMRead")) {
            return;
        }
        if (rem) rem->setPsm(pkt.data.psm);
        break;
    case EventType::DiscGAPChar:
        if (checkError(header, pkt, "DiscoverGAP")) {
            return;
        }
        break;
    case EventType::DevNameRead:
        if (checkError(header, pkt, "DeviceNameRead")) {
            return;
        }
        if (rem) rem->setDeviceName(pkt.data.str);
        break;
    case EventType::DiscDISChar:
        if (checkError(header, pkt, "DiscoverDIS")) {
            return;
        }
        break;
    case EventType::MfgRead:
        if (checkError(header, pkt, "MfgRead")) {
            return;
        }
        if (rem) rem->setMfgName(pkt.data.str);
        break;
    case EventType::ModelRead:
        if (checkError(header, pkt, "ModelRead")) {
            return;
        }
        if (rem) rem->setModelName(pkt.data.str);
        break;
    case EventType::FWRead:
        if (checkError(header, pkt, "FirmwareRead")) {
            return;
        }
        if (rem) rem->setFwVersion(pkt.data.str);
        break;
    case EventType::SWRead:
        if (checkError(header, pkt, "SoftwareRead")) {
            return;
        }
        if (rem) rem->setSwVersion(pkt.data.str);
        break;
    case EventType::DiscMFIChar:
        if (checkError(header, pkt, "DiscMFIChar")) {
            return;
        }
        break;
    case EventType::MfiBatteryRead:
        if (checkError(header, pkt, "MfiBatteryRead")) {
            return;
        }
        if (rem) rem->setCurrBattery(pkt.data.battery_level);
        break;
    case EventType::MFIBatteryNotEnable:
        if (checkError(header, pkt, "MFIBatteryNotEnable")) {
            return;
        }
        break;
    case EventType::L2CAPCon:
        if (checkError(header, pkt, "L2CAPConnected")) {
            return;
        }
        if (rem) rem->setL2CID(pkt.data.cid);
        break;
    case EventType::L2CAPDiscon:
        if (checkError(header, pkt, "L2CAPDisconnected")) {
            return;
        }
        if (rem) rem->setL2CID(0);
        break;
    case EventType::ASPNotEnable:
        if (checkError(header, pkt, "ASPNotificationEnable")) {
            return;
        }
        break;
    case EventType::ACPStart:
        if (checkError(header, pkt, "ACPStart")) {
            return;
        }
        break;
    case EventType::ACPStop:
        if (checkError(header, pkt, "ACPStop")) {
            return;
        }
        break;
    case EventType::ACPStatus:
        if (checkError(header, pkt, "ACPStatus")) {
            return;
        }
        break;
    case EventType::ASPStart:
        if (checkError(header, pkt, "ASPStart")) {
            return;
        }
        if (rem) rem->setAudioStreaming(true);
        break;
    case EventType::ASPStop:
        if (checkError(header, pkt, "ASPStop")) {
            return;
        }
        if (rem) rem->setAudioStreaming(false);
        break;
    case EventType::ASPError:
        if (checkError(header, pkt, "ASPError")) {
            return;
        }
        break;
    case EventType::StreamReady:
        if (checkError(header, pkt, "StreamReady")) {
            return;
        }
        break;
    case EventType::StreamPause:
        if (checkError(header, pkt, "StreamPaused")) {
            return;
        }
        break;
    case EventType::AudioVolume:
        if (checkError(header, pkt, "AudioVolume")) {
            return;
        }
        if (rem) rem->setCurrVolume(pkt.data.volume);
        break;
    case EventType::G722EncTimings:
        m_ui->updateEncodeTimes(pkt.data.encode_timings, 10);
        break;
    }
}

bool PicoAshaComm::checkError(const asha::comm::HeaderPacket header, const asha::comm::EventPacket &pkt, QString const& state)
{
    using namespace asha::comm;
    if (pkt.status_type == StatusType::StatusSuccess) {
        appendLog(QString("%1 - [%2] Success").arg(logHeader(header)).arg(state));
        return false;
    }

    QString err = QStringLiteral("%1 - [%2] Error with status: %3");
    QString err_with_reason = QStringLiteral("%1 - [%2] Error with status: %3 and reason %4");

    QString status = "";
    QString reason = "";

    switch(pkt.status_type) {
    case StatusType::BtstackStatus:
        status = asha::bt_err_str(pkt.status);
        reason = asha::bt_err_str(pkt.reason);
        break;
    case StatusType::ATTStatus:
        status = asha::att_err_str(pkt.status);
        reason = asha::bt_err_str(pkt.reason);
        break;
    case StatusType::SMStatus:
        status = asha::att_err_str(pkt.status);
        reason = asha::sm_reason_str(pkt.reason);
        break;
    case StatusType::L2CapStatus:
        status = asha::l2cap_err_str(pkt.status);
        reason = asha::bt_err_str(pkt.reason);
        if (status == "unknown") {
            status = asha::bt_err_str(pkt.status);
        }
        break;
    case StatusType::PAStatus:
        switch(pkt.status) {
        case PAError::PAMaxConnected:
            status = "PA_MAX_CONNECTED";
            break;
        case PAError::PAASHAServiceNotFound:
            status = "PA_ASHA_SERVICE_NOT_FOUND";
            break;
        case PAError::PAMFIServiceNotFound:
            status = "PA_MFI_SERVICE_NOT_FOUND";
            break;
        case PAError::PARuntimeSettingsErr:
            status = "PA_RUNTIME_SETTINGS_ERR";
            break;
        default:
            status = "";
        }
        break;
    case StatusType::StatusSuccess:
        break;
    }

    QString err_str;


    if (pkt.reason > 0) {
        err_str = err_with_reason.arg(logHeader(header))
                                 .arg(state)
                                 .arg(status)
                                 .arg(reason);
    } else {
        err_str = err.arg(logHeader(header))
                     .arg(state)
                     .arg(status);
    }

    appendLog(err_str);
    setRemoteError(err_str);

    return true;
}

QString PicoAshaComm::logHeader(const asha::comm::HeaderPacket header)
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch((int64_t)header.ts_ms);
    return QString("[%1] [%2]").arg(dt.toString("dd:hh:mm:ss:zzz")).arg(header.conn_id);
}

bool PicoAshaComm::appendLog(const QString &logLine)
{
    m_ui->appendLog(logLine);
    if (m_logOpened) {
        m_logStream << logLine << "\n";
    }
    return true;
}

void PicoAshaComm::closeSerial()
{
    if (m_serial.isOpen()) {
        m_serial.close();
    }
}

bool PicoAshaComm::sendCommandPacket(asha::comm::CmdPacket const& cmd_pkt)
{
    using namespace asha::comm;
    struct {
        HeaderPacket head;
        CmdPacket cmd;
    } pkt;

    pkt.head.type = Type::Cmd;
    pkt.head.conn_id = unset_conn_id;
    pkt.head.len = sizeof(pkt);
    pkt.head.ts_ms = 0;
    pkt.cmd = cmd_pkt;


    static_assert(sizeof(pkt) == sizeof(HeaderPacket) + sizeof(CmdPacket));

    uint8_t enc[1 + COBS_ENCODE_MAX(sizeof(pkt))] = {};
    enc[0] = 0;
    size_t enc_len = 0;
    cobs_encode(&pkt, sizeof(pkt), enc + 1, sizeof(enc) - 1, &enc_len);
    ++enc_len;
    if (m_serialConnected) {
        if (m_serial.write((const char*)enc, enc_len) < 0) {
            setErrMsg(m_serial.errorString());
            return false;
        }
    }
    return true;
}

void PicoAshaComm::writeHciPacket(const char *data, size_t len)
{
    // While logging is on, every btsnoop record (including the first one) is
    // written to the HCI log file.
    if (!m_hciLoggingEnabled || !m_hciLogFile.isOpen()) {
        return;
    }
    if (m_hciLogFile.write(data, static_cast<qint64>(len)) < 0) {
        appendLog(QString("HCI log write error: %1").arg(m_hciLogFile.errorString()));
        stopHciLogging();
        return;
    }
    // One flush per record: the file stays usable even if
    // the application is killed or the Pico is unplugged.
    m_hciLogFile.flush();
}

bool PicoAshaComm::openHciLog(QString const& path)
{
    closeHciLog();

    m_hciLogFile.setFileName(path);
    if (!m_hciLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    struct {
        char id_pattern[8] = "btsnoop";
        uint32_t version = 1;
        uint32_t data_type = 1002; // H4
    } btsnoop_file_header;

    btsnoop_file_header.version = qToBigEndian(btsnoop_file_header.version);
    btsnoop_file_header.data_type = qToBigEndian(btsnoop_file_header.data_type);

    if (m_hciLogFile.write((const char*)&btsnoop_file_header, sizeof(btsnoop_file_header)) < 0) {
        m_hciLogFile.close();
        return false;
    }
    m_hciLogFile.flush();
    return true;
}

void PicoAshaComm::closeHciLog()
{
    if (m_hciLogFile.isOpen()) {
        m_hciLogFile.flush();
        m_hciLogFile.close();
    }
}

bool PicoAshaComm::startHciLogging(QString const& path)
{
    if (openHciLog(path)) {
        m_hciLoggingPath = path;
        m_hciLoggingEnabled = true;
        m_ui->setHciLogPath(path);
        m_ui->setHciActionBtnStop(true);
        m_ui->setHciBrowseEnabled(false);
        appendLog(QString("HCI log: %1").arg(QDir::toNativeSeparators(path)));
        return true;
    }

    appendLog(QString("Cannot open HCI log %1: %2")
                  .arg(QDir::toNativeSeparators(path), m_hciLogFile.errorString()));
    m_hciLoggingEnabled = false;
    setErrMsg(m_hciLogFile.errorString());
    m_ui->setHciActionBtnStart(m_serialConnected);
    return false;
}

void PicoAshaComm::stopHciLogging()
{
    m_hciLoggingEnabled = false;
    closeHciLog();
    m_ui->setHciActionBtnStart(m_serialConnected);
    m_ui->setHciBrowseEnabled(true);
    appendLog("HCI log stopped");
}

void PicoAshaComm::maybeScheduleAudioResync()
{
    if (m_audioResyncWaiting && m_ui->connectedRemoteCount() >= 2) {
        // Both hearing aids are back: give the second one time to finish its
        // start-up, then restart the stream so both play together.
        m_audioResyncWaiting = false;
        QTimer::singleShot(5000, this, [this]() { sendAudioRestartCommand(); });
    }
}

void PicoAshaComm::sendAudioRestartCommand()
{
    using namespace asha::comm;
    if (!m_serialConnected) {
        return;
    }
    if (sendCommandPacket(
            {
                .cmd = Command::AudioRestart,
                .cmd_status = CmdStatus::CmdOk,
                .data = {}
            })) {
        appendLog("Audio restarted after the USB settings update");
    }
}

void PicoAshaComm::sendHciDumpCommand(bool enable)
{
    using namespace asha::comm;
    sendCommandPacket(
        {
            .cmd = Command::HCIDump,
            .cmd_status = CmdStatus::CmdOk,
            .data = {.enable_hci = enable}
        }
    );
}

void PicoAshaComm::showUI() const
{
#ifdef Q_OS_ANDROID
    m_ui->showMaximized();
#else
    m_ui->show();
#endif
}

QString PicoAshaComm::paFirmwareVers() const
{
    return m_paFirmwareVers;
}

void PicoAshaComm::setPaFirmwareVers(const QString &newPaFirmwareVers)
{
    if (m_paFirmwareVers == newPaFirmwareVers)
        return;
    m_paFirmwareVers = newPaFirmwareVers;
    emit paFirmwareVersChanged();
}

QString PicoAshaComm::remoteError() const
{
    return m_remoteError;
}

void PicoAshaComm::setRemoteError(const QString &newRemoteError)
{
    if (m_remoteError == newRemoteError)
        return;
    m_remoteError = newRemoteError;
    emit remoteErrorChanged();
}

QString PicoAshaComm::errMsg() const
{
    return m_errMsg;
}

void PicoAshaComm::setErrMsg(const QString &newErrMsg)
{
    if (m_errMsg == newErrMsg)
        return;
    m_errMsg = newErrMsg;
    emit errMsgChanged();
}
