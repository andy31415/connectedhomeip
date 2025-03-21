// DO NOT EDIT - Generated file
//
// Application configuration for ThermostatUserInterfaceConfiguration
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace ThermostatUserInterfaceConfiguration {


using FeatureBitmapType = uint32_t;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      0x0
    },
    .clusterRevision = 2,
  },
};

} // namespace ThermostatUserInterfaceConfiguration
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

