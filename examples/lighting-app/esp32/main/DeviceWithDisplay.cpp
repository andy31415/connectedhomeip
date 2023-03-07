/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "DeviceWithDisplay.h"
#include "Globals.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

#if CONFIG_HAVE_DISPLAY
using namespace ::chip;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceManager;
using namespace ::chip::DeviceLayer;

static const char * TAG = "DeviceWithDisplay";

#if CONFIG_DEVICE_TYPE_M5STACK

Button gButtons[BUTTON_NUMBER] = { Button(BUTTON_1_GPIO_NUM), Button(BUTTON_2_GPIO_NUM), Button(BUTTON_3_GPIO_NUM) };

// Pretend these are devices with endpoints with clusters with attributes
typedef std::tuple<std::string, std::string> Attribute;
typedef std::vector<Attribute> Attributes;
typedef std::tuple<std::string, Attributes> Cluster;
typedef std::vector<Cluster> Clusters;
typedef std::tuple<std::string, Clusters> Endpoint;
typedef std::vector<Endpoint> Endpoints;
typedef std::tuple<std::string, Endpoints> Device;
typedef std::vector<Device> Devices;
Devices devices;

void AddAttribute(std::string name, std::string value)
{
    Attribute attribute = std::make_tuple(std::move(name), std::move(value));
    std::get<1>(std::get<1>(std::get<1>(devices.back()).back()).back()).emplace_back(std::move(attribute));
}

void AddCluster(std::string name)
{
    Cluster cluster = std::make_tuple(std::move(name), std::move(Attributes()));
    std::get<1>(std::get<1>(devices.back()).back()).emplace_back(std::move(cluster));
}

void AddEndpoint(std::string name)
{
    Endpoint endpoint = std::make_tuple(std::move(name), std::move(Clusters()));
    std::get<1>(devices.back()).emplace_back(std::move(endpoint));
}

void AddDevice(std::string name)
{
    Device device = std::make_tuple(std::move(name), std::move(Endpoints()));
    devices.emplace_back(std::move(device));
}

class TouchesMatterStackModel : public ListScreen::Model
{
    // We could override Action() and then hope focusIndex has not changed by
    // the time our queued task runs, but it's cleaner to just capture its value
    // now.
    struct QueuedAction
    {
        QueuedAction(TouchesMatterStackModel * selfArg, int iArg) : self(selfArg), i(iArg) {}
        TouchesMatterStackModel * self;
        int i;
    };

    void ItemAction(int i) final
    {
        auto * action = chip::Platform::New<QueuedAction>(this, i);
        chip::DeviceLayer::PlatformMgr().ScheduleWork(QueuedActionHandler, reinterpret_cast<intptr_t>(action));
    }

    static void QueuedActionHandler(intptr_t closure)
    {
        auto * queuedAction = reinterpret_cast<QueuedAction *>(closure);
        queuedAction->self->DoAction(queuedAction->i);
        chip::Platform::Delete(queuedAction);
    }

    virtual void DoAction(int i) = 0;
};

class EditAttributeListModel : public TouchesMatterStackModel
{
    int deviceIndex;
    int endpointIndex;
    int clusterIndex;
    int attributeIndex;

public:
    EditAttributeListModel(int deviceIndex, int endpointIndex, int clusterIndex, int attributeIndex) :
        deviceIndex(deviceIndex), endpointIndex(endpointIndex), clusterIndex(clusterIndex), attributeIndex(attributeIndex)
    {}
    Attribute & attribute()
    {
        return std::get<1>(std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex])[clusterIndex])[attributeIndex];
    }
    bool IsBooleanAttribute()
    {
        auto & attribute = this->attribute();
        auto & value     = std::get<1>(attribute);
        return value == "On" || value == "Off" || value == "Yes" || value == "No";
    }
    virtual std::string GetTitle()
    {
        auto & attribute = this->attribute();
        auto & name      = std::get<0>(attribute);
        auto & value     = std::get<1>(attribute);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s : %s", name.c_str(), value.c_str());
        return buffer;
    }
    virtual int GetItemCount() { return IsBooleanAttribute() ? 1 : 2; }
    virtual std::string GetItemText(int i)
    {
        if (IsBooleanAttribute())
        {
            return "Toggle";
        }
        return i == 0 ? "+" : "-";
    }

    void DoAction(int i) override
    {
        auto & attribute = this->attribute();
        auto & value     = std::get<1>(attribute);
        int n;
        if (sscanf(value.c_str(), "%d", &n) == 1)
        {
            auto & name = std::get<0>(attribute);

            ESP_LOGI(TAG, "editing attribute as integer: %d (%s)", n, i == 0 ? "+" : "-");
            n += (i == 0) ? 1 : -1;
            char buffer[32];
            sprintf(buffer, "%d", n);
            if (name == "Color Current Level")
            {
                // update the current level here for hardcoded endpoint 1
                ESP_LOGI(TAG, "Brightness changed to : %d", (n * 100 / 255));
                app::Clusters::LevelControl::Attributes::CurrentLevel::Set(1, n);
            }
            else if (name == "Current Hue")
            {
                // update the current hue here for hardcoded endpoint 1
                ESP_LOGI(TAG, "Hue changed to : %d", n * 360 / 254);
                app::Clusters::ColorControl::Attributes::CurrentHue::Set(1, n);
            }
            else if (name == "Current Saturation")
            {
                // update the current saturation here for hardcoded endpoint 1
                ESP_LOGI(TAG, "Saturation changed to : %d", n * 100 / 254);
                app::Clusters::ColorControl::Attributes::CurrentSaturation::Set(1, n);
            }
            value = buffer;
        }
        else if (IsBooleanAttribute())
        {
            auto & name    = std::get<0>(attribute);
            auto & cluster = std::get<0>(std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex])[i]);

            if (name == "OnOff" && cluster == "OnOff")
            {
                value               = (value == "On") ? "Off" : "On";
                bool attributeValue = (value == "On");
                app::Clusters::OnOff::Attributes::OnOff::Set(endpointIndex + 1, attributeValue);
            }
        }
    }
};

class AttributeListModel : public ListScreen::Model
{
    int deviceIndex;
    int endpointIndex;
    int clusterIndex;

public:
    AttributeListModel(int deviceIndex, int endpointIndex, int clusterIndex) :
        deviceIndex(deviceIndex), endpointIndex(endpointIndex), clusterIndex(clusterIndex)
    {}
    virtual std::string GetTitle() { return "Attributes"; }
    virtual int GetItemCount()
    {
        return std::get<1>(std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex])[clusterIndex]).size();
    }
    virtual std::string GetItemText(int i)
    {
        auto & attribute = std::get<1>(std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex])[clusterIndex])[i];
        auto & name      = std::get<0>(attribute);
        auto & value     = std::get<1>(attribute);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s : %s", name.c_str(), value.c_str());
        return buffer;
    }
    virtual void ItemAction(int i)
    {
        ESP_LOGI(TAG, "Opening attribute %d", i);
        ScreenManager::PushScreen(chip::Platform::New<ListScreen>(
            chip::Platform::New<EditAttributeListModel>(deviceIndex, endpointIndex, clusterIndex, i)));
    }
};
class ClusterListModel : public ListScreen::Model
{
    int deviceIndex;
    int endpointIndex;

public:
    ClusterListModel(int deviceIndex, int endpointIndex) : deviceIndex(deviceIndex), endpointIndex(endpointIndex) {}
    virtual std::string GetTitle() { return "Clusters"; }
    virtual int GetItemCount() { return std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex]).size(); }
    virtual std::string GetItemText(int i) { return std::get<0>(std::get<1>(std::get<1>(devices[deviceIndex])[endpointIndex])[i]); }
    virtual void ItemAction(int i)
    {
        ESP_LOGI(TAG, "Opening cluster %d", i);
        ScreenManager::PushScreen(
            chip::Platform::New<ListScreen>(chip::Platform::New<AttributeListModel>(deviceIndex, endpointIndex, i)));
    }
};

class EndpointListModel : public ListScreen::Model
{
    int deviceIndex;

public:
    EndpointListModel(int deviceIndex) : deviceIndex(deviceIndex) {}
    virtual std::string GetTitle() { return "Endpoints"; }
    virtual int GetItemCount() { return std::get<1>(devices[deviceIndex]).size(); }
    virtual std::string GetItemText(int i) { return std::get<0>(std::get<1>(devices[deviceIndex])[i]); }
    virtual void ItemAction(int i)
    {
        ESP_LOGI(TAG, "Opening endpoint %d", i);
        ScreenManager::PushScreen(chip::Platform::New<ListScreen>(chip::Platform::New<ClusterListModel>(deviceIndex, i)));
    }
};

class DeviceListModel : public ListScreen::Model
{
public:
    virtual std::string GetTitle() { return "Devices"; }
    virtual int GetItemCount() { return devices.size(); }
    virtual std::string GetItemText(int i) { return std::get<0>(devices[i]); }
    virtual void ItemAction(int i)
    {
        ESP_LOGI(TAG, "Opening device %d", i);
        ScreenManager::PushScreen(chip::Platform::New<ListScreen>(chip::Platform::New<EndpointListModel>(i)));
    }
};

class ActionListModel : public ListScreen::Model
{
    int GetItemCount() override { return static_cast<int>(mActions.size()); }
    std::string GetItemText(int i) override { return mActions[i].title.c_str(); }
    void ItemAction(int i) override
    {
        ESP_LOGI(TAG, "generic action %d", i);
        mActions[i].action();
    }

protected:
    void AddAction(const char * name, std::function<void(void)> action) { mActions.push_back(Action(name, action)); }

private:
    struct Action
    {
        std::string title;
        std::function<void(void)> action;

        Action(const char * t, std::function<void(void)> a) : title(t), action(a) {}
    };

    std::vector<Action> mActions;
};

class MdnsDebugListModel : public ActionListModel
{
public:
    std::string GetTitle() override { return "mDNS Debug"; }

    MdnsDebugListModel() { AddAction("(Re-)Init", std::bind(&MdnsDebugListModel::DoReinit, this)); }

private:
    void DoReinit()
    {
        CHIP_ERROR err = Dnssd::ServiceAdvertiser::Instance().Init(DeviceLayer::UDPEndPointManager());
        if (err != CHIP_NO_ERROR)
        {
            ESP_LOGE(TAG, "Error initializing: %s", err.AsString());
        }
    }
};

class SetupListModel : public TouchesMatterStackModel
{
public:
    SetupListModel()
    {
        std::string resetWiFi                   = "Reset WiFi";
        std::string resetToFactory              = "Reset to factory";
        std::string forceWiFiCommissioningBasic = "Force WiFi commissioning (basic)";
        options.emplace_back(resetWiFi);
        options.emplace_back(resetToFactory);
        options.emplace_back(forceWiFiCommissioningBasic);
    }
    virtual std::string GetTitle() { return "Setup"; }
    virtual int GetItemCount() { return options.size(); }
    virtual std::string GetItemText(int i) { return options.at(i); }
    void DoAction(int i) override
    {
        ESP_LOGI(TAG, "Opening options %d: %s", i, GetItemText(i).c_str());
        if (i == 0)
        {
            ConnectivityMgr().ClearWiFiStationProvision();
            chip::Server::GetInstance().GetFabricTable().DeleteAllFabrics();
            chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow();
        }
        else if (i == 1)
        {
            chip::Server::GetInstance().ScheduleFactoryReset();
        }
        else if (i == 2)
        {
            chip::Server::GetInstance().GetFabricTable().DeleteAllFabrics();
            auto & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
            commissionMgr.OpenBasicCommissioningWindow(commissionMgr.MaxCommissioningTimeout(),
                                                       CommissioningWindowAdvertisement::kDnssdOnly);
        }
    }

private:
    std::vector<std::string> options;
};
class CustomScreen : public Screen
{
public:
    virtual void Display()
    {
        TFT_drawCircle(0.3 * DisplayWidth, 0.3 * DisplayHeight, 8, TFT_BLUE);
        TFT_drawCircle(0.7 * DisplayWidth, 0.3 * DisplayHeight, 8, TFT_BLUE);
        TFT_drawLine(0.2 * DisplayWidth, 0.6 * DisplayHeight, 0.3 * DisplayWidth, 0.7 * DisplayHeight, TFT_BLUE);
        TFT_drawLine(0.3 * DisplayWidth, 0.7 * DisplayHeight, 0.7 * DisplayWidth, 0.7 * DisplayHeight, TFT_BLUE);
        TFT_drawLine(0.7 * DisplayWidth, 0.7 * DisplayHeight, 0.8 * DisplayWidth, 0.6 * DisplayHeight, TFT_BLUE);
    }
};

void SetupPretendDevices()
{
    AddDevice("Light Bulb");
    AddEndpoint("1");
    AddCluster("OnOff");
    AddAttribute("OnOff", "Off");
    AddCluster("Level Control");
    AddAttribute("Current Level", "255");

    AddDevice("Color Light");
    AddEndpoint("1");
    AddCluster("OnOff");
    AddAttribute("OnOff", "Off");
    app::Clusters::OnOff::Attributes::OnOff::Set(1, false);
    AddCluster("Level Control");
    AddAttribute("Color Current Level", "255");
    app::Clusters::LevelControl::Attributes::CurrentLevel::Set(1, 255);
    AddCluster("Color Control");
    AddAttribute("Current Hue", "200");
    app::Clusters::ColorControl::Attributes::CurrentHue::Set(1, 200);
    AddAttribute("Current Saturation\n", "150");
    app::Clusters::ColorControl::Attributes::CurrentSaturation::Set(1, 150);
}

esp_err_t InitM5Stack(std::string qrCodeText)
{
    esp_err_t err;
    // Initialize the buttons.
    err = gpio_install_isr_service(0);
    ESP_RETURN_ON_ERROR(err, TAG, "Button preInit failed: %s", esp_err_to_name(err));
    for (int i = 0; i < BUTTON_NUMBER; ++i)
    {
        err = gButtons[i].Init();
        ESP_RETURN_ON_ERROR(err, TAG, "Button.Init() failed: %s", esp_err_to_name(err));
    }
    // Push a rudimentary user interface.
    ScreenManager::PushScreen(chip::Platform::New<ListScreen>(
        (chip::Platform::New<SimpleListModel>())
            ->Title("CHIP")
            ->Action([](int i) { ESP_LOGI(TAG, "action on item %d", i); })
            ->Item("Devices",
                   []() {
                       ESP_LOGI(TAG, "Opening device list");
                       ScreenManager::PushScreen(chip::Platform::New<ListScreen>(chip::Platform::New<DeviceListModel>()));
                   })
            ->Item("mDNS Debug",
                   []() {
                       ESP_LOGI(TAG, "Opening MDNS debug");
                       ScreenManager::PushScreen(chip::Platform::New<ListScreen>(chip::Platform::New<MdnsDebugListModel>()));
                   })
            ->Item("QR Code",
                   [=]() {
                       ESP_LOGI(TAG, "Opening QR code screen");
                       PrintOnboardingCodes(chip::RendezvousInformationFlags(CONFIG_RENDEZVOUS_MODE));
                       ScreenManager::PushScreen(chip::Platform::New<QRCodeScreen>(qrCodeText));
                   })
            ->Item("Setup",
                   [=]() {
                       ESP_LOGI(TAG, "Opening Setup list");
                       ScreenManager::PushScreen(chip::Platform::New<ListScreen>(chip::Platform::New<SetupListModel>()));
                   })
            ->Item("Status",
                   [=]() {
                       ESP_LOGI(TAG, "Opening Status screen");
                       ScreenManager::PushScreen(chip::Platform::New<StatusScreen>());
                   })
            ->Item("Custom",
                   []() {
                       ESP_LOGI(TAG, "Opening custom screen");
                       ScreenManager::PushScreen(chip::Platform::New<CustomScreen>());
                   })
            ->Item("More")
            ->Item("Items")
            ->Item("For")
            ->Item("Demo")));
    return ESP_OK;
}
#endif

void InitDeviceDisplay()
{
    // Create buffer for QR code that can fit max size and null terminator.
    char qrCodeBuffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
    chip::MutableCharSpan qrCodeText(qrCodeBuffer);

    // Get QR Code and emulate its content using NFC tag
    GetQRCode(qrCodeText, chip::RendezvousInformationFlags(CONFIG_RENDEZVOUS_MODE));

    // Initialize the display device.
    esp_err_t err = InitDisplay();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "InitDisplay() failed: %s", esp_err_to_name(err));
        return;
    }

    // Initialize the screen manager
    ScreenManager::Init();

    // Connect the status LED to VLEDs.
    int vled1 = ScreenManager::AddVLED(TFT_GREEN);
    statusLED1.SetVLED(vled1);

    int vled2 = ScreenManager::AddVLED(TFT_CYAN);
    statusLED2.SetVLED(vled2);

    bluetoothLED.SetVLED(ScreenManager::AddVLED(TFT_BLUE));
    wifiLED.SetVLED(ScreenManager::AddVLED(TFT_YELLOW));
    pairingWindowLED.SetVLED(ScreenManager::AddVLED(TFT_ORANGE));

#if CONFIG_DEVICE_TYPE_M5STACK

    InitM5Stack(qrCodeText.data());

#elif CONFIG_DEVICE_TYPE_ESP32_WROVER_KIT

    // Display the QR Code
    QRCodeScreen qrCodeScreen(qrCodeText.data());
    qrCodeScreen.Display();

#endif
}
#endif
