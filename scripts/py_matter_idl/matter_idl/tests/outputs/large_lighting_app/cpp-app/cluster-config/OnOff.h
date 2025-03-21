// DO NOT EDIT - Generated file
//
// Application configuration for OnOff
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace OnOff {

using FeatureBitmapType = BitFlags<Clusters::OnOff::OnOffFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kLighting
    },
    .clusterRevision = 4,
  },
};

} // namespace OnOff
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

