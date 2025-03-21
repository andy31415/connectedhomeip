// DO NOT EDIT - Generated file
//
// Application configuration for WindowCovering
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace WindowCovering {

using FeatureBitmapType = BitFlags<Clusters::WindowCovering::Feature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kLift,
      FeatureBitmapType::kTilt,
      FeatureBitmapType::kPositionAwareLift,
      FeatureBitmapType::kPositionAwareTilt
    },
    .clusterRevision = 5,
  },
};

} // namespace WindowCovering
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

