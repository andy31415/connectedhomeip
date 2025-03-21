// DO NOT EDIT - Generated file
//
// Application configuration for NetworkCommissioning
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace NetworkCommissioning {

using FeatureBitmapType = BitFlags<Clusters::NetworkCommissioning::NetworkCommissioningFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      FeatureBitmapType::kThreadNetworkInterface
    },
    .clusterRevision = 1,
  },
};

} // namespace NetworkCommissioning
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

