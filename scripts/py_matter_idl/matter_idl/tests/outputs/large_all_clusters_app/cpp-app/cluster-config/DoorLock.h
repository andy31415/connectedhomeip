// DO NOT EDIT - Generated file
//
// Application configuration for DoorLock
#pragma once

#include <app-common/zap-generated/cluster-enums.h>
#include <app/util/cluster-config.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {
namespace config {
namespace Clusters {
namespace DoorLock {

using FeatureBitmapType = BitFlags<Clusters::DoorLock::DoorLockFeature>;

inline constexpr ClusterEndpointConfiguration<FeatureBitmapType> kFixedEndpoints[] = {
  {
    .endpointNumber = 1,
    .featureMap = {
      FeatureBitmapType::kPINCredentials,
      FeatureBitmapType::kRFIDCredentials,
      FeatureBitmapType::kWeekDaySchedules,
      FeatureBitmapType::kUsersManagement,
      FeatureBitmapType::kYearDaySchedules,
      FeatureBitmapType::kHolidaySchedules
    },
    .clusterRevision = 6,
  },
};

} // namespace DoorLock
} // namespace Clusters
} // namespace config
} // namespace app
} // namespace namespace chip

