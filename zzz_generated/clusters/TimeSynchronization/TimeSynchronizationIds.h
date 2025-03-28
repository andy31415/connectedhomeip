// DO NOT EDIT MANUALLY - Generated file
//
// Identifier constant values for cluster TimeSynchronization (cluster code: 56/0x38)
// based on src/controller/data_model/controller-clusters.matter

#include <app/common/GlobalIds.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {
namespace Clusters {
namespace TimeSynchronization {

inline constexpr ClusterId kClusterId = 0x00000038;

namespace Attributes {
namespace UTCTime {
inline constexpr AttributeId Id = 0x00000000;
} // namespace UTCTime
namespace Granularity {
inline constexpr AttributeId Id = 0x00000001;
} // namespace Granularity
namespace TimeSource {
inline constexpr AttributeId Id = 0x00000002;
} // namespace TimeSource
namespace TrustedTimeSource {
inline constexpr AttributeId Id = 0x00000003;
} // namespace TrustedTimeSource
namespace DefaultNTP {
inline constexpr AttributeId Id = 0x00000004;
} // namespace DefaultNTP
namespace TimeZone {
inline constexpr AttributeId Id = 0x00000005;
} // namespace TimeZone
namespace DSTOffset {
inline constexpr AttributeId Id = 0x00000006;
} // namespace DSTOffset
namespace LocalTime {
inline constexpr AttributeId Id = 0x00000007;
} // namespace LocalTime
namespace TimeZoneDatabase {
inline constexpr AttributeId Id = 0x00000008;
} // namespace TimeZoneDatabase
namespace NTPServerAvailable {
inline constexpr AttributeId Id = 0x00000009;
} // namespace NTPServerAvailable
namespace TimeZoneListMaxSize {
inline constexpr AttributeId Id = 0x0000000A;
} // namespace TimeZoneListMaxSize
namespace DSTOffsetListMaxSize {
inline constexpr AttributeId Id = 0x0000000B;
} // namespace DSTOffsetListMaxSize
namespace SupportsDNSResolve {
inline constexpr AttributeId Id = 0x0000000C;
} // namespace SupportsDNSResolve
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
namespace SetUTCTime {
inline constexpr CommandId Id = 0x00000000;
} // namespace SetUTCTime
namespace SetTrustedTimeSource {
inline constexpr CommandId Id = 0x00000001;
} // namespace SetTrustedTimeSource
namespace SetTimeZone {
inline constexpr CommandId Id = 0x00000002;
} // namespace SetTimeZone
namespace SetDSTOffset {
inline constexpr CommandId Id = 0x00000004;
} // namespace SetDSTOffset
namespace SetDefaultNTP {
inline constexpr CommandId Id = 0x00000005;
} // namespace SetDefaultNTP
} // namespace Commands

namespace Events {
namespace DSTTableEmpty {
inline constexpr EventId Id = 0x00000000;
} // namespace DSTTableEmpty
namespace DSTStatus {
inline constexpr EventId Id = 0x00000001;
} // namespace DSTStatus
namespace TimeZoneStatus {
inline constexpr EventId Id = 0x00000002;
} // namespace TimeZoneStatus
namespace TimeFailure {
inline constexpr EventId Id = 0x00000003;
} // namespace TimeFailure
namespace MissingTrustedTimeSource {
inline constexpr EventId Id = 0x00000004;
} // namespace MissingTrustedTimeSource
} // namespace Events

} // namespace TimeSynchronization
} // namespace Clusters
} // namespace app
} // namespace chip
