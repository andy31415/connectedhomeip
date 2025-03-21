// DO NOT EDIT - Generated file
//
// Application configuration for FanControl
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace FanControl {

using FeatureBitmapType = BitFlags<Clusters::FanControl::FanControlFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kMultiSpeed,
      FeatureBitmapType::kAuto,
      FeatureBitmapType::kRocking,
      FeatureBitmapType::kWind
    },
    .clusterRevision = 2,
  },
};

} // namespace FanControl
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

