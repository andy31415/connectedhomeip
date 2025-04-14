// DO NOT EDIT MANUALLY - Generated file
//
// Cluster metadata information for cluster BasicInformation (cluster code: 40/0x28)
// based on src/controller/data_model/controller-clusters.matter
#pragma once

#include <app/data-model-provider/MetadataTypes.h>
#include <lib/core/DataModelTypes.h>

#include <cstdint>

#include <clusters/BasicInformation/Ids.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {

inline constexpr uint32_t kRevision = 3;

namespace Attributes {
namespace DataModelRevision {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::DataModelRevision::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace DataModelRevision
namespace VendorName {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::VendorName::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace VendorName
namespace VendorID {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::VendorID::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace VendorID
namespace ProductName {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ProductName::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ProductName
namespace ProductID {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ProductID::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ProductID
namespace NodeLabel {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::NodeLabel::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace NodeLabel
namespace Location {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::Location::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kAdminister,
};
} // namespace Location
namespace HardwareVersion {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::HardwareVersion::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace HardwareVersion
namespace HardwareVersionString {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::HardwareVersionString::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace HardwareVersionString
namespace SoftwareVersion {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::SoftwareVersion::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace SoftwareVersion
namespace SoftwareVersionString {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::SoftwareVersionString::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace SoftwareVersionString
namespace ManufacturingDate {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ManufacturingDate::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ManufacturingDate
namespace PartNumber {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::PartNumber::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace PartNumber
namespace ProductURL {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ProductURL::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ProductURL
namespace ProductLabel {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ProductLabel::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ProductLabel
namespace SerialNumber {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::SerialNumber::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace SerialNumber
namespace LocalConfigDisabled {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::LocalConfigDisabled::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = Access::Privilege::kManage,
};
} // namespace LocalConfigDisabled
namespace Reachable {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::Reachable::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace Reachable
namespace UniqueID {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::UniqueID::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace UniqueID
namespace CapabilityMinima {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::CapabilityMinima::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace CapabilityMinima
namespace ProductAppearance {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::ProductAppearance::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace ProductAppearance
namespace SpecificationVersion {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::SpecificationVersion::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace SpecificationVersion
namespace MaxPathsPerInvoke {
inline constexpr DataModel::AttributeEntry kMetadataEntry = {
    .attributeId    = BasicInformation::Attributes::MaxPathsPerInvoke::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
} // namespace MaxPathsPerInvoke

} // namespace Attributes

namespace Commands {
namespace MfgSpecificPing {
inline constexpr DataModel::AcceptedCommandEntry kMetadataEntry = {
    .commandId       = BasicInformation::Commands::MfgSpecificPing::Id,
    .flags           = BitFlags<DataModel::CommandQualityFlags>{},
    .invokePrivilege = Access::Privilege::kOperate,
};
} // namespace MfgSpecificPing

} // namespace Commands
} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
