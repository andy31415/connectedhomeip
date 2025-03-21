// DO NOT EDIT - Generated file
//
// Application configuration for LevelControl
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace LevelControl {

using FeatureBitmapType = BitFlags<Clusters::LevelControl::LevelControlFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kOnOff,
      FeatureBitmapType::kLighting
    },
    .clusterRevision = 5,
  },
};

} // namespace LevelControl
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

