// DO NOT EDIT - Generated file
//
// Application configuration for Third
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Third {


using FeatureBitmapType = uint32_t;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      0x0
    },
    .clusterRevision = 2,
  },
};

} // namespace Third
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

