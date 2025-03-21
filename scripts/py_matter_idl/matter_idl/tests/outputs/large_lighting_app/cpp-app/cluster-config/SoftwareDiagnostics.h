// DO NOT EDIT - Generated file
//
// Application configuration for SoftwareDiagnostics
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace SoftwareDiagnostics {

using FeatureBitmapType = BitFlags<Clusters::SoftwareDiagnostics::SoftwareDiagnosticsFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      FeatureBitmapType::kWaterMarks
    },
    .clusterRevision = 1,
  },
};

} // namespace SoftwareDiagnostics
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

