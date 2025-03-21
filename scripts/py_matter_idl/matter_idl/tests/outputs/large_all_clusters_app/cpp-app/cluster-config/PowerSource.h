// DO NOT EDIT - Generated file
//
// Application configuration for PowerSource
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace PowerSource {

using FeatureBitmapType = BitFlags<Clusters::PowerSource::PowerSourceFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      FeatureBitmapType::kBattery
    },
    .clusterRevision = 1,
  },
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kBattery
    },
    .clusterRevision = 1,
  },
  {
    .endpointNumber = 2,
    .featureMap = {
      FeatureBitmapType::kBattery
    },
    .clusterRevision = 1,
  },
};

} // namespace PowerSource
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

