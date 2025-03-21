// DO NOT EDIT - Generated file
//
// Application configuration for WiFiNetworkDiagnostics
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace WiFiNetworkDiagnostics {


using FeatureBitmapType = uint32_t;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      0x3
    },
    .clusterRevision = 1,
  },
};

} // namespace WiFiNetworkDiagnostics
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

