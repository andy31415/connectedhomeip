// DO NOT EDIT - Generated file
//
// Application configuration for KeypadInput
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace KeypadInput {

using FeatureBitmapType = BitFlags<Clusters::KeypadInput::KeypadInputFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
    },
    .clusterRevision = 1,
  },
};

} // namespace KeypadInput
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

