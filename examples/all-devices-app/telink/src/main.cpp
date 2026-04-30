/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lib/support/CHIPMem.h>
#include <platform/CHIPDeviceLayer.h>

#include <app/server/Server.h>
#include <app/persistence/DefaultAttributePersistenceProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <devices/device-factory/DeviceFactory.h>
#include <devices/root-node/RootNodeDevice.h>

#include "AppTaskCommon.h"

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/telink/wifi/TelinkWiFiDriver.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#endif

#if CHIP_ENABLE_OPENTHREAD
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#endif

#if CONFIG_CHIP_LIB_SHELL
#include <lib/shell/Engine.h>
#include <lib/shell/commands/Help.h>
#include <lib/shell/streamer.h>
#endif
#include <app/InteractionModelEngine.h>
#include <lib/support/CodeUtils.h>

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer;

LOG_MODULE_REGISTER(app, CONFIG_CHIP_APP_LOG_LEVEL);

namespace {



chip::app::DefaultAttributePersistenceProvider gAttributePersistenceProvider;
chip::app::DefaultSafeAttributePersistenceProvider gSafeAttributePersistenceProvider;
Credentials::GroupDataProviderImpl gGroupDataProvider;
chip::app::CodeDrivenDataModelProvider * gDataModelProvider = nullptr;
std::unique_ptr<chip::app::RootNodeDevice> gRootNodeDevice;
std::unique_ptr<chip::app::DeviceInterface> gConstructedDevice;
DefaultTimerDelegate gTimerDelegate;

std::string gDeviceType = "contact-sensor";

#if CONFIG_CHIP_LIB_SHELL
using namespace chip::Shell;

CHIP_ERROR cmd_devtype(int argc, char ** argv)
{
    if (argc == 0)
    {
        streamer_printf(streamer_get(), "Current device type: %s\r\n", gDeviceType.c_str());
        return CHIP_NO_ERROR;
    }
    if (argc == 1)
    {
        gDeviceType = argv[0];
        streamer_printf(streamer_get(), "Device type set to: %s\r\n", gDeviceType.c_str());
        streamer_printf(streamer_get(), "Please reboot to apply changes.\r\n");
        return CHIP_NO_ERROR;
    }
    return CHIP_ERROR_INVALID_ARGUMENT;
}

void RegisterDeviceCommands()
{
    static const shell_command_t sDeviceCommands[] = {
        { &cmd_devtype, "devtype", "Get/Set device type. Usage: devtype [type]" },
    };
    Engine::Root().RegisterCommands(sDeviceCommands, MATTER_ARRAY_SIZE(sDeviceCommands));
}
#endif

chip::app::DataModel::Provider * PopulateCodeDrivenDataModelProvider(PersistentStorageDelegate * delegate)
{
    // Initialize the attribute persistence provider with the storage delegate
    CHIP_ERROR err = gAttributePersistenceProvider.Init(delegate);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to init attribute persistence provider");
        return nullptr;
    }

    // Initialize the safe attribute persistence provider with the storage delegate
    err = gSafeAttributePersistenceProvider.Init(delegate);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to init safe attribute persistence provider");
        return nullptr;
    }
    SetSafeAttributePersistenceProvider(&gSafeAttributePersistenceProvider);

    static chip::app::CodeDrivenDataModelProvider dataModelProvider =
        chip::app::CodeDrivenDataModelProvider(*delegate, gAttributePersistenceProvider);

    gDataModelProvider = &dataModelProvider;

    DeviceLayer::DeviceInstanceInfoProvider * provider = DeviceLayer::GetDeviceInstanceInfoProvider();
    if (provider == nullptr)
    {
        LOG_ERR("Failed to get DeviceInstanceInfoProvider.");
        return nullptr;
    }

    gRootNodeDevice = std::make_unique<chip::app::RootNodeDevice>(
        chip::app::RootNodeDevice::Context {
            .commissioningWindowManager           = Server::GetInstance().GetCommissioningWindowManager(),
            .configurationManager             = DeviceLayer::ConfigurationMgr(),
            .deviceControlServer              = DeviceLayer::DeviceControlServer::DeviceControlSvr(),
            .fabricTable                      = Server::GetInstance().GetFabricTable(),
            .accessControl                    = Server::GetInstance().GetAccessControl(),
            .persistentStorage                = Server::GetInstance().GetPersistentStorage(),
            .failSafeContext                  = Server::GetInstance().GetFailSafeContext(),
            .deviceInstanceInfoProvider       = *provider,
            .platformManager                  = DeviceLayer::PlatformMgr(),
            .groupDataProvider                = gGroupDataProvider,
            .sessionManager                   = Server::GetInstance().GetSecureSessionManager(),
            .dnssdServer                      = DnssdServer::Instance(),
            .deviceLoadStatusProvider         = *InteractionModelEngine::GetInstance(),
            .diagnosticDataProvider           = DeviceLayer::GetDiagnosticDataProvider(),
            .testEventTriggerDelegate         = nullptr, // TODO
            .dacProvider                      = *Credentials::GetDeviceAttestationCredentialsProvider(),
            .eventManagement                  = EventManagement::GetInstance(),
            .safeAttributePersistenceProvider = gSafeAttributePersistenceProvider,
            .timerDelegate                    = gTimerDelegate,
        });

    err = gRootNodeDevice->Register(kRootEndpointId, dataModelProvider, kInvalidEndpointId);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to register root node device");
        return nullptr;
    }

    // Default to contact-sensor device type
    const char * deviceType = gDeviceType.c_str();
    gConstructedDevice      = chip::app::DeviceFactory::GetInstance().Create(deviceType);
    if (gConstructedDevice == nullptr)
    {
        LOG_ERR("Failed to create device of type: %s", deviceType);
        return nullptr;
    }

    err = gConstructedDevice->Register(1, dataModelProvider, kInvalidEndpointId);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to register device");
        return nullptr;
    }

    return &dataModelProvider;
}

void InitServer(intptr_t context)
{
    chip::app::DeviceFactory::GetInstance().Init(chip::app::DeviceFactory::Context{
        .groupDataProvider = gGroupDataProvider,
        .fabricTable       = Server::GetInstance().GetFabricTable(),
        .timerDelegate     = gTimerDelegate,
    });

    static chip::CommonCaseDeviceServerInitParams initParams;
    CHIP_ERROR err = initParams.InitializeStaticResourcesBeforeServerInit();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("InitializeStaticResourcesBeforeServerInit() failed");
        return;
    }

    initParams.groupDataProvider = &gGroupDataProvider;
    gGroupDataProvider.SetStorageDelegate(initParams.persistentStorageDelegate);
    gGroupDataProvider.SetSessionKeystore(initParams.sessionKeystore);
    SuccessOrDie(gGroupDataProvider.Init());
    Credentials::SetGroupDataProvider(&gGroupDataProvider);

    initParams.dataModelProvider = PopulateCodeDrivenDataModelProvider(initParams.persistentStorageDelegate);

    if (initParams.dataModelProvider == nullptr)
    {
        LOG_ERR("Failed to populate data model provider");
        return;
    }

    err = Server::GetInstance().Init(initParams);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Server init failed");
        return;
    }
}

} // namespace

int main(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    err = chip::Platform::MemoryInit();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("MemoryInit fail");
        goto exit;
    }

    err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("InitChipStack fail");
        goto exit;
    }

    err = PlatformMgr().StartEventLoopTask();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("StartEventLoopTask fail");
        goto exit;
    }

#if CHIP_ENABLE_OPENTHREAD
    err = ThreadStackMgr().InitThreadStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("InitThreadStack fail");
        goto exit;
    }

#if defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_ROUTER)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
#elif defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_END_DEVICE)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
#elif defined(CONFIG_CHIP_THREAD_DEVICE_ROLE_SLEEPY_END_DEVICE)
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice);
#else
#error THREAD_DEVICE_ROLE not selected
#endif
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("SetThreadDeviceType fail");
        goto exit;
    }

#endif

    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());

#if CONFIG_CHIP_LIB_SHELL
    RegisterDeviceCommands();
#endif

    SuccessOrDie(PlatformMgr().ScheduleWork(InitServer, reinterpret_cast<intptr_t>(nullptr)));

    k_sleep(K_FOREVER);

exit:
    return (err == CHIP_NO_ERROR) ? EXIT_SUCCESS : EXIT_FAILURE;
}
