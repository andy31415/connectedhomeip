/**
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include "general-diagnostics-server.h"
#include "lib/core/TLVReader.h"
#include "lib/support/CodeUtils.h"

#include <cstdint>
#include <cstring>

#include <app/util/config.h>

#include <access/Privilege.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/CommandHandler.h>
#include <app/CommandHandlerInterface.h>
#include <app/CommandHandlerInterfaceRegistry.h>
#include <app/ConcreteCommandPath.h>
#include <app/EventLogging.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/reporting/reporting.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <app/server-cluster/ServerClusterInterface.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/ScopedBuffer.h>
#include <optional>
#include <platform/ConnectivityManager.h>
#include <platform/DiagnosticDataProvider.h>
#include <zap-generated/gen_config.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::GeneralDiagnostics;
using namespace chip::app::Clusters::GeneralDiagnostics::Attributes;
using namespace chip::DeviceLayer;
using chip::DeviceLayer::ConnectivityMgr;
using chip::DeviceLayer::DiagnosticDataProvider;
using chip::DeviceLayer::GetDiagnosticDataProvider;
using chip::Protocols::InteractionModel::Status;

namespace {

constexpr uint8_t kCurrentClusterRevision = 2;

bool IsTestEventTriggerEnabled()
{
    auto * triggerDelegate = chip::Server::GetInstance().GetTestEventTriggerDelegate();
    if (triggerDelegate == nullptr)
    {
        return false;
    }
    uint8_t zeroByteSpanData[TestEventTriggerDelegate::kEnableKeyLength] = { 0 };
    if (triggerDelegate->DoesEnableKeyMatch(ByteSpan(zeroByteSpanData)))
    {
        return false;
    }
    return true;
}

bool IsByteSpanAllZeros(const ByteSpan & byteSpan)
{
    for (unsigned char it : byteSpan)
    {
        if (it != 0)
        {
            return false;
        }
    }
    return true;
}

void ReportAttributeOnAllEndpoints(AttributeId attribute)
{
    for (auto endpoint : EnabledEndpointsWithServerCluster(GeneralDiagnostics::Id))
    {
        MatterReportingAttributeChangeCallback(endpoint, GeneralDiagnostics::Id, attribute);
    }
}

TestEventTriggerDelegate * GetTriggerDelegateOnMatchingKey(ByteSpan enableKey)
{
    if (enableKey.size() != TestEventTriggerDelegate::kEnableKeyLength)
    {
        return nullptr;
    }

    if (IsByteSpanAllZeros(enableKey))
    {
        return nullptr;
    }

    auto * triggerDelegate = chip::Server::GetInstance().GetTestEventTriggerDelegate();

    if (triggerDelegate == nullptr || !triggerDelegate->DoesEnableKeyMatch(enableKey))
    {
        return nullptr;
    }

    return triggerDelegate;
}

class GeneralDiagosticsGlobalInstance : public DefaultServerCluster, public DeviceLayer::ConnectivityManagerDelegate
{
public:
    // Register for the GeneralDiagnostics cluster on all endpoints.
    GeneralDiagosticsGlobalInstance() = default;

    ClusterId GetClusterId() const override { return GeneralDiagnostics::Id; }

    ActionReturnStatus ReadAttribute(const ReadAttributeRequest & request, AttributeValueEncoder & encoder) override;
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, DataModel::ListBuilder<AttributeEntry> & builder) override;
    std::optional<ActionReturnStatus> InvokeCommand(const InvokeRequest & request, chip::TLV::TLVReader & input_arguments,
                                                    CommandHandler * handler) override;
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<AcceptedCommandEntry> & builder) override;
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<CommandId> & builder) override;

private:
    template <typename T>
    CHIP_ERROR ReadIfSupported(CHIP_ERROR (DiagnosticDataProvider::*getter)(T &), AttributeValueEncoder & aEncoder);

    template <typename T>
    CHIP_ERROR ReadListIfSupported(CHIP_ERROR (DiagnosticDataProvider::*getter)(T &), AttributeValueEncoder & aEncoder);

    CHIP_ERROR ReadNetworkInterfaces(AttributeValueEncoder & aEncoder);

    std::optional<ActionReturnStatus> HandleTestEventTrigger(chip::TLV::TLVReader & input);
    std::optional<ActionReturnStatus> HandleTimeSnapshot(CommandHandler * handler, const ConcreteCommandPath & requestPath,
                                                         chip::TLV::TLVReader & input);

    template <typename RequestT, typename FuncT>
    std::optional<ActionReturnStatus> HandleCommand(chip::TLV::TLVReader & input_arguments, FuncT func)
    {
        RequestT requestPayload;

        CHIP_ERROR err = DataModel::Decode(input_arguments, requestPayload);
        if (err != CHIP_NO_ERROR)
        {
            return ActionReturnStatus{ err };
        }

        return func(requestPayload);
    }

#ifdef GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD
    std::optional<ActionReturnStatus> HandlePayloadTestRequest(

        CommandHandler * handler, const ConcreteCommandPath & requestPath,
        const Commands::PayloadTestRequest::DecodableType & commandData);
#endif

    // Gets called when any network interface on the Node is updated.
    void OnNetworkInfoChanged() override
    {
        ChipLogDetail(Zcl, "GeneralDiagnosticsDelegate: OnNetworkInfoChanged");

        ReportAttributeOnAllEndpoints(GeneralDiagnostics::Attributes::NetworkInterfaces::Id);
    }
};

CHIP_ERROR GeneralDiagosticsGlobalInstance::Attributes(const ConcreteClusterPath & path,
                                                       DataModel::ListBuilder<AttributeEntry> & builder)
{
    static constexpr AttributeEntry kAttributes[] = {
        { NetworkInterfaces::Id, AttributeQualityFlags::kListAttribute, Access::Privilege::kView },
        { RebootCount::Id, {}, Access::Privilege::kView },
        { UpTime::Id, {}, Access::Privilege::kView },
        { TotalOperationalHours::Id, {}, Access::Privilege::kView },
        { BootReason::Id, {}, Access::Privilege::kView },
        { ActiveHardwareFaults::Id, AttributeQualityFlags::kListAttribute, Access::Privilege::kView },
        { ActiveRadioFaults::Id, AttributeQualityFlags::kListAttribute, Access::Privilege::kView },
        { ActiveNetworkFaults::Id, AttributeQualityFlags::kListAttribute, Access::Privilege::kView },
        { TestEventTriggersEnabled::Id, {}, Access::Privilege::kView },
    };

    ReturnErrorOnFailure(builder.ReferenceExisting(Span<const AttributeEntry>(kAttributes)));
    return builder.AppendElements(GetGlobalAttributes());
}

ActionReturnStatus GeneralDiagosticsGlobalInstance::ReadAttribute(const ReadAttributeRequest & aRequest,
                                                                  AttributeValueEncoder & aEncoder)
{
    switch (aRequest.path.mAttributeId)
    {
    case NetworkInterfaces::Id: {
        return ReadNetworkInterfaces(aEncoder);
    }
    case ActiveHardwareFaults::Id: {
        return ReadListIfSupported(&DiagnosticDataProvider::GetActiveHardwareFaults, aEncoder);
    }
    case ActiveRadioFaults::Id: {
        return ReadListIfSupported(&DiagnosticDataProvider::GetActiveRadioFaults, aEncoder);
    }
    case ActiveNetworkFaults::Id: {
        return ReadListIfSupported(&DiagnosticDataProvider::GetActiveNetworkFaults, aEncoder);
    }
    case RebootCount::Id: {
        uint16_t rebootCount = 0;
        CHIP_ERROR err       = GetDiagnosticDataProvider().GetRebootCount(rebootCount);
        if ((err != CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE) && (err != CHIP_NO_ERROR))
        {
            return err;
        }
        return aEncoder.Encode(rebootCount);
    }
    case UpTime::Id: {
        System::Clock::Seconds64 system_time_seconds =
            std::chrono::duration_cast<System::Clock::Seconds64>(Server::GetInstance().TimeSinceInit());
        return aEncoder.Encode(static_cast<uint64_t>(system_time_seconds.count()));
    }
    case TotalOperationalHours::Id: {
        uint32_t operationalHours = 0;
        CHIP_ERROR err            = GetDiagnosticDataProvider().GetTotalOperationalHours(operationalHours);
        if ((err != CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE) && (err != CHIP_NO_ERROR))
        {
            return err;
        }
        return aEncoder.Encode(operationalHours);
    }
    case BootReason::Id: {
        return ReadIfSupported(&DiagnosticDataProvider::GetBootReason, aEncoder);
    }
    case TestEventTriggersEnabled::Id: {
        bool isTestEventTriggersEnabled = IsTestEventTriggerEnabled();
        return aEncoder.Encode(isTestEventTriggersEnabled);
    }
        // Note: Attribute ID 0x0009 was removed (#30002).

    case FeatureMap::Id: {
        uint32_t features = 0;

#if CHIP_CONFIG_MAX_PATHS_PER_INVOKE > 1
        features |= to_underlying(Clusters::GeneralDiagnostics::Feature::kDataModelTest);
#endif // CHIP_CONFIG_MAX_PATHS_PER_INVOKE > 1

        return aEncoder.Encode(features);
    }

    case ClusterRevision::Id: {
        return aEncoder.Encode(kCurrentClusterRevision);
    }
    }
    return CHIP_NO_ERROR;
}

std::optional<ActionReturnStatus> GeneralDiagosticsGlobalInstance::InvokeCommand(const InvokeRequest & request,
                                                                                 chip::TLV::TLVReader & input_arguments,
                                                                                 CommandHandler * handler)
{
    const ConcreteCommandPath & requestPath = request.path;

    switch (requestPath.mCommandId)
    {
    case Commands::TestEventTrigger::Id:
        return HandleTestEventTrigger(input_arguments);
    case Commands::TimeSnapshot::Id:
        return HandleTimeSnapshot(handler, requestPath, input_arguments);
#ifdef GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD
    case Commands::PayloadTestRequest::Id:
        return HandleCommand<Commands::PayloadTestRequest::DecodableType>(
            input_arguments, [this, &requestPath, &handler](const auto & commandData) {
                return HandlePayloadTestRequest(handler, requestPath, commandData);
            });
#endif
    }

    return Status::InvalidCommand;
}

CHIP_ERROR GeneralDiagosticsGlobalInstance::AcceptedCommands(const ConcreteClusterPath & path,
                                                             DataModel::ListBuilder<AcceptedCommandEntry> & builder)
{
    static constexpr AcceptedCommandEntry kAcceptedCommands[] = {
        { Commands::TestEventTrigger::Id, {}, Access::Privilege::kManage },
        { Commands::TimeSnapshot::Id, {}, Access::Privilege::kOperate },
#ifdef GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD
        { Commands::PayloadTestRequest::Id, {}, Access::Privilege::kManage },
#endif
    };

    return builder.ReferenceExisting(Span<const AcceptedCommandEntry>(kAcceptedCommands));
}

CHIP_ERROR GeneralDiagosticsGlobalInstance::GeneratedCommands(const ConcreteClusterPath & path,
                                                              DataModel::ListBuilder<CommandId> & builder)
{
    static constexpr CommandId kGeneratedCommands[] = {
        Commands::TimeSnapshotResponse::Id,
#ifdef GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD
        Commands::PayloadTestResponse::Id,
#endif
    };

    return builder.ReferenceExisting(Span<const CommandId>(kGeneratedCommands));
}

template <typename T>
CHIP_ERROR GeneralDiagosticsGlobalInstance::ReadIfSupported(CHIP_ERROR (DiagnosticDataProvider::*getter)(T &),
                                                            AttributeValueEncoder & aEncoder)
{
    T data;
    CHIP_ERROR err = (GetDiagnosticDataProvider().*getter)(data);
    if (err == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
    {
        data = {};
    }
    else if (err != CHIP_NO_ERROR)
    {
        return err;
    }

    return aEncoder.Encode(data);
}

template <typename T>
CHIP_ERROR GeneralDiagosticsGlobalInstance::ReadListIfSupported(CHIP_ERROR (DiagnosticDataProvider::*getter)(T &),
                                                                AttributeValueEncoder & aEncoder)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    T faultList;

    if ((GetDiagnosticDataProvider().*getter)(faultList) == CHIP_NO_ERROR)
    {
        err = aEncoder.EncodeList([&faultList](const auto & encoder) -> CHIP_ERROR {
            for (auto fault : faultList)
            {
                ReturnErrorOnFailure(encoder.Encode(fault));
            }

            return CHIP_NO_ERROR;
        });
    }
    else
    {
        err = aEncoder.EncodeEmptyList();
    }

    return err;
}

CHIP_ERROR GeneralDiagosticsGlobalInstance::ReadNetworkInterfaces(AttributeValueEncoder & aEncoder)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    DeviceLayer::NetworkInterface * netifs;

    if (DeviceLayer::GetDiagnosticDataProvider().GetNetworkInterfaces(&netifs) == CHIP_NO_ERROR)
    {
        err = aEncoder.EncodeList([&netifs](const auto & encoder) -> CHIP_ERROR {
            for (DeviceLayer::NetworkInterface * ifp = netifs; ifp != nullptr; ifp = ifp->Next)
            {
                ReturnErrorOnFailure(encoder.Encode(*ifp));
            }

            return CHIP_NO_ERROR;
        });

        DeviceLayer::GetDiagnosticDataProvider().ReleaseNetworkInterfaces(netifs);
    }
    else
    {
        err = aEncoder.EncodeEmptyList();
    }

    return err;
}

std::optional<ActionReturnStatus> GeneralDiagosticsGlobalInstance::HandleTestEventTrigger(chip::TLV::TLVReader & input)
{
    Commands::TestEventTrigger::DecodableType commandData;

    ReturnErrorOnFailure(commandData.Decode(input));

    auto * triggerDelegate = GetTriggerDelegateOnMatchingKey(commandData.enableKey);
    if (triggerDelegate == nullptr)
    {
        return Status::ConstraintError;
    }

    CHIP_ERROR handleEventTriggerResult = triggerDelegate->HandleEventTriggers(commandData.eventTrigger);

    // When HandleEventTrigger fails, we simply convert any error to INVALID_COMMAND
    return (handleEventTriggerResult != CHIP_NO_ERROR) ? Status::InvalidCommand : Status::Success;
}

std::optional<ActionReturnStatus> GeneralDiagosticsGlobalInstance::HandleTimeSnapshot(CommandHandler * handler,
                                                                                      const ConcreteCommandPath & requestPath,
                                                                                      chip::TLV::TLVReader & input)
{
    ChipLogError(Zcl, "Received TimeSnapshot command!");

    Commands::TimeSnapshot::DecodableType commandData;
    ReturnErrorOnFailure(commandData.Decode(input));

    Commands::TimeSnapshotResponse::Type response;

    System::Clock::Microseconds64 posix_time_us{ 0 };

    // Only consider real time if time sync cluster is actually enabled. Avoids
    // likelihood of frequently reporting unsynced time.
#ifdef ZCL_USING_TIME_SYNCHRONIZATION_CLUSTER_SERVER
    CHIP_ERROR posix_time_err = System::SystemClock().GetClock_RealTime(posix_time_us);
    if (posix_time_err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Failed to get POSIX real time: %" CHIP_ERROR_FORMAT, posix_time_err.Format());
        posix_time_us = System::Clock::Microseconds64{ 0 };
    }
#endif // ZCL_USING_TIME_SYNCHRONIZATION_CLUSTER_SERVER

    System::Clock::Milliseconds64 system_time_ms =
        std::chrono::duration_cast<System::Clock::Milliseconds64>(Server::GetInstance().TimeSinceInit());

    response.systemTimeMs = static_cast<uint64_t>(system_time_ms.count());
    if (posix_time_us.count() != 0)
    {
        response.posixTimeMs.SetNonNull(
            static_cast<uint64_t>(std::chrono::duration_cast<System::Clock::Milliseconds64>(posix_time_us).count()));
    }
    handler->AddResponse(requestPath, response);
    return std::nullopt;
}

#ifdef GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD
std::optional<ActionReturnStatus>
GeneralDiagosticsGlobalInstance::HandlePayloadTestRequest(CommandHandler * handler, const ConcreteCommandPath & requestPath,
                                                          const Commands::PayloadTestRequest::DecodableType & commandData)
{
    // Max allowed is 2048.
    VerifyOrReturnError(commandData.count <= 2048, Status::ConstraintError);

    // Ensure Test Event triggers are enabled and key matches.
    auto * triggerDelegate = GetTriggerDelegateOnMatchingKey(commandData.enableKey);
    VerifyOrReturnError(triggerDelegate != nullptr, Status::ConstraintError);

    Commands::PayloadTestResponse::Type response;
    Platform::ScopedMemoryBufferWithSize<uint8_t> payload;
    if (!payload.Calloc(commandData.count))
    {
        return Status::ResourceExhausted;
    }

    memset(payload.Get(), commandData.value, payload.AllocatedSize());
    response.payload = ByteSpan{ payload.Get(), payload.AllocatedSize() };

    handler->AddResponse(requestPath, response);
    return std::nullopt;
}
#endif // GENERAL_DIAGNOSTICS_ENABLE_PAYLOAD_TEST_REQUEST_CMD

GeneralDiagosticsGlobalInstance gGeneralDiagosticsInstance;

} // anonymous namespace

namespace chip {
namespace app {
namespace Clusters {

GeneralDiagnosticsServer GeneralDiagnosticsServer::instance;

/**********************************************************
 * GeneralDiagnosticsServer Implementation
 *********************************************************/

GeneralDiagnosticsServer & GeneralDiagnosticsServer::Instance()
{
    return instance;
}

// Gets called when the device has been rebooted.
void GeneralDiagnosticsServer::OnDeviceReboot(BootReasonEnum bootReason)
{
    ChipLogDetail(Zcl, "GeneralDiagnostics: OnDeviceReboot");

    ReportAttributeOnAllEndpoints(GeneralDiagnostics::Attributes::BootReason::Id);

    // GeneralDiagnostics cluster should exist only for endpoint 0.
    if (emberAfContainsServer(0, GeneralDiagnostics::Id))
    {
        Events::BootReason::Type event{ bootReason };
        EventNumber eventNumber;

        CHIP_ERROR err = LogEvent(event, 0, eventNumber);
        if (CHIP_NO_ERROR != err)
        {
            ChipLogError(Zcl, "GeneralDiagnostics: Failed to record BootReason event: %" CHIP_ERROR_FORMAT, err.Format());
        }
    }
}

// Get called when the Node detects a hardware fault has been raised.
void GeneralDiagnosticsServer::OnHardwareFaultsDetect(const GeneralFaults<kMaxHardwareFaults> & previous,
                                                      const GeneralFaults<kMaxHardwareFaults> & current)
{
    ChipLogDetail(Zcl, "GeneralDiagnostics: OnHardwareFaultsDetect");

    for (auto endpointId : EnabledEndpointsWithServerCluster(GeneralDiagnostics::Id))
    {
        // If General Diagnostics cluster is implemented on this endpoint
        MatterReportingAttributeChangeCallback(endpointId, GeneralDiagnostics::Id,
                                               GeneralDiagnostics::Attributes::ActiveHardwareFaults::Id);

        // Record HardwareFault event
        EventNumber eventNumber;
        DataModel::List<const HardwareFaultEnum> currentList(reinterpret_cast<const HardwareFaultEnum *>(current.data()),
                                                             current.size());
        DataModel::List<const HardwareFaultEnum> previousList(reinterpret_cast<const HardwareFaultEnum *>(previous.data()),
                                                              previous.size());
        Events::HardwareFaultChange::Type event{ currentList, previousList };

        if (CHIP_NO_ERROR != LogEvent(event, endpointId, eventNumber))
        {
            ChipLogError(Zcl, "GeneralDiagnostics: Failed to record HardwareFault event");
        }
    }
}

// Get called when the Node detects a radio fault has been raised.
void GeneralDiagnosticsServer::OnRadioFaultsDetect(const GeneralFaults<kMaxRadioFaults> & previous,
                                                   const GeneralFaults<kMaxRadioFaults> & current)
{
    ChipLogDetail(Zcl, "GeneralDiagnostics: OnRadioFaultsDetect");

    for (auto endpointId : EnabledEndpointsWithServerCluster(GeneralDiagnostics::Id))
    {
        // If General Diagnostics cluster is implemented on this endpoint
        MatterReportingAttributeChangeCallback(endpointId, GeneralDiagnostics::Id,
                                               GeneralDiagnostics::Attributes::ActiveRadioFaults::Id);

        // Record RadioFault event
        EventNumber eventNumber;
        DataModel::List<const RadioFaultEnum> currentList(reinterpret_cast<const RadioFaultEnum *>(current.data()), current.size());
        DataModel::List<const RadioFaultEnum> previousList(reinterpret_cast<const RadioFaultEnum *>(previous.data()),
                                                           previous.size());
        Events::RadioFaultChange::Type event{ currentList, previousList };

        if (CHIP_NO_ERROR != LogEvent(event, endpointId, eventNumber))
        {
            ChipLogError(Zcl, "GeneralDiagnostics: Failed to record RadioFault event");
        }
    }
}

// Get called when the Node detects a network fault has been raised.
void GeneralDiagnosticsServer::OnNetworkFaultsDetect(const GeneralFaults<kMaxNetworkFaults> & previous,
                                                     const GeneralFaults<kMaxNetworkFaults> & current)
{
    ChipLogDetail(Zcl, "GeneralDiagnostics: OnNetworkFaultsDetect");

    for (auto endpointId : EnabledEndpointsWithServerCluster(GeneralDiagnostics::Id))
    {
        // If General Diagnostics cluster is implemented on this endpoint
        MatterReportingAttributeChangeCallback(endpointId, GeneralDiagnostics::Id,
                                               GeneralDiagnostics::Attributes::ActiveNetworkFaults::Id);

        // Record NetworkFault event
        EventNumber eventNumber;
        DataModel::List<const NetworkFaultEnum> currentList(reinterpret_cast<const NetworkFaultEnum *>(current.data()),
                                                            current.size());
        DataModel::List<const NetworkFaultEnum> previousList(reinterpret_cast<const NetworkFaultEnum *>(previous.data()),
                                                             previous.size());
        Events::NetworkFaultChange::Type event{ currentList, previousList };

        if (CHIP_NO_ERROR != LogEvent(event, endpointId, eventNumber))
        {
            ChipLogError(Zcl, "GeneralDiagnostics: Failed to record NetworkFault event");
        }
    }
}

} // namespace Clusters
} // namespace app
} // namespace chip

void MatterGeneralDiagnosticsPluginServerInitCallback()
{
    BootReasonEnum bootReason;

    static_assert(MATTER_DM_GENERAL_COMMISSIONING_CLUSTER_SERVER_ENDPOINT_COUNT == 1,
                  "Code expects general diagnostics on EP0 only");
    (void) ServerClusterInterfaceRegistry::Instance().Register(kRootEndpointId, &gGeneralDiagosticsInstance);

    ConnectivityMgr().SetDelegate(&gGeneralDiagosticsInstance);

    if (GetDiagnosticDataProvider().GetBootReason(bootReason) == CHIP_NO_ERROR)
    {
        GeneralDiagnosticsServer::Instance().OnDeviceReboot(bootReason);
    }
}
