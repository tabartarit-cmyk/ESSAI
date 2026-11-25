#include <QBitArray>
#include <QByteArray>
#include "remotedevice.h"

RemoteDevice::RemoteDevice(QWidget *parent)
    : QGroupBox(parent)
{
    setupUI();
    setDefaultValues();
    setGreyedOut(true);
}

void RemoteDevice::setRemoteInfo(const asha::comm::RemoteInfo &remote)
{
    setConnID(remote.conn_id);
    setHCIHandle(remote.hci_handle);
    setAddr(remote.addr);
    setPairedBonded(remote.paired);
    setPsm(remote.psm);
    setL2CID(remote.l2cap_id);
    setDeviceName(remote.dev_name);
    setMfgName(remote.mfg_name);
    setModelName(remote.model_name);
    setFwVersion(remote.fw_vers);
    setSwVersion(remote.sw_vers);
    setSide(remote.side);
    setMode(remote.mode);
    setAudioStreaming(remote.audio_streaming);
    setCurrVolume(remote.curr_vol);
    setCurrBattery(remote.curr_battery);
}

void RemoteDevice::setROPInfo(const char* rop)
{
    QBitArray device_cap = QBitArray::fromBits(rop + 1, 8);
    QBitArray codecs = QBitArray::fromBits(rop + 15, 16);

    setSide(device_cap.testBit(0) ? Side::Right : Side::Left);
    setMode(device_cap.testBit(1) ? Mode::Binaural : Mode::Mono);
    setG24KHz(codecs.testBit(2));
}

RemoteDevice::CachedProps RemoteDevice::cachedProps()
{
    return m_cachedProps;
}

void RemoteDevice::setCachedProps(CachedProps const& props)
{
    setDeviceName(props.deviceName);
    setModelName(props.modelName);
    setMfgName(props.mfgName);
    setFwVersion(props.fwVersion);
    setSwVersion(props.swVersion);
    setG24KHz(props.g24kHZ);
    setMode(props.mode);
    setSide(props.side);
}

void RemoteDevice::setConnID(uint16_t connID)
{
    m_connIDLabel.setText(QString::number(connID));
}

void RemoteDevice::setHCIHandle(uint16_t hciHandle)
{
    m_hciConnHandleLabel.setText(QString("0x%1").arg((int)hciHandle, 2, 16, QChar('0')));
}

void RemoteDevice::setAddr(const uint8_t *addr)
{
    QByteArray addrBytes(reinterpret_cast<const char*>(addr), 6);
    m_addrLabel.setText(addrBytes.toHex(':'));
    m_cachedProps.addr = addrBytes;
}

void RemoteDevice::setPairedBonded(bool paired)
{
    m_pairedAndBondedLabel.setText(paired ? tr("Yes") : tr("No"));
}

void RemoteDevice::setPsm(uint16_t psm)
{
    m_psmLabel.setText(QString("0x%1").arg((int)psm, 2, 16, QChar('0')));
}

void RemoteDevice::setL2CID(uint16_t cid)
{
    m_l2CIDLabel.setText(QString("0x%1").arg((int)cid, 2, 16, QChar('0')));
}

void RemoteDevice::setDeviceName(QString const& deviceName)
{
    m_deviceNameLabel.setText(deviceName);
    m_cachedProps.deviceName = deviceName;
}

void RemoteDevice::setMfgName(QString const& mfgName)
{
    m_mfgNameLabel.setText(mfgName);
    m_cachedProps.mfgName = mfgName;
}

void RemoteDevice::setModelName(QString const& modelName)
{
    m_modelNameLabel.setText(modelName);
    m_cachedProps.modelName = modelName;
}

void RemoteDevice::setFwVersion(QString const& fwVersion)
{
    m_fwVersionLabel.setText(fwVersion);
    m_cachedProps.fwVersion = fwVersion;
}

void RemoteDevice::setSwVersion(QString const& swVersion)
{
    m_swVersionLabel.setText(swVersion);
    m_cachedProps.swVersion = swVersion;
}

void RemoteDevice::setSide(Side side)
{
    QString sideStr = side == Side::Left ? tr("Left")
                      : side == Side::Right ? tr("Right") : tr("Side Unknown");
    setTitle(sideStr);
    m_cachedProps.side = side;
}

void RemoteDevice::setSide(asha::comm::CSide side)
{
    using namespace asha::comm;
    Side s = side == CSide::Left ? Side::Left
                : side == CSide::Right ? Side::Right : Side::SideUnset;
    setSide(s);
}

void RemoteDevice::setMode(Mode mode)
{
    QString modeStr = mode == Mode::Mono ? tr("Mono")
                      : mode == Mode::Binaural ? tr("Binaural") : tr("Unknown");
    m_modeLabel.setText(modeStr);
    m_cachedProps.mode = mode;
}

void RemoteDevice::setMode(asha::comm::CMode mode)
{
    using namespace asha::comm;
    Mode m = mode == CMode::Mono ? Mode::Mono
                : mode == CMode::Binaural ? Mode::Binaural : Mode::ModeUnset;
    setMode(m);
}

void RemoteDevice::setAudioStreaming(bool audioStreaming)
{
    m_audioStreamingLabel.setText(audioStreaming ? tr("Yes") : tr("No"));
}

void RemoteDevice::setCurrVolume(int8_t currVol)
{
    m_currVolumeLabel.setText(QString::number(currVol));
}

void RemoteDevice::setG24KHz(bool enabled)
{
    m_g72224Label.setText(enabled ? tr("Supported") : tr("Unsupported"));
    m_cachedProps.g24kHZ = enabled;
}

void RemoteDevice::setCurrBattery(uint8_t currBattery)
{
    //m_currBatteryLabel.setText(QString::number(currBattery));
    m_currBatteryBar.setValue(currBattery);
}

void RemoteDevice::setDefaultValues()
{
    setTitle(tr("Side Unknown"));

    m_connIDLabel.setText("0");
    m_hciConnHandleLabel.setText("0x00");
    m_addrLabel.setText("00:00:00:00:00:00");
    m_pairedAndBondedLabel.setText(tr("No"));
    m_deviceNameLabel.setText("");
    m_mfgNameLabel.setText("");
    m_modelNameLabel.setText("");
    m_fwVersionLabel.setText("");
    m_swVersionLabel.setText("");
    m_modeLabel.setText(tr("Unknown"));
    m_g72224Label.setText(tr("Unsupported"));
    m_psmLabel.setText("0x00");
    m_l2CIDLabel.setText("0x00");
    m_audioStreamingLabel.setText(tr("No"));
    m_currVolumeLabel.setText("-128");
    m_currBatteryBar.setValue(0);
}

bool RemoteDevice::isDefaultValues()
{
    return m_connIDLabel.text() == "0";
}

void RemoteDevice::setGreyedOut(bool enabled)
{
    greyedOut = enabled;
    if (greyedOut) {
        setStyleSheet("color: grey");
    } else {
        setStyleSheet("");
    }
}

void RemoteDevice::setupUI()
{
    auto addRowToLayout = [this] (QString const& header, QLabel* val) {
        val->setStyleSheet("font-weight: normal");
        this->m_formLayout.addRow(header, val);
    };

    setStyleSheet("font-weight: bold");
    setAlignment(Qt::AlignHCenter);

    addRowToLayout(tr("Connection ID"), &m_connIDLabel);
    addRowToLayout(tr("HCI Conn Handle"), &m_hciConnHandleLabel);
    addRowToLayout(tr("Address"), &m_addrLabel);
    addRowToLayout(tr("Paired and Bonded"), &m_pairedAndBondedLabel);
    addRowToLayout(tr("Device Name"), &m_deviceNameLabel);
    addRowToLayout(tr("Make"), &m_mfgNameLabel);
    addRowToLayout(tr("Model"), &m_modelNameLabel);
    addRowToLayout(tr("FW Version"), &m_fwVersionLabel);
    addRowToLayout(tr("SW Version"), &m_swVersionLabel);
    addRowToLayout(tr("Mode"), &m_modeLabel);
    addRowToLayout(tr("24 kHz Support"), &m_g72224Label);
    addRowToLayout(tr("PSM"), &m_psmLabel);
    addRowToLayout(tr("L2CAP CID"), &m_l2CIDLabel);
    addRowToLayout(tr("Audio Streaming"), &m_audioStreamingLabel);
    addRowToLayout(tr("Current Volume"), &m_currVolumeLabel);

    m_currBatteryBar.setRange(0, 10);
    m_currBatteryBar.setFormat("%v/%m");
    this->m_formLayout.addRow(tr("Current Battery"), &m_currBatteryBar);

    setLayout(&m_formLayout);
}
