/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <device/types/camera/Camera.h>
#include <device/types/camera/impl/LoggingCamera.h>
#include <device/types/snapshot-camera/SnapshotCamera.h>
#include <device/types/snapshot-camera/impl/LoggingSnapshotCamera.h>
#include <lib/support/tests/ExtraPwTestMacros.h>
#include <pw_unit_test/framework.h>

#include <app/server-cluster/testing/ClusterTester.h>
#include <app/server-cluster/testing/FabricTestFixture.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <clusters/CameraAvStreamManagement/Attributes.h>
#include <clusters/CameraAvStreamManagement/Commands.h>
#include <clusters/Descriptor/ClusterId.h>
#include <clusters/WebRTCTransportProvider/Attributes.h>
#include <clusters/WebRTCTransportProvider/Commands.h>
#include <clusters/WebRTCTransportRequestor/ClusterId.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <devices/Types.h>
#include <platform/CHIPDeviceLayer.h>

#include <algorithm>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;
using namespace chip::app::Clusters::WebRTCTransportProvider;
using chip::Protocols::InteractionModel::Status;
using chip::Testing::ClusterTester;

namespace {

constexpr EndpointId kCameraEndpoint         = 1;
constexpr EndpointId kSecondCameraEndpoint   = 2;
constexpr EndpointId kSnapshotCameraEndpoint = 3;

class TestCameraDevice : public ::testing::Test
{
public:
    static void SetUpTestSuite() { ASSERT_SUCCESS(chip::Platform::MemoryInit()); }
    static void TearDownTestSuite() { chip::Platform::MemoryShutdown(); }

    void SetUp() override { ASSERT_SUCCESS(mFabricFixture.SetUpTestFabric(mTestFabricIndex)); }

    void TearDown() override { ASSERT_SUCCESS(mFabricFixture.TearDownTestFabric(mTestFabricIndex)); }

protected:
    Testing::TestServerClusterContext mContext;
    CodeDrivenDataModelProvider mProvider{ mContext.StorageDelegate(), mContext.AttributePersistenceProvider() };
    Testing::FabricTestFixture mFabricFixture{ &mContext.StorageDelegate() };
    FabricIndex mTestFabricIndex = kMinValidFabricIndex;
};

TEST_F(TestCameraDevice, CameraRegistrationDescriptorAndTeardown)
{
    LoggingCamera camera(LoggingCamera::Context{ .fabricTable = mFabricFixture.GetFabricTable() });

    ASSERT_SUCCESS(camera.Register(kCameraEndpoint, mProvider));
    EXPECT_EQ(camera.GetEndpointId(), kCameraEndpoint);

    ASSERT_SUCCESS(mProvider.Startup(mContext.ImContext()));

    ReadOnlyBufferBuilder<DataModel::EndpointEntry> endpoints;
    ASSERT_SUCCESS(mProvider.Endpoints(endpoints));
    auto endpointList = endpoints.TakeBuffer();
    ASSERT_EQ(endpointList.size(), 1U);
    EXPECT_EQ(endpointList[0].id, kCameraEndpoint);

    ReadOnlyBufferBuilder<DataModel::DeviceTypeEntry> deviceTypes;
    ASSERT_SUCCESS(mProvider.DeviceTypes(kCameraEndpoint, deviceTypes));
    auto deviceTypeList = deviceTypes.TakeBuffer();
    ASSERT_EQ(deviceTypeList.size(), 1U);
    EXPECT_EQ(deviceTypeList[0].deviceTypeId, Device::Type::kCamera.deviceTypeId);
    EXPECT_EQ(deviceTypeList[0].deviceTypeRevision, Device::Type::kCamera.deviceTypeRevision);

    ReadOnlyBufferBuilder<DataModel::ServerClusterEntry> serverClusters;
    ASSERT_SUCCESS(mProvider.ServerClusters(kCameraEndpoint, serverClusters));
    auto serverClusterList = serverClusters.TakeBuffer();
    auto hasServerCluster  = [&](ClusterId id) {
        return std::any_of(serverClusterList.begin(), serverClusterList.end(),
                           [id](const DataModel::ServerClusterEntry & entry) { return entry.clusterId == id; });
    };
    EXPECT_TRUE(hasServerCluster(Descriptor::Id));
    EXPECT_TRUE(hasServerCluster(CameraAvStreamManagement::Id));
    EXPECT_TRUE(hasServerCluster(WebRTCTransportProvider::Id));

    ReadOnlyBufferBuilder<ClusterId> clientClusters;
    ASSERT_SUCCESS(mProvider.ClientClusters(kCameraEndpoint, clientClusters));
    auto clientClusterList = clientClusters.TakeBuffer();
    ASSERT_EQ(clientClusterList.size(), 1U);
    EXPECT_EQ(clientClusterList[0], WebRTCTransportRequestor::Id);

    // Clean teardown while provider is started, followed by re-registration on another endpoint.
    camera.Unregister(mProvider);
    EXPECT_EQ(camera.GetEndpointId(), kInvalidEndpointId);
    EXPECT_SUCCESS(mProvider.Shutdown());

    ASSERT_SUCCESS(camera.Register(kSecondCameraEndpoint, mProvider));
    EXPECT_EQ(camera.GetEndpointId(), kSecondCameraEndpoint);
    camera.Unregister(mProvider);
    EXPECT_EQ(camera.GetEndpointId(), kInvalidEndpointId);
}

TEST_F(TestCameraDevice, CameraAvsmAttributesAndStubWebRTC)
{
    LoggingCamera camera(LoggingCamera::Context{ .fabricTable = mFabricFixture.GetFabricTable() });
    ASSERT_SUCCESS(camera.Register(kCameraEndpoint, mProvider));
    ASSERT_SUCCESS(mProvider.Startup(mContext.ImContext()));

    // Verify AVSM attributes
    {
        ClusterTester avsmTester(camera.CameraAVStreamManagementCluster());
        CameraAvStreamManagement::Attributes::FeatureMap::TypeInfo::DecodableType featureMap = 0;
        ASSERT_EQ(avsmTester.ReadAttribute(CameraAvStreamManagement::Attributes::FeatureMap::Id, featureMap), CHIP_NO_ERROR);
        EXPECT_TRUE((featureMap & to_underlying(CameraAvStreamManagement::Feature::kVideo)) != 0);
        EXPECT_TRUE((featureMap & to_underlying(CameraAvStreamManagement::Feature::kAudio)) != 0);
        EXPECT_TRUE((featureMap & to_underlying(CameraAvStreamManagement::Feature::kSnapshot)) != 0);

        uint8_t maxConcurrentEncoders = 0;
        ASSERT_EQ(avsmTester.ReadAttribute(CameraAvStreamManagement::Attributes::MaxConcurrentEncoders::Id, maxConcurrentEncoders),
                  CHIP_NO_ERROR);
        EXPECT_GE(maxConcurrentEncoders, 1U);

        VideoSensorParamsStruct sensorParams;
        ASSERT_EQ(avsmTester.ReadAttribute(CameraAvStreamManagement::Attributes::VideoSensorParams::Id, sensorParams),
                  CHIP_NO_ERROR);
        EXPECT_GT(sensorParams.sensorWidth, 0U);
        EXPECT_GT(sensorParams.sensorHeight, 0U);

        // Allocate a snapshot stream and capture a dummy JPEG
        CameraAvStreamManagement::Commands::SnapshotStreamAllocate::Type allocCmd;
        allocCmd.imageCodec    = ImageCodecEnum::kJpeg;
        allocCmd.maxFrameRate  = 30;
        allocCmd.minResolution = { 320, 240 };
        allocCmd.maxResolution = { 640, 480 };
        allocCmd.quality       = 80;

        auto allocResult = avsmTester.Invoke(CameraAvStreamManagement::Commands::SnapshotStreamAllocate::Id, allocCmd);
        ASSERT_TRUE(allocResult.IsSuccess());

        CameraAvStreamManagement::Commands::CaptureSnapshot::Type captureCmd;
        captureCmd.snapshotStreamID    = DataModel::MakeNullable(allocResult.response->snapshotStreamID);
        captureCmd.requestedResolution = { 640, 480 };

        auto captureResult = avsmTester.Invoke(CameraAvStreamManagement::Commands::CaptureSnapshot::Id, captureCmd);
        ASSERT_TRUE(captureResult.IsSuccess());
        EXPECT_EQ(captureResult.response->imageCodec, ImageCodecEnum::kJpeg);
        ASSERT_GE(captureResult.response->data.size(), 2U);
        EXPECT_EQ(captureResult.response->data[0], 0xFF);
        EXPECT_EQ(captureResult.response->data[1], 0xD8);
    }

    // Verify WebRTC Transport Provider cluster presence and stub offer rejection
    {
        ClusterTester webrtcTester(camera.WebRTCTransportProviderCluster());
        WebRTCTransportProvider::Attributes::CurrentSessions::TypeInfo::DecodableType sessions;
        ASSERT_EQ(webrtcTester.ReadAttribute(WebRTCTransportProvider::Attributes::CurrentSessions::Id, sessions), CHIP_NO_ERROR);
        auto iter = sessions.begin();
        EXPECT_FALSE(iter.Next());
        EXPECT_SUCCESS(iter.GetStatus());

        WebRTCTransportProvider::Commands::SolicitOffer::Type solicitCmd;
        solicitCmd.streamUsage           = Globals::StreamUsageEnum::kLiveView;
        solicitCmd.originatingEndpointID = 1;
        solicitCmd.videoStreams.SetValue(DataModel::List<const uint16_t>());

        auto solicitResult = webrtcTester.Invoke(WebRTCTransportProvider::Commands::SolicitOffer::Id, solicitCmd);
        EXPECT_FALSE(solicitResult.IsSuccess());
    }

    camera.Unregister(mProvider);
    EXPECT_SUCCESS(mProvider.Shutdown());
}

TEST_F(TestCameraDevice, SnapshotCameraRegistrationAndCapture)
{
    LoggingSnapshotCamera snapshotCamera;

    ASSERT_SUCCESS(snapshotCamera.Register(kSnapshotCameraEndpoint, mProvider));
    EXPECT_EQ(snapshotCamera.GetEndpointId(), kSnapshotCameraEndpoint);
    ASSERT_SUCCESS(mProvider.Startup(mContext.ImContext()));

    ReadOnlyBufferBuilder<DataModel::DeviceTypeEntry> deviceTypes;
    ASSERT_SUCCESS(mProvider.DeviceTypes(kSnapshotCameraEndpoint, deviceTypes));
    auto deviceTypeList = deviceTypes.TakeBuffer();
    ASSERT_EQ(deviceTypeList.size(), 1U);
    EXPECT_EQ(deviceTypeList[0].deviceTypeId, Device::Type::kSnapshotCamera.deviceTypeId);
    EXPECT_EQ(deviceTypeList[0].deviceTypeRevision, Device::Type::kSnapshotCamera.deviceTypeRevision);

    ReadOnlyBufferBuilder<DataModel::ServerClusterEntry> serverClusters;
    ASSERT_SUCCESS(mProvider.ServerClusters(kSnapshotCameraEndpoint, serverClusters));
    auto serverClusterList = serverClusters.TakeBuffer();
    auto hasServerCluster  = [&](ClusterId id) {
        return std::any_of(serverClusterList.begin(), serverClusterList.end(),
                           [id](const DataModel::ServerClusterEntry & entry) { return entry.clusterId == id; });
    };
    EXPECT_TRUE(hasServerCluster(Descriptor::Id));
    EXPECT_TRUE(hasServerCluster(CameraAvStreamManagement::Id));
    EXPECT_FALSE(hasServerCluster(WebRTCTransportProvider::Id));

    ReadOnlyBufferBuilder<ClusterId> clientClusters;
    ASSERT_SUCCESS(mProvider.ClientClusters(kSnapshotCameraEndpoint, clientClusters));
    EXPECT_TRUE(clientClusters.TakeBuffer().empty());

    ClusterTester avsmTester(snapshotCamera.CameraAVStreamManagementCluster());
    CameraAvStreamManagement::Attributes::FeatureMap::TypeInfo::DecodableType featureMap = 0;
    ASSERT_EQ(avsmTester.ReadAttribute(CameraAvStreamManagement::Attributes::FeatureMap::Id, featureMap), CHIP_NO_ERROR);
    EXPECT_TRUE((featureMap & to_underlying(CameraAvStreamManagement::Feature::kSnapshot)) != 0);
    EXPECT_EQ(featureMap & to_underlying(CameraAvStreamManagement::Feature::kVideo), 0U);
    EXPECT_EQ(featureMap & to_underlying(CameraAvStreamManagement::Feature::kAudio), 0U);

    CameraAvStreamManagement::Commands::SnapshotStreamAllocate::Type allocCmd;
    allocCmd.imageCodec    = ImageCodecEnum::kJpeg;
    allocCmd.maxFrameRate  = 30;
    allocCmd.minResolution = { 320, 240 };
    allocCmd.maxResolution = { 640, 480 };
    allocCmd.quality       = 80;

    auto allocResult = avsmTester.Invoke(CameraAvStreamManagement::Commands::SnapshotStreamAllocate::Id, allocCmd);
    ASSERT_TRUE(allocResult.IsSuccess());

    CameraAvStreamManagement::Commands::CaptureSnapshot::Type captureCmd;
    captureCmd.snapshotStreamID.SetNull();
    captureCmd.requestedResolution = { 640, 480 };

    auto captureResult = avsmTester.Invoke(CameraAvStreamManagement::Commands::CaptureSnapshot::Id, captureCmd);
    ASSERT_TRUE(captureResult.IsSuccess());
    EXPECT_EQ(captureResult.response->imageCodec, ImageCodecEnum::kJpeg);
    ASSERT_GE(captureResult.response->data.size(), 2U);
    EXPECT_EQ(captureResult.response->data[0], 0xFF);
    EXPECT_EQ(captureResult.response->data[1], 0xD8);

    snapshotCamera.Unregister(mProvider);
    EXPECT_EQ(snapshotCamera.GetEndpointId(), kInvalidEndpointId);
    EXPECT_SUCCESS(mProvider.Shutdown());
}

} // namespace
