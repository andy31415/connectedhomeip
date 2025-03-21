// DO NOT EDIT - Generated file
//
// Application configuration for UnitLocalization
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace UnitLocalization {

using FeatureBitmapType = BitFlags<Clusters::UnitLocalization::UnitLocalizationFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      FeatureBitmapType::kTemperatureUnit
    },
    .clusterRevision = 1,
  },
};

} // namespace UnitLocalization
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

