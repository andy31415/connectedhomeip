// DO NOT EDIT MANUALLY - Generated file
//
// Identifier constant values for cluster CommodityPrice (cluster code: 149/0x95)
// based on src/controller/data_model/controller-clusters.matter
#pragma once

#include <app/common/GlobalIds.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {
namespace Clusters {
namespace CommodityPrice {

inline constexpr ClusterId Id = 0x00000095;

namespace Attributes {
namespace TariffUnit {
inline constexpr AttributeId Id = 0x00000000;
} // namespace TariffUnit
namespace Currency {
inline constexpr AttributeId Id = 0x00000001;
} // namespace Currency
namespace CurrentPrice {
inline constexpr AttributeId Id = 0x00000002;
} // namespace CurrentPrice
namespace PriceForecast {
inline constexpr AttributeId Id = 0x00000003;
} // namespace PriceForecast
namespace GeneratedCommandList {
inline constexpr AttributeId Id = Globals::Attributes::GeneratedCommandList::Id;
} // namespace GeneratedCommandList
namespace AcceptedCommandList {
inline constexpr AttributeId Id = Globals::Attributes::AcceptedCommandList::Id;
} // namespace AcceptedCommandList
namespace EventList {
inline constexpr AttributeId Id = Globals::Attributes::EventList::Id;
} // namespace EventList
namespace AttributeList {
inline constexpr AttributeId Id = Globals::Attributes::AttributeList::Id;
} // namespace AttributeList
namespace FeatureMap {
inline constexpr AttributeId Id = Globals::Attributes::FeatureMap::Id;
} // namespace FeatureMap
namespace ClusterRevision {
inline constexpr AttributeId Id = Globals::Attributes::ClusterRevision::Id;
} // namespace ClusterRevision

} // namespace Attributes

namespace Commands {
namespace GetDetailedPriceRequest {
inline constexpr CommandId Id = 0x00000000;
} // namespace GetDetailedPriceRequest
namespace GetDetailedForecastRequest {
inline constexpr CommandId Id = 0x00000002;
} // namespace GetDetailedForecastRequest
namespace GetDetailedPriceResponse {
inline constexpr CommandId Id = 0x00000001;
} // namespace GetDetailedPriceResponse
namespace GetDetailedForecastResponse {
inline constexpr CommandId Id = 0x00000003;
} // namespace GetDetailedForecastResponse
} // namespace Commands

namespace Events {
namespace PriceChange {
inline constexpr EventId Id = 0x00000000;
} // namespace PriceChange
namespace ForecastChange {
inline constexpr EventId Id = 0x00000001;
} // namespace ForecastChange
} // namespace Events

} // namespace CommodityPrice
} // namespace Clusters
} // namespace app
} // namespace chip
