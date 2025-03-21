// DO NOT EDIT - Generated file
//
// Application configuration for Channel
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Channel {

using FeatureBitmapType = BitFlags<Clusters::Channel::ChannelFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
    },
    .clusterRevision = 1,
  },
};

} // namespace Channel
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

