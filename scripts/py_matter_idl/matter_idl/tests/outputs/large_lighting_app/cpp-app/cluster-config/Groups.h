// DO NOT EDIT - Generated file
//
// Application configuration for Groups
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Groups {


using FeatureBitmapType = uint32_t;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      0x0
    },
    .clusterRevision = 4,
  },
  {
    .endpointNumber = 1,
    .featureMap = {
      0x0
    },
    .clusterRevision = 4,
  },
};

} // namespace Groups
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

