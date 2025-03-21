// DO NOT EDIT - Generated file
//
// Application configuration for ThreadNetworkDiagnostics
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace ThreadNetworkDiagnostics {

using FeatureBitmapType = BitFlags<Clusters::ThreadNetworkDiagnostics::ThreadNetworkDiagnosticsFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 0,
    .featureMap = {
      FeatureBitmapType::kPacketCounts,
      FeatureBitmapType::kErrorCounts,
      FeatureBitmapType::kMLECounts,
      FeatureBitmapType::kMACCounts
    },
    .clusterRevision = 1,
  },
};

} // namespace ThreadNetworkDiagnostics
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

