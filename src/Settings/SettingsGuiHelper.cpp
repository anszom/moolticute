#include "SettingsGuiHelper.h"
#include "ISettingsGui.h"
#include "SettingsGuiMini.h"
#include "SettingsGuiBLE.h"
#include "MainWindow.h"
#include "ui_MainWindow.h"


SettingsGuiHelper::SettingsGuiHelper(WSClient* parent)
    : QObject(parent),
      m_wsClient{parent}
{
    m_fw13LangMap = {{"ca_FR", ID_KEYB_CA_FR_LUT},
                     {"ca_MAC", ID_KEYB_CA_MAC_LUT},
                     {"dk_MAC", ID_KEYB_DK_MAC_LUT},
                     {"uk_MAC", ID_KEYB_UK_MAC_LUT},
                     {"por", ID_KEYB_POR_LUT}};
}

void SettingsGuiHelper::setMainWindow(MainWindow *mw)
{
    m_mw = mw;
    ui = mw->ui;
    m_widgetMapping = {
        {MPParams::KEYBOARD_LAYOUT_PARAM, ui->comboBoxLang},
        {MPParams::LOCK_TIMEOUT_ENABLE_PARAM, ui->checkBoxLock},
        {MPParams::LOCK_TIMEOUT_PARAM, ui->spinBoxLock},
        {MPParams::SCREENSAVER_PARAM, ui->checkBoxScreensaver},
        {MPParams::USER_REQ_CANCEL_PARAM, ui->checkBoxInput},
        {MPParams::USER_INTER_TIMEOUT_PARAM, ui->spinBoxInput},
        {MPParams::FLASH_SCREEN_PARAM, ui->checkBoxFlash},
        {MPParams::OFFLINE_MODE_PARAM, ui->checkBoxBoot},
        {MPParams::TUTORIAL_BOOL_PARAM, ui->checkBoxTuto},
        {MPParams::KEY_AFTER_LOGIN_SEND_BOOL_PARAM, ui->checkBoxSendAfterLogin},
        {MPParams::KEY_AFTER_LOGIN_SEND_PARAM, ui->comboBoxLoginOutput},
        {MPParams::KEY_AFTER_PASS_SEND_BOOL_PARAM, ui->checkBoxSendAfterPassword},
        {MPParams::KEY_AFTER_PASS_SEND_PARAM, ui->comboBoxPasswordOutput},
        {MPParams::DELAY_AFTER_KEY_ENTRY_BOOL_PARAM, ui->checkBoxSlowHost},
        {MPParams::DELAY_AFTER_KEY_ENTRY_PARAM, ui->spinBoxInputDelayAfterKeyPressed},
        {MPParams::MINI_OLED_CONTRAST_CURRENT_PARAM, ui->comboBoxScreenBrightness},
        {MPParams::RANDOM_INIT_PIN_PARAM, ui->randomStartingPinCheckBox},
        {MPParams::MINI_KNOCK_DETECT_ENABLE_PARAM, ui->checkBoxKnock},
        {MPParams::MINI_KNOCK_THRES_PARAM, ui->comboBoxKnock},
        {MPParams::HASH_DISPLAY_FEATURE_PARAM, ui->hashDisplayFeatureCheckBox},
        {MPParams::LOCK_UNLOCK_FEATURE_PARAM, ui->lockUnlockModeComboBox},
        {MPParams::RESERVED_BLE, ui->checkBoxBLEReserved},
        {MPParams::PROMPT_ANIMATION_PARAM, ui->checkBoxPromptAnim},
        {MPParams::DEVICE_LANGUAGE, ui->comboBoxDeviceLang},
        {MPParams::USER_LANGUAGE, ui->comboBoxUserLanguage},
        {MPParams::KEYBOARD_USB_LAYOUT, ui->comboBoxUsbLayout},
        {MPParams::KEYBOARD_BT_LAYOUT, ui->comboBoxBtLayout},
        {MPParams::BOOT_ANIMATION_PARAM, ui->checkBoxBootAnim},
        {MPParams::DEVICE_LOCK_USB_DISC, ui->checkBoxDeviceLockUSBDisc},
        {MPParams::PIN_SHOWN_ON_BACK, ui->checkBoxPinOnBack}
    };
    //When something changed in GUI, show save/reset buttons
    for (const auto& widget : m_widgetMapping)
    {
        std::string signal;
        if (dynamic_cast<QComboBox*>(widget))
        {
            signal = SIGNAL(currentIndexChanged(int));
        } else if (dynamic_cast<QCheckBox*>(widget))
        {
            signal = SIGNAL(toggled(bool));
        } else if (dynamic_cast<QSpinBox*>(widget))
        {
            signal = SIGNAL(valueChanged(int));
        }
        connect(widget, signal.c_str(), m_mw, SLOT(checkSettingsChanged()));
    }
}

void SettingsGuiHelper::createSettingUIMapping()
{
    const auto type = m_wsClient->get_mpHwVersion();
    if (type != m_deviceType)
    {
        delete m_settings;
    }
    else if (m_settings)
    {
        // If type is not different do not recreate m_settings
        return;
    }
    m_deviceType = type;
    if (m_deviceType == Common::MP_BLE)
    {
        m_settings = new SettingsGuiBLE(this, m_mw);
    }
    else if (m_deviceType == Common::MP_Mini || m_deviceType == Common::MP_Classic)
    {
        m_settings = new SettingsGuiMini(this, m_mw);
    }
    else
    {
        return;
    }

    m_settings->setupKeyboardLayout();
    m_settings->connectSendParams(this);
    dynamic_cast<ISettingsGui*>(m_settings)->updateUI();
    initKnockSetting();
}

bool SettingsGuiHelper::checkSettingsChanged()
{
    auto* metaObj = m_settings->getMetaObject();
    while (nullptr != metaObj && QString{metaObj->className()} != "QObject")
    {
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i)
        {
            const auto paramId = m_settings->getParamId(metaObj->property(i).name());
            QWidget* widget = m_widgetMapping[paramId];
            const auto val = metaObj->property(i).read(m_settings);
            if (auto* combobox = dynamic_cast<QComboBox*>(widget))
            {
                if (combobox->currentData().toInt() != val.toInt())
                {
                    return true;
                }
            }
            else if (auto* checkBox = dynamic_cast<QCheckBox*>(widget))
            {
                if (checkBox->isChecked() != val.toBool())
                {
                    return true;
                }
            }
            else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
            {
                if (spinBox->value() != val.toInt())
                {
                    return true;
                }
            }
        }
        metaObj = metaObj->superClass();
    }
    return false;
}

void SettingsGuiHelper::resetSettings()
{
    auto* metaObj = m_settings->getMetaObject();
    while (nullptr != metaObj && QString{metaObj->className()} != "QObject")
    {
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i)
        {
            const auto paramId = m_settings->getParamId(metaObj->property(i).name());
            QWidget* widget = m_widgetMapping[paramId];
            const auto val = metaObj->property(i).read(m_settings);
            if (auto* combobox = dynamic_cast<QComboBox*>(widget))
            {
                updateComboBoxIndex(combobox, val.toInt());
            }
            else if (auto* checkBox = dynamic_cast<QCheckBox*>(widget))
            {
                checkBox->setChecked(val.toBool());
            }
            else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
            {
                spinBox->setValue(val.toInt());
            }
        }
        metaObj = metaObj->superClass();
    }
}

void SettingsGuiHelper::getChangedSettings(QJsonObject &o)
{
    auto* metaObj = m_settings->getMetaObject();
    while (nullptr != metaObj && QString{metaObj->className()} != "QObject")
    {
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i)
        {
            const QString paramName = metaObj->property(i).name();
            const auto paramId = m_settings->getParamId(metaObj->property(i).name());
            QWidget* widget = m_widgetMapping[paramId];
            const auto val = metaObj->property(i).read(m_settings);
            if (auto* combobox = dynamic_cast<QComboBox*>(widget))
            {
                if (combobox->currentData().toInt() != val.toInt())
                {
                    o[paramName] = combobox->currentData().toInt();
                }
            }
            else if (auto* checkBox = dynamic_cast<QCheckBox*>(widget))
            {
                if (checkBox->isChecked() != val.toBool())
                {
                    o[paramName] = checkBox->isChecked();
                }
            }
            else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
            {
                if (spinBox->value() != val.toInt())
                {
                    o[paramName] = spinBox->value();
                }
            }
        }
        metaObj = metaObj->superClass();
    }
}

void SettingsGuiHelper::updateParameters(const QJsonObject &data)
{
    QString param = data["parameter"].toString();
    int val = data["value"].toInt();
    if (data["value"].isBool())
    {
        val = data["value"].toBool();
    }
    m_settings->updateParam(m_settings->getParamId(param), val);
}

void SettingsGuiHelper::sendParams(bool value, int param)
{
    QWidget* widget = m_widgetMapping[MPParams::Param(param)];
    if (auto* comboBox = dynamic_cast<QCheckBox*>(widget))
    {
        comboBox->setChecked(value);
    }
    else
    {
        qCritical() << "Invalid widget";
    }
    m_mw->checkSettingsChanged();
}

void SettingsGuiHelper::sendParams(int value, int param)
{
    QWidget* widget = m_widgetMapping[MPParams::Param(param)];
    if (auto* comboBox = dynamic_cast<QComboBox*>(widget))
    {
        updateComboBoxIndex(comboBox, value);
    }
    else if (auto* spinBox = dynamic_cast<QSpinBox*>(widget))
    {
        spinBox->setValue(value);
    }
    else
    {
        qCritical() << "Invalid widget";
    }
    m_mw->checkSettingsChanged();
}

void SettingsGuiHelper::initKnockSetting()
{
    if (m_wsClient->isMPBLE())
    {
        m_mw->enableKnockSettings(true);
    }
    else
    {
        m_mw->enableKnockSettings(m_wsClient->get_status() == Common::NoCardInserted);
    }
}

void SettingsGuiHelper::checkKeyboardLayout()
{
    if (m_wsClient->isMPBLE())
    {
        return;
    }
    const auto containsFw13LangLayouts = ui->comboBoxLang->findText(m_fw13LangMap.firstKey()) != -1;
    const bool isFw13 = m_wsClient->isFw13();
    if (!containsFw13LangLayouts && isFw13)
    {
        for (const auto& langs : m_fw13LangMap.toStdMap())
        {
            ui->comboBoxLang->addItem(langs.first, langs.second);
        }
        ui->comboBoxLang->model()->sort(0);
    }
    else if (containsFw13LangLayouts && !isFw13)
    {
        for (const auto& lang : m_fw13LangMap.keys())
        {
            ui->comboBoxLang->removeItem(ui->comboBoxLang->findText(lang));
        }
    }
}
