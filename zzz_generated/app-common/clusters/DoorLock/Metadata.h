// DO NOT EDIT MANUALLY - Generated file
//
// Cluster metadata information for cluster DoorLock (cluster code: 257/0x101)
// based on src/controller/data_model/controller-clusters.matter
#pragma once

#include <app/data-model-provider/MetadataTypes.h>
#include <lib/core/DataModelTypes.h>

#include <cstdint>

#include <clusters/DoorLock/Ids.h>

namespace chip {
namespace app {
namespace Clusters {
namespace DoorLock {

inline constexpr uint32_t kRevision = 7;

namespace Attributes {
namespace LockState {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::LockState::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace LockState
namespace LockType {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::LockType::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace LockType
namespace ActuatorEnabled {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::ActuatorEnabled::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ActuatorEnabled
namespace DoorState {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::DoorState::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace DoorState
namespace DoorOpenEvents {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::DoorOpenEvents::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace DoorOpenEvents
namespace DoorClosedEvents {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::DoorClosedEvents::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace DoorClosedEvents
namespace OpenPeriod {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::OpenPeriod::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace OpenPeriod
namespace NumberOfTotalUsersSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfTotalUsersSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfTotalUsersSupported
namespace NumberOfPINUsersSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfPINUsersSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfPINUsersSupported
namespace NumberOfRFIDUsersSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfRFIDUsersSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfRFIDUsersSupported
namespace NumberOfWeekDaySchedulesSupportedPerUser {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfWeekDaySchedulesSupportedPerUser::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfWeekDaySchedulesSupportedPerUser
namespace NumberOfYearDaySchedulesSupportedPerUser {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfYearDaySchedulesSupportedPerUser::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfYearDaySchedulesSupportedPerUser
namespace NumberOfHolidaySchedulesSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfHolidaySchedulesSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfHolidaySchedulesSupported
namespace MaxPINCodeLength {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::MaxPINCodeLength::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace MaxPINCodeLength
namespace MinPINCodeLength {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::MinPINCodeLength::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace MinPINCodeLength
namespace MaxRFIDCodeLength {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::MaxRFIDCodeLength::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace MaxRFIDCodeLength
namespace MinRFIDCodeLength {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::MinRFIDCodeLength::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace MinRFIDCodeLength
namespace CredentialRulesSupport {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::CredentialRulesSupport::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace CredentialRulesSupport
namespace NumberOfCredentialsSupportedPerUser {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfCredentialsSupportedPerUser::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfCredentialsSupportedPerUser
namespace Language {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::Language::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace Language
namespace LEDSettings {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::LEDSettings::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace LEDSettings
namespace AutoRelockTime {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AutoRelockTime::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace AutoRelockTime
namespace SoundVolume {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::SoundVolume::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace SoundVolume
namespace OperatingMode {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::OperatingMode::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace OperatingMode
namespace SupportedOperatingModes {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::SupportedOperatingModes::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace SupportedOperatingModes
namespace DefaultConfigurationRegister {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::DefaultConfigurationRegister::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace DefaultConfigurationRegister
namespace EnableLocalProgramming {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::EnableLocalProgramming::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace EnableLocalProgramming
namespace EnableOneTouchLocking {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::EnableOneTouchLocking::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace EnableOneTouchLocking
namespace EnableInsideStatusLED {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::EnableInsideStatusLED::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace EnableInsideStatusLED
namespace EnablePrivacyModeButton {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::EnablePrivacyModeButton::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace EnablePrivacyModeButton
namespace LocalProgrammingFeatures {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::LocalProgrammingFeatures::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace LocalProgrammingFeatures
namespace WrongCodeEntryLimit {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::WrongCodeEntryLimit::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace WrongCodeEntryLimit
namespace UserCodeTemporaryDisableTime {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::UserCodeTemporaryDisableTime::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace UserCodeTemporaryDisableTime
namespace SendPINOverTheAir {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::SendPINOverTheAir::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace SendPINOverTheAir
namespace RequirePINforRemoteOperation {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::RequirePINforRemoteOperation::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace RequirePINforRemoteOperation
namespace ExpiringUserTimeout {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::ExpiringUserTimeout::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace ExpiringUserTimeout
namespace AliroReaderVerificationKey {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroReaderVerificationKey::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroReaderVerificationKey
namespace AliroReaderGroupIdentifier {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroReaderGroupIdentifier::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroReaderGroupIdentifier
namespace AliroReaderGroupSubIdentifier {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroReaderGroupSubIdentifier::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroReaderGroupSubIdentifier
namespace AliroExpeditedTransactionSupportedProtocolVersions {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroExpeditedTransactionSupportedProtocolVersions::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{ DataModel::AttributeQualityFlags::kListAttribute },
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroExpeditedTransactionSupportedProtocolVersions
namespace AliroGroupResolvingKey {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroGroupResolvingKey::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroGroupResolvingKey
namespace AliroSupportedBLEUWBProtocolVersions {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroSupportedBLEUWBProtocolVersions::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{ DataModel::AttributeQualityFlags::kListAttribute },
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroSupportedBLEUWBProtocolVersions
namespace AliroBLEAdvertisingVersion {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::AliroBLEAdvertisingVersion::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kAdminister,
    .writePrivilege = std::nullopt,
};
} // namespace AliroBLEAdvertisingVersion
namespace NumberOfAliroCredentialIssuerKeysSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfAliroCredentialIssuerKeysSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfAliroCredentialIssuerKeysSupported
namespace NumberOfAliroEndpointKeysSupported {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = DoorLock::Attributes::NumberOfAliroEndpointKeysSupported::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace NumberOfAliroEndpointKeysSupported

} // namespace Attributes

namespace Commands {
namespace LockDoor {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::LockDoor::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kOperate,
};
} // namespace LockDoor
namespace UnlockDoor {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::UnlockDoor::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kOperate,
};
} // namespace UnlockDoor
namespace UnlockWithTimeout {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::UnlockWithTimeout::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kOperate,
};
} // namespace UnlockWithTimeout
namespace SetWeekDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetWeekDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetWeekDaySchedule
namespace GetWeekDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::GetWeekDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace GetWeekDaySchedule
namespace ClearWeekDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearWeekDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearWeekDaySchedule
namespace SetYearDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetYearDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetYearDaySchedule
namespace GetYearDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::GetYearDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace GetYearDaySchedule
namespace ClearYearDaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearYearDaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearYearDaySchedule
namespace SetHolidaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetHolidaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetHolidaySchedule
namespace GetHolidaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::GetHolidaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace GetHolidaySchedule
namespace ClearHolidaySchedule {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearHolidaySchedule::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearHolidaySchedule
namespace SetUser {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetUser::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetUser
namespace GetUser {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::GetUser::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace GetUser
namespace ClearUser {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearUser::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearUser
namespace SetCredential {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetCredential::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetCredential
namespace GetCredentialStatus {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::GetCredentialStatus::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace GetCredentialStatus
namespace ClearCredential {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearCredential::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearCredential
namespace UnboltDoor {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::UnboltDoor::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kOperate,
};
} // namespace UnboltDoor
namespace SetAliroReaderConfig {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::SetAliroReaderConfig::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace SetAliroReaderConfig
namespace ClearAliroReaderConfig {
inline constexpr DataModel::AcceptedCommandEntry kMetatadaEntry = {
    .commandId       = DoorLock::Commands::ClearAliroReaderConfig::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{ DataModel::CommandQualityFlags::kTimed },
    .invokePrivilege = Access::Privilege::kAdminister,
};
} // namespace ClearAliroReaderConfig

} // namespace Commands
} // namespace DoorLock
} // namespace Clusters
} // namespace app
} // namespace chip
