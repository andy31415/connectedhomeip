// DO NOT EDIT - Generated file
//
// Application configuration for ModeSelect
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace ModeSelect {

using FeatureBitmapType = BitFlags<Clusters::ModeSelect::ModeSelectFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kDeponoff
    },
    .clusterRevision = 1,
  },
};

} // namespace ModeSelect
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

