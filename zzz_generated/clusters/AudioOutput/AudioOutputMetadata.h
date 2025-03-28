// DO NOT EDIT MANUALLY - Generated file
//
// Identifier constant values for cluster AudioOutput (cluster code: 1291/0x50B)
// based on src/controller/data_model/controller-clusters.matter

#include <app/common/GlobalIds.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {
namespace clusters {
namespace AudioOutput {

inline constexpr ClusterId kClusterId = 0x0000050B;

namespace Attributes {
namespace OutputList {
inline constexpr AttributeId Id = 0x00000000;
} // namespace OutputList
namespace CurrentOutput {
inline constexpr AttributeId Id = 0x00000001;
} // namespace CurrentOutput
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
namespace SelectOutput {
inline constexpr CommandId Id = 0x00000000;
} // namespace SelectOutput
namespace RenameOutput {
inline constexpr CommandId Id = 0x00000001;
} // namespace RenameOutput
} // namespace Commands

namespace Events {} // namespace Events

} // namespace AudioOutput
} // namespace clusters
} // namespace app
} // namespace chip
