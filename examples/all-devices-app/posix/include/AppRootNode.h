/*
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
#pragma once

#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceConfig.h>

#include <app/CASESessionManager.h>
#include <app/InteractionModelEngine.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <data-model-providers/codedriven/endpoint/EndpointInterfaceRegistry.h>
#include <device/api/Interface.h>
#include <devices/Types.h>

#include <device/types/root-node/RootNode.h>
#include <device/types/root-node/RootNodeWith.h>
#include <device/types/root-node/features/PowerSourceFeature.h>
#include <device/types/root-node/features/TimeSynchronizationFeature.h>
#include <device/types/root-node/features/TlsFeature.h>
#include <lib/support/Span.h>

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <device/types/root-node/features/WifiFeature.h>
#if CHIP_DEVICE_LAYER_TARGET_DARWIN
#include <platform/Darwin/WiFi/NetworkCommissioningWiFiDriver.h>
#else
#include <platform/Linux/NetworkCommissioningDriver.h>
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#include <posix/emulated_network/EmulatedWiFiDriver.h>
#endif
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <device/types/root-node/features/ThreadFeature.h> // nogncheck
#if CHIP_SYSTEM_CONFIG_USE_OPENTHREAD_ENDPOINT
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h> // nogncheck
#else
#include <platform/Linux/NetworkCommissioningDriver.h> // nogncheck
#endif
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

namespace chip::app {

/// Provides an application root node for darwin and linux.
///
/// In particular if the SDK supports WIFI, it will also provide the ability
/// to enable wifi in the root node.
class AppRootNode
{
public:
    enum class EnabledFeatures : uint32_t
    {
        kNone = 0,
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        kWiFi         = 0x01, // enable wifi support
        kEmulatedWiFi = 0x02, // with kWiFi: use EmulatedWiFiDriver instead of the platform driver
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        kThread = 0x04, // enable thread support
#endif
        kPowerSource = 0x08, // enable Power Source cluster on root node
        kTimeSync    = 0x10, // enable Time Synchronization cluster on root node
        kTls         = 0x20, // enable TLS Certificate & Client Management clusters on root node
    };

    struct FeatureContext
    {
        CASESessionManager & caseSessionManager;
        InteractionModelEngine & interactionModelEngine;
    };

    AppRootNode(const RootNode::Context & context, const FeatureContext & featureContext, BitFlags<EnabledFeatures> features = {}) :
        mEnabledFeatures(features),
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        mWiFiRootNode(context, WifiFeature::Context{ .wifiDriver = SelectedWiFiDriver() }, MakePowerSourceContext(features),
                      MakeTimeSyncContext(features, featureContext), MakeTlsContext(features)),
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        mThreadRootNode(context, ThreadFeature::Context{ .threadDriver = mThreadDriver }, MakePowerSourceContext(features),
                        MakeTimeSyncContext(features, featureContext), MakeTlsContext(features)),
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD
        mRootNode(context, MakePowerSourceContext(features), MakeTimeSyncContext(features, featureContext),
                  MakeTlsContext(features))
    {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI && CHIP_DEVICE_CONFIG_ENABLE_THREAD
        VerifyOrDie(!(mEnabledFeatures.Has(EnabledFeatures::kWiFi) && mEnabledFeatures.Has(EnabledFeatures::kThread)));
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI && CHIP_DEVICE_CONFIG_ENABLE_THREAD
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI && !CHIP_DEVICE_LAYER_TARGET_DARWIN
        mWiFiDriver.Set5gSupport(true);
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI && !CHIP_DEVICE_LAYER_TARGET_DARWIN
    }

    RootNode & RootDevice()
    {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        if (mEnabledFeatures.Has(EnabledFeatures::kWiFi))
        {
            return mWiFiRootNode;
        }
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        if (mEnabledFeatures.Has(EnabledFeatures::kThread))
        {
            return mThreadRootNode;
        }
#endif
        return mRootNode;
    }

    RootNode & GetRootNode() { return RootDevice(); }

private:
    static PowerSourceFeature::Context MakePowerSourceContext(BitFlags<EnabledFeatures> features)
    {
        return PowerSourceFeature::Context{
            .enabled          = features.Has(EnabledFeatures::kPowerSource),
            .description      = "AC Mains"_span,
            .wiredCurrentType = Clusters::PowerSource::WiredCurrentTypeEnum::kAc,
            .order            = 0,
        };
    }

    static TimeSynchronizationFeature::Context MakeTimeSyncContext(BitFlags<EnabledFeatures> features,
                                                                   const FeatureContext & featureContext)
    {
        return TimeSynchronizationFeature::Context{
            .enabled                = features.Has(EnabledFeatures::kTimeSync),
            .caseSessionManager     = featureContext.caseSessionManager,
            .interactionModelEngine = featureContext.interactionModelEngine,
        };
    }

    static TlsFeature::Context MakeTlsContext(BitFlags<EnabledFeatures> features)
    {
        return TlsFeature::Context{
            .enabled = features.Has(EnabledFeatures::kTls),
        };
    }

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    DeviceLayer::NetworkCommissioning::WiFiDriver & SelectedWiFiDriver()
    {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
        if (mEnabledFeatures.Has(EnabledFeatures::kEmulatedWiFi))
        {
            return mEmulatedWiFiDriver;
        }
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
        return mWiFiDriver;
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

    BitFlags<EnabledFeatures> mEnabledFeatures;

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI

#if CHIP_DEVICE_LAYER_TARGET_DARWIN
    DeviceLayer::NetworkCommissioning::DarwinWiFiDriver mWiFiDriver;
#else
    DeviceLayer::NetworkCommissioning::LinuxWiFiDriver mWiFiDriver;
#endif // CHIP_DEVICE_LAYER_TARGET_DARWIN
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
    EmulatedWiFiDriver mEmulatedWiFiDriver;
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION

    RootNodeWith<WifiFeature, PowerSourceFeature, TimeSynchronizationFeature, TlsFeature> mWiFiRootNode;
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#if CHIP_SYSTEM_CONFIG_USE_OPENTHREAD_ENDPOINT
    DeviceLayer::NetworkCommissioning::GenericThreadDriver mThreadDriver;
#else
    DeviceLayer::NetworkCommissioning::LinuxThreadDriver mThreadDriver;
#endif // CHIP_SYSTEM_CONFIG_USE_OPENTHREAD_ENDPOINT

    RootNodeWith<ThreadFeature, PowerSourceFeature, TimeSynchronizationFeature, TlsFeature> mThreadRootNode;
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

    RootNodeWith<PowerSourceFeature, TimeSynchronizationFeature, TlsFeature> mRootNode;
};

} // namespace chip::app
