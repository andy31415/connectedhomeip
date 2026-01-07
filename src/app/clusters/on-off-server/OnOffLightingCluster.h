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

#pragma once

#include "OnOffCluster.h"
#include <lib/support/TimerDelegate.h>

namespace chip::app::Clusters::OnOff {

/// Implements the On/Off cluster with Lighting features.
///
/// This includes:
/// - GlobalSceneControl
/// - OnTime / OffWaitTime
/// - StartUpOnOff
/// - Timed commands
class OnOffLightingCluster : public OnOffCluster, public TimerContext
{
public:
    OnOffLightingCluster(EndpointId endpointId, OnOffDelegate & delegate, TimerDelegate & timerDelegate,
                         BitMask<Feature> featureMap = BitMask<Feature>(Feature::kLighting));

    ~OnOffLightingCluster() override;

    CHIP_ERROR Startup(ServerClusterContext & context) override;

    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;

    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;
    DataModel::ActionReturnStatus WriteAttribute(const DataModel::WriteAttributeRequest & request,
                                                 AttributeValueDecoder & decoder) override;

    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

    CHIP_ERROR SetOnOff(bool on) override;

    // TimerContext
    void TimerFired() override;

private:
    TimerDelegate & mTimerDelegate;

    // Lighting Attributes
    bool mGlobalSceneControl = true;
    uint16_t mOnTime         = 0;
    uint16_t mOffWaitTime    = 0;
    DataModel::Nullable<StartUpOnOffEnum> mStartUpOnOff;

    // Timer logic
    void UpdateTimer();

    // Command Handlers
    DataModel::ActionReturnStatus HandleOffWithEffect(const DataModel::InvokeRequest & request,
                                                      chip::TLV::TLVReader & input_arguments);
    DataModel::ActionReturnStatus HandleOnWithRecallGlobalScene();
    DataModel::ActionReturnStatus HandleOnWithTimedOff(chip::TLV::TLVReader & input_arguments);

    // Wrappers for basic commands to add lighting side effects
    DataModel::ActionReturnStatus HandleOn();
    DataModel::ActionReturnStatus HandleOff();
    DataModel::ActionReturnStatus HandleToggle();
};

} // namespace chip::app::Clusters::OnOff
