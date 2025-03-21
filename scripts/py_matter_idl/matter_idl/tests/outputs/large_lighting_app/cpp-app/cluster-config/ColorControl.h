// DO NOT EDIT - Generated file
//
// Application configuration for ColorControl
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace ColorControl {

using FeatureBitmapType = BitFlags<Clusters::ColorControl::ColorControlFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kHueAndSaturation,
      FeatureBitmapType::kEnhancedHue,
      FeatureBitmapType::kColorLoop,
      FeatureBitmapType::kXy,
      FeatureBitmapType::kColorTemperature
    },
    .clusterRevision = 5,
  },
};

} // namespace ColorControl
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

