#include <algorithm>
#include <cmath>
#include <numbers>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QActionGroup>
#include <QFrame>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProxyStyle>
#include <QToolButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVariant>

#include "picoashamainwindow.h"
#include "i18n.h"
#include "remotedevice.h"

static QString widgetNameFromConnID(uint16_t connID)
{
    return QString("remote_connid_%1").arg(connID);
}

namespace
{
// Agrandit les icônes (drapeaux) du menu des langues
class FlagMenuStyle : public QProxyStyle
{
public:
    int pixelMetric(PixelMetric metric, QStyleOption const* option = nullptr,
                    QWidget const* widget = nullptr) const override
    {
        if (metric == QStyle::PM_SmallIconSize) {
            return 24;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};
} // namespace

PicoAshaMainWindow::PicoAshaMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_currPairDlg = nullptr;
    m_mainWidget = new QWidget;

    // Bouton "Language" avec le drapeau de la langue affichée. Son menu propose la langue
    // de l'ordinateur (automatique) ou une langue choisie, chaque langue précédée de son drapeau.
    auto langBtn = new QToolButton;
    {
        auto langMenu = new QMenu(langBtn);
        auto menuStyle = new FlagMenuStyle;
        menuStyle->setParent(langMenu);
        langMenu->setStyle(menuStyle);

        auto langGroup = new QActionGroup(this);
        langGroup->setExclusive(true);
        QString const saved = i18n::savedLanguage();

        auto autoAct = langMenu->addAction(
            i18n::autoIcon(),
            tr("Use computer language (automatic): %1").arg(i18n::nativeName(i18n::systemLanguage())));
        autoAct->setCheckable(true);
        autoAct->setData(QString());
        autoAct->setChecked(saved.isEmpty());
        langGroup->addAction(autoAct);
        langMenu->addSeparator();

        for (auto const& code : i18n::availableLanguages()) {
            auto act = langMenu->addAction(i18n::flagIcon(code), i18n::nativeName(code));
            act->setCheckable(true);
            act->setData(code);
            act->setChecked(code == saved);
            langGroup->addAction(act);
        }
        // Un choix qui n'existe plus : retour sur "automatique"
        if (!saved.isEmpty() && !langGroup->checkedAction()) {
            autoAct->setChecked(true);
        }

        QObject::connect(langGroup, &QActionGroup::triggered, this, [this](QAction* act) {
            QString const code = act->data().toString();   // "" = automatique
            if (code == i18n::savedLanguage()) {
                return;
            }
            i18n::setSavedLanguage(code);
            auto ans = QMessageBox::question(
                this, this->windowTitle(),
                tr("The language will change when the application restarts. Restart now?"));
            if (ans == QMessageBox::Yes) {
                QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                        QCoreApplication::arguments().mid(1));
                QCoreApplication::quit();
            }
        });

        langBtn->setText(tr("Language"));
        langBtn->setIcon(i18n::flagIcon(i18n::effectiveLanguage()));
        langBtn->setIconSize(QSize(24, 16));
        langBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        langBtn->setPopupMode(QToolButton::InstantPopup);
        langBtn->setMenu(langMenu);
    }
    auto mainVBox = new QVBoxLayout;

    auto langRow = new QHBoxLayout;
    langRow->addStretch();
    langRow->addWidget(langBtn);
    mainVBox->addLayout(langRow);

    m_remoteFrame = new QFrame;
    auto remoteHBox = new QHBoxLayout;
    remoteHBox->addStretch(1);

    RemoteDevice* rem1 = new RemoteDevice;
    RemoteDevice* rem2 = new RemoteDevice;

    m_remotes.append(rem1);
    m_remotes.append(rem2);

    for (auto r : std::as_const(m_remotes)) {
        remoteHBox->addWidget(r);
    }

    remoteHBox->addStretch(1);
    m_remoteFrame->setLayout(remoteHBox);
    mainVBox->addWidget(m_remoteFrame);

    auto cmdGroup = new QGroupBox(tr("Send Commands"));
    auto cmdLayout = new QHBoxLayout;
    cmdLayout->addStretch();

    m_cmdStreamingEnabledBtn = new QPushButton;
    m_cmdStreamingEnabledBtn->setToolTip(tr("Stop or start ASHA streaming.\n"
                                         "This can be useful to restart audio streaming if you "
                                         "encounter stereo sync issues."));
    cmdLayout->addWidget(m_cmdStreamingEnabledBtn);

    m_cmdConnAllowedBtn = new QPushButton;
    m_cmdConnAllowedBtn->setToolTip(tr("Allow or deny hearing aids from connecting to Pico-ASHA.\n"
                                    "This can be useful if you want to stream from another "
                                    "device such as a mobile phone."));
    cmdLayout->addWidget(m_cmdConnAllowedBtn);

    m_cmdRestartBtn = new QPushButton(tr("Restart"));
    m_cmdRestartBtn->setToolTip(tr("Restarts Pico-ASHA.\n"
                                "It can be useful if your previously connected "
                                "hearing aids stop connecting."));
    cmdLayout->addWidget(m_cmdRestartBtn);

    m_cmdRemoveBondBtn = new QPushButton(tr("Unpair"));
    m_cmdRemoveBondBtn->setToolTip(tr("Unpair connected hearing aids.\n"
                                   "As Pico-ASHA only supports one set of hearing aids at a time,\n"
                                   "this allows pairing a different set of hearing aids."));
    cmdLayout->addWidget(m_cmdRemoveBondBtn);

    cmdLayout->addStretch();
    cmdGroup->setLayout(cmdLayout);
    mainVBox->addWidget(cmdGroup);

    auto usbGroup = new QGroupBox(tr("USB Settings"));
    auto usbVBox = new QVBoxLayout;
    auto usbLayout = new QHBoxLayout;       // ligne 1 : réglages USB
    auto usbAudioLayout = new QHBoxLayout;  // ligne 2 : Audio Setting Experimental
    usbLayout->addStretch();

    auto usbUacLabel = new QLabel(tr("UAC Version"));
    usbLayout->addWidget(usbUacLabel);

    m_USBUacVersCombo = new QComboBox();
    m_USBUacVersCombo->addItem("UAC1", QVariant(1u));
    m_USBUacVersCombo->addItem("UAC2", QVariant(2u));
    m_USBUacVersCombo->setToolTip(tr("Change USB Audio Class version.\n"
                               "Most people should stick with UAC2, however UAC1 may be compatible "
                               "with older operating systems such as Windows XP - 8.\n"
                               "This setting will persist, allowing you to set it on one device, "
                               "then plug it into an older device that only supports UAC1."));
    usbLayout->addWidget(m_USBUacVersCombo);

    auto usbVolMinLabel = new QLabel(tr("Min Volume"));
    usbVolMinLabel->setToolTip(tr("Level at the bottom of the Windows slider (just above mute), in ASHA steps (0.375 dB).\nBelow -127 (-47.6 dB), the hearing aids' floor, the level is\nobtained by digitally attenuating the audio."));
    usbLayout->addWidget(usbVolMinLabel);
    m_USBVolMinSpin = new QSpinBox();
    m_USBVolMinSpin->setRange(-240, 0);
    usbLayout->addWidget(m_USBVolMinSpin);

    auto usbVolMaxLabel = new QLabel(tr("Max Volume"));
    usbVolMaxLabel->setToolTip(tr("Level at 100% on the Windows slider, in ASHA steps (0.375 dB)."));
    usbLayout->addWidget(usbVolMaxLabel);
    m_USBVolMaxSpin = new QSpinBox();
    m_USBVolMaxSpin->setRange(-240, 0);
    usbLayout->addWidget(m_USBVolMaxSpin);

    auto usbDigThrLabel = new QLabel(tr("Digital Attenuation Below"));
    usbDigThrLabel->setToolTip(tr("Level (ASHA steps, 0.375 dB) under which the hearing aids' volume stays fixed\n"
                               "and the audio signal is attenuated digitally instead, like the volume\n"
                               "slider of a web browser (gives very quiet levels).\n"
                               "-127 = only below the hearing aids' own floor.\n"
                               "Equal to Max Volume = fully digital volume (hearing aids' volume never moves)."));
    usbLayout->addWidget(usbDigThrLabel);
    m_USBDigThrSpin = new QSpinBox();
    m_USBDigThrSpin->setRange(-240, -1);
    m_USBDigThrSpin->setValue(-127);
    usbLayout->addWidget(m_USBDigThrSpin);

    m_USBSettingsBtn = new QPushButton(tr("Update"));
    usbLayout->addWidget(m_USBSettingsBtn);

    usbLayout->addStretch();

    // Ligne 2 : "Audio Setting Experimental"
    usbAudioLayout->addStretch();

    auto usbAudioLabel = new QLabel(tr("AUDIO SETTING EXPERIMENTAL :"));
    usbAudioLabel->setToolTip(tr("Experimental: computes Min Volume, Digital Attenuation Below and Max Volume\n"
                              "from the characteristics of the hearing aids.\n"
                              "A value of 0 leaves the corresponding settings under manual control."));
    usbAudioLayout->addWidget(usbAudioLabel);

    auto usbOsplLabel = new QLabel(tr("Maximum Output Level (dB SPL)"));
    usbOsplLabel->setToolTip(tr("If above 0: Min Volume = -240 and\n"
                             "Digital Attenuation Below = -240 + this value."));
    usbAudioLayout->addWidget(usbOsplLabel);
    m_USBOsplSpin = new QSpinBox();
    m_USBOsplSpin->setRange(0, 239);
    m_USBOsplSpin->setValue(0);
    usbAudioLayout->addWidget(m_USBOsplSpin);

    auto usbRefGainLabel = new QLabel(tr("Reference Test Gain (dB)"));
    usbRefGainLabel->setToolTip(tr("If above 0: Max Volume = Digital Attenuation Below + this value."));
    usbAudioLayout->addWidget(usbRefGainLabel);
    m_USBRefGainSpin = new QSpinBox();
    m_USBRefGainSpin->setRange(0, 240);
    m_USBRefGainSpin->setValue(0);
    usbAudioLayout->addWidget(m_USBRefGainSpin);

    usbAudioLayout->addStretch();

    usbVBox->addLayout(usbLayout);
    usbVBox->addLayout(usbAudioLayout);
    usbGroup->setLayout(usbVBox);
    mainVBox->addWidget(usbGroup);


    QObject::connect(m_cmdRestartBtn, &QPushButton::clicked, this, &PicoAshaMainWindow::cmdRestartBtnClicked);
    QObject::connect(m_cmdConnAllowedBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        emit cmdConnAllowedBtnClicked(!m_connectionsAllowed);
    });
    QObject::connect(m_cmdStreamingEnabledBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        emit cmdStreamingEnabledBtnClicked(!m_streamingEnabled);
    });
    QObject::connect(m_cmdRemoveBondBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        auto ans = QMessageBox::question(this, this->windowTitle(), tr("Are you sure you want to unpair connected hearing aids?"));
        if (ans == QMessageBox::Yes) {
            emit cmdRemoveBondBtnClicked();
        }
    });
    QObject::connect(m_USBUacVersCombo, &QComboBox::activated, this, [=, this](int index) {
        setUSBSettingsBtnState();
    });
    QObject::connect(m_USBVolMinSpin, &QSpinBox::valueChanged, this, [=, this](int value) {
        setUSBSettingsBtnState();
    });
    QObject::connect(m_USBVolMaxSpin, &QSpinBox::valueChanged, this, [=, this](int value) {
        setUSBSettingsBtnState();
    });
    QObject::connect(m_USBDigThrSpin, &QSpinBox::valueChanged, this, [=, this](int value) {
        updateUSBOverrides();
    });
    QObject::connect(m_USBOsplSpin, &QSpinBox::valueChanged, this, [=, this](int value) {
        updateUSBOverrides();
    });
    QObject::connect(m_USBRefGainSpin, &QSpinBox::valueChanged, this, [=, this](int value) {
        updateUSBOverrides();
    });
    QObject::connect(m_USBSettingsBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        emit usbSettingsBtnClicked(fromUsbWidgets());
    });

    setConnectionsAllowed(false);
    setAudioStreamingEnabled(false);
    setCmdBtnsEnabled(false);
    setUSBWidgetsEnabled(false);

    auto hciGroup = new QGroupBox(tr("HCI Logging"));
    auto hciLayout = new QHBoxLayout;
    m_hciActionBtn = new QPushButton;
    m_hciActionBtn->setToolTip(tr("Start or stop saving the HCI packets to the log file.\n"
                               "If no file has been chosen yet, you will be asked where to save it.\n"
                               "Useful to help diagnose pairing and connection issues."));
    setHciActionBtnStart(false);
    m_hciBrowseBtn = new QPushButton(tr("Browse..."));
    m_hciBrowseBtn->setToolTip(tr("Choose where the HCI log file is saved."));
    m_hciPathLbl = new QLabel(tr("HCI log: (not set)"));
    m_hciPathLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hciLayout->addWidget(m_hciActionBtn, 0);
    hciLayout->addWidget(m_hciBrowseBtn, 0);
    hciLayout->addWidget(m_hciPathLbl, 1);
    hciGroup->setLayout(hciLayout);
    mainVBox->addWidget(hciGroup);

    QObject::connect(m_hciActionBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        emit hciLogActionBtnClicked();
    });
    QObject::connect(m_hciBrowseBtn, &QPushButton::clicked, this, [=, this](bool clicked) {
        askHciLogPath();
    });

    m_logWidget = new QPlainTextEdit;
    m_logWidget->setLayoutDirection(Qt::LeftToRight);   // journal technique, en anglais
    m_logWidget->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logWidget->setReadOnly(true);
    m_logWidget->setCenterOnScroll(true);
    mainVBox->addWidget(m_logWidget);

    m_mainWidget->setLayout(mainVBox);
    setCentralWidget(m_mainWidget);

    m_serialConnectedStatus = new QLabel();
    onSerialConnected(false);

    m_encodeTimesLabel = new QLabel();
    m_encodeTimes.reserve(1000);

    QStatusBar* mainStatusBar = new QStatusBar;
    mainStatusBar->addPermanentWidget(m_encodeTimesLabel);
    mainStatusBar->addPermanentWidget(m_serialConnectedStatus);
    setStatusBar(mainStatusBar);
}

PicoAshaMainWindow::~PicoAshaMainWindow()
{
}

QFrame *PicoAshaMainWindow::remoteFrame() const
{
    return m_remoteFrame;
}

void PicoAshaMainWindow::appendLog(const QString &logLine)
{
    m_logWidget->appendPlainText(logLine);
}

RemoteDevice* PicoAshaMainWindow::addRemote(uint16_t connID, const asha::comm::RemoteInfo* remote)
{
    using namespace asha::comm;
    QString remName = widgetNameFromConnID(connID);
    if (getRemote(connID) != nullptr) {
        qDebug("Remote device already exists");
        return nullptr;
    }
    auto rem0 = m_remotes.at(0);
    auto rem1 = m_remotes.at(1);
    RemoteDevice* r = nullptr;

    auto side = RemoteDevice::Side::SideUnset;
    if (remote != nullptr) {
        if (remote->side == CSide::Left) {
            side = RemoteDevice::Side::Left;
        } else if (remote->side == CSide::Right) {
            side = RemoteDevice::Side::Right;
        }
    }

    if (side == RemoteDevice::Side::Left) {
        r = rem0;
    } else if (side == RemoteDevice::Side::Right) {
        r = rem1;
    } else if (rem0->isDefaultValues() && rem1->isDefaultValues()) {
        r = rem0;
    } else {
        r = rem0->isDefaultValues() ? rem0 : rem1;
    }
    r->setObjectName(remName);
    r->setGreyedOut(false);
    if (remote != nullptr) {
        r->setRemoteInfo(*remote);
    } else {
        r->setConnID(connID);
    }
    return r;
}

RemoteDevice* PicoAshaMainWindow::addRemote(const asha::comm::RemoteInfo &remote)
{
    return addRemote(remote.conn_id, &remote);
}

void PicoAshaMainWindow::removeRemote(uint16_t connID)
{
    auto rem = getRemote(connID);
    if (rem) {
        rem->setObjectName("");
        rem->setDefaultValues();
        rem->setGreyedOut(true);
    }
}

void PicoAshaMainWindow::removeRemotes()
{
    for (auto rem : std::as_const(m_remotes)) {
        rem->setObjectName("");
        rem->setDefaultValues();
        rem->setGreyedOut(true);
    }
}

void PicoAshaMainWindow::setRemoteOrder()
{
    QHBoxLayout* l = qobject_cast<QHBoxLayout*>(m_remoteFrame->layout());
    auto rem0 = m_remotes.at(0);
    auto rem1 = m_remotes.at(1);
    auto p0 = rem0->cachedProps();
    auto p1 = rem1->cachedProps();

    if (p0.side == RemoteDevice::Side::Right) {
        auto li = l->takeAt(1);
        l->insertItem(2, li);
        m_remotes.swapItemsAt(0,1);
    } else if (p1.side == RemoteDevice::Side::Left) {
        auto li = l->takeAt(2);
        l->insertItem(1, li);
        m_remotes.swapItemsAt(1,0);
    }
}

RemoteDevice *PicoAshaMainWindow::getRemote(uint16_t connID)
{
    auto name = widgetNameFromConnID(connID);
    for (auto r : std::as_const(m_remotes)) {
        if (r->objectName() == name) {
            return r;
        }
    }
    return nullptr;
}

void PicoAshaMainWindow::setConnectionsAllowed(bool allowed)
{
    m_connectionsAllowed = allowed;
    if (m_connectionsAllowed) {
        m_cmdConnAllowedBtn->setText(tr("Disable Connections"));
    } else {
        m_cmdConnAllowedBtn->setText(tr("Enable Connections"));
    }
}

void PicoAshaMainWindow::setAudioStreamingEnabled(bool enabled)
{
    m_streamingEnabled = enabled;
    if (m_streamingEnabled) {
        m_cmdStreamingEnabledBtn->setText(tr("Stop Audio"));
    } else {
        m_cmdStreamingEnabledBtn->setText(tr("Start Audio"));
    }
}

void PicoAshaMainWindow::setUSBInfo(const asha::comm::USBInfo &usb_info)
{
    m_usbInfo = usb_info;
    m_usbInfo.reserved = 0U; // unused
    // Normalise (older firmware sends the plain UAC version)
    m_usbInfo.uac_vers = asha::comm::USBInfo::pack_uac(m_usbInfo.uac(), m_usbInfo.dig_thr_steps());
    int uac_ver = static_cast<int>(m_usbInfo.uac());
    int vol_min = static_cast<int>(m_usbInfo.min_vol) / 96;
    int vol_max = static_cast<int>(m_usbInfo.max_vol) / 96;
    m_USBUacVersCombo->setCurrentIndex(uac_ver - 1);
    m_USBVolMinSpin->setValue(vol_min);
    m_USBVolMaxSpin->setValue(vol_max);
    m_USBDigThrSpin->setValue(m_usbInfo.dig_thr_steps());
    // Les valeurs de la Pico font foi : les deux réglages expérimentaux,
    // qui ne sont pas stockés dans la Pico, repartent à 0.
    m_USBOsplSpin->blockSignals(true);
    m_USBRefGainSpin->blockSignals(true);
    m_USBOsplSpin->setValue(0);
    m_USBRefGainSpin->setValue(0);
    m_USBOsplSpin->blockSignals(false);
    m_USBRefGainSpin->blockSignals(false);
    updateUSBOverrides();
}

void PicoAshaMainWindow::setUSBSettingsBtnState()
{
    auto info = fromUsbWidgets();
    if (m_usbInfo != info && info.min_vol < info.max_vol) {
        m_USBSettingsBtn->setEnabled(true);
    } else {
        m_USBSettingsBtn->setEnabled(false);
    }
}

void PicoAshaMainWindow::updateUSBOverrides()
{
    int const ospl = m_USBOsplSpin->value();
    int const gain = m_USBRefGainSpin->value();
    bool const osplActive = ospl > 0;
    bool const gainActive = gain > 0;

    // Maximum Output Level > 0 : Min Volume = -240 et
    // Digital Attenuation Below = -240 + Maximum Output Level
    if (osplActive) {
        m_USBVolMinSpin->setValue(-240);
        m_USBDigThrSpin->setValue(std::clamp(-240 + ospl, -240, -1));
    }
    // Reference Test Gain > 0 : Max Volume = Digital Attenuation Below + Reference Test Gain
    if (gainActive) {
        m_USBVolMaxSpin->setValue(std::clamp(m_USBDigThrSpin->value() + gain, -240, 0));
    }

    // Les réglages calculés ne sont plus modifiables à la main
    m_USBVolMinSpin->setEnabled(m_usbWidgetsEnabled && !osplActive);
    m_USBDigThrSpin->setEnabled(m_usbWidgetsEnabled && !osplActive);
    m_USBVolMaxSpin->setEnabled(m_usbWidgetsEnabled && !gainActive);

    setUSBSettingsBtnState();
}

QString PicoAshaMainWindow::hciLogPath() const
{
    return m_hciLogPath;
}

QString PicoAshaMainWindow::askHciLogPath()
{
    QString start = m_hciLogPath;
    if (start.isEmpty()) {
        QString const docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        start = QDir(docs.isEmpty() ? QDir::homePath() : docs).filePath("HCI.log");
    }
    QString const path = QFileDialog::getSaveFileName(this, tr("Save HCI log as"), start,
                                                      tr("HCI log (*.log *.btsnoop);;All files (*)"));
    if (!path.isEmpty()) {
        m_hciLogPath = path;
        setHciLogPath(path);
    }
    return path;
}

void PicoAshaMainWindow::setHciBrowseEnabled(bool enabled)
{
    m_hciBrowseBtn->setEnabled(enabled);
}

int PicoAshaMainWindow::connectedRemoteCount() const
{
    int count = 0;
    for (auto r : m_remotes) {
        if (!r->objectName().isEmpty()) {
            ++count;
        }
    }
    return count;
}

void PicoAshaMainWindow::setHciActionBtnStart(bool enabled)
{
    m_hciActionBtn->setText(tr("HCI Start"));
    m_hciActionBtn->setEnabled(enabled);
}

void PicoAshaMainWindow::setHciActionBtnStop(bool enabled)
{
    m_hciActionBtn->setText(tr("HCI Stop"));
    m_hciActionBtn->setEnabled(enabled);
}

void PicoAshaMainWindow::setHciLogPath(QString const& path)
{
    QString const nativePath = QDir::toNativeSeparators(path);
    m_hciPathLbl->setText(tr("HCI log: %1").arg(nativePath));
    m_hciPathLbl->setToolTip(nativePath);
}

void PicoAshaMainWindow::onAdPacketReceived(const asha::comm::AdvertisingPacket &ad_pkt)
{
    if (!m_currPairDlg) {
        m_currPairDlg = new PairDialog(this);
        QObject::connect(m_currPairDlg, &PairDialog::addressSelected, this, &PicoAshaMainWindow::pairWithAddress);
        QObject::connect(m_currPairDlg, &PairDialog::accepted, this, &PicoAshaMainWindow::onPairDialogAcceptedRejected);
        m_currPairDlg->open();
    }
    m_currPairDlg->setAdPacket(ad_pkt);
}

void PicoAshaMainWindow::updateEncodeTimes(const int16_t *times, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        m_encodeTimes.append(times[i]);
    }
    if (m_encodeTimes.size() >= 1000) {
        auto min = *(std::min_element(m_encodeTimes.begin(), m_encodeTimes.end()));
        auto max = *(std::max_element(m_encodeTimes.begin(), m_encodeTimes.end()));
        auto avg = std::accumulate(m_encodeTimes.begin(), m_encodeTimes.end(), 0.0) / m_encodeTimes.size();

        m_encodeTimesLabel->setText(tr("MIN: %1    AVG: %2    MAX: %3").arg(min).arg(avg).arg(max));
        m_encodeTimes.clear();
    }
}

void PicoAshaMainWindow::onPairDialogAcceptedRejected()
{
    m_currPairDlg->deleteLater();
    m_currPairDlg = nullptr;
}

asha::comm::USBInfo PicoAshaMainWindow::fromUsbWidgets()
{
    auto uac_ver = static_cast<uint16_t>(m_USBUacVersCombo->currentData().toUInt());
    auto min_vol = static_cast<int16_t>(m_USBVolMinSpin->value() * 96);
    auto max_vol = static_cast<int16_t>(m_USBVolMaxSpin->value() * 96);
    auto packed = asha::comm::USBInfo::pack_uac(uac_ver, m_USBDigThrSpin->value());
    return {.uac_vers = packed, .min_vol = min_vol, .max_vol = max_vol, .reserved = 0U};
}

void PicoAshaMainWindow::onSerialConnected(bool connected)
{
    m_serialConnected = connected;
    if (m_serialConnected) {
        m_serialConnectedStatus->setText(tr("Connected"));
        m_serialConnectedStatus->setStyleSheet("QLabel { color : green; }");
    } else {
        m_serialConnectedStatus->setText(tr("Not Connected"));
        m_serialConnectedStatus->setStyleSheet("QLabel { color : red; }");
    }
}

void PicoAshaMainWindow::setCmdBtnsEnabled(bool enabled)
{
    m_cmdRestartBtn->setEnabled(enabled);
    m_cmdConnAllowedBtn->setEnabled(enabled);
    m_cmdStreamingEnabledBtn->setEnabled(enabled);
    m_cmdRemoveBondBtn->setEnabled(enabled);
}

void PicoAshaMainWindow::setUSBWidgetsEnabled(bool enabled)
{
    m_usbWidgetsEnabled = enabled;
    m_USBUacVersCombo->setEnabled(enabled);
    m_USBOsplSpin->setEnabled(enabled);
    m_USBRefGainSpin->setEnabled(enabled);
    updateUSBOverrides();   // gère aussi l'état des champs Min / Max / Digital Attenuation
    if (!enabled) {
        m_USBSettingsBtn->setEnabled(enabled);
    }
}

void PicoAshaMainWindow::setPicoAshaVerStr(const QString &version)
{
    m_serialConnectedStatus->setText(tr("Connected :: %1").arg(version));
}

