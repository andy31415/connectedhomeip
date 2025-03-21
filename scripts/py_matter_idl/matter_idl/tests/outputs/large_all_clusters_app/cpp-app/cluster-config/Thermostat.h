// DO NOT EDIT - Generated file
//
// Application configuration for Thermostat
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace Thermostat {

using FeatureBitmapType = BitFlags<Clusters::Thermostat::ThermostatFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kHeating,
      FeatureBitmapType::kCooling,
      FeatureBitmapType::kAutomode
    },
    .clusterRevision = 5,
  },
};

} // namespace Thermostat
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

