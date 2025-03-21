// DO NOT EDIT - Generated file
//
// Application configuration for Descriptor
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Descriptor {


using FeatureBitmapType = uint32_t;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      0x0
    },
    .clusterRevision = 1,
  },
  {
    .endpointNumber = 1,
    .featureMap = {
      0x0
    },
    .clusterRevision = 1,
  },
  {
    .endpointNumber = 2,
    .featureMap = {
      0x0
    },
    .clusterRevision = 1,
  },
  {
    .endpointNumber = 65534,
    .featureMap = {
      0x0
    },
    .clusterRevision = 1,
  },
};

} // namespace Descriptor
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

