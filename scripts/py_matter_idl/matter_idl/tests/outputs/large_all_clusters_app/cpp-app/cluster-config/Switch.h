// DO NOT EDIT - Generated file
//
// Application configuration for Switch
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Switch {

using FeatureBitmapType = BitFlags<Clusters::Switch::SwitchFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kLatchingSwitch
    },
    .clusterRevision = 1,
  },
};

} // namespace Switch
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

