// DO NOT EDIT MANUALLY - Generated file
//
// Cluster metadata information for cluster MeterIdentification (cluster code: 2822/0xB06)
// based on src/controller/data_model/controller-clusters.matter
#pragma once

#include <app/data-model-provider/MetadataTypes.h>
#include <lib/core/DataModelTypes.h>

#include <cstdint>

#include <clusters/MeterIdentification/MeterIdentificationIds.h>

namespace chip {
namespace app {
namespace clusters {
namespace MeterIdentification {
namespace Metadata {

inline constexpr uint32_t kRevision = 1;

namespace Attributes {

inline constexpr DataModel::AttributeEntry kMeterTypeEntry = {
    .attributeId    = MeterIdentification::Attributes::MeterType::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
inline constexpr DataModel::AttributeEntry kPointOfDeliveryEntry = {
    .attributeId    = MeterIdentification::Attributes::PointOfDelivery::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
inline constexpr DataModel::AttributeEntry kMeterSerialNumberEntry = {
    .attributeId    = MeterIdentification::Attributes::MeterSerialNumber::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
inline constexpr DataModel::AttributeEntry kProtocolVersionEntry = {
    .attributeId    = MeterIdentification::Attributes::ProtocolVersion::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};
inline constexpr DataModel::AttributeEntry kPowerThresholdEntry = {
    .attributeId    = MeterIdentification::Attributes::PowerThreshold::Id,
    .flags          = BitFlags<DataModel::AttributeQualityFlags>{},
    .readPrivilege  = Access::Privilege::kView,
    .writePrivilege = std::nullopt,
};

} // namespace Attributes

namespace Commands {} // namespace Commands
} // namespace Metadata
} // namespace MeterIdentification
} // namespace clusters
} // namespace app
} // namespace chip
