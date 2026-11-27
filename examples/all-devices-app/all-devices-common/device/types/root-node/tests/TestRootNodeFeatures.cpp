/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
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

#include <device/types/root-node/RootNodeWith.h>
#include <device/types/root-node/features/PowerSourceFeature.h>
#include <device/types/root-node/features/TimeSynchronizationFeature.h>
#include <device/types/root-node/features/TlsFeature.h>
#include <pw_unit_test/framework.h>

#include <access/AccessControl.h>
#include <app/AppConfig.h>
#include <app/CASEClientPool.h>
#include <app/CASESessionManager.h>
#include <app/DeviceLoadStatusProvider.h>
#include <app/EventManagement.h>
#include <app/FailSafeContext.h>
#include <app/InteractionModelEngine.h>
#include <app/OperationalSessionSetupPool.h>
#include <app/data-model-provider/tests/ReadTesting.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Dnssd.h>
#if CHIP_CONFIG_TERMS_AND_CONDITIONS_REQUIRED
#include <app/server/TermsAndConditionsManager.h> // nogncheck
#endif                                            // CHIP_CONFIG_TERMS_AND_CONDITIONS_REQUIRED
#include <clusters/PowerSource/AttributeIds.h>
#include <clusters/PowerSource/Attributes.h>
#include <clusters/PowerSource/ClusterId.h>
#include <clusters/PowerSource/Enums.h>
#include <clusters/TimeSynchronization/AttributeIds.h>
#include <clusters/TimeSynchronization/Attributes.h>
#include <clusters/TimeSynchronization/ClusterId.h>
#include <clusters/TimeSynchronization/Enums.h>
#include <clusters/TlsCertificateManagement/AttributeIds.h>
#include <clusters/TlsCertificateManagement/Attributes.h>
#include <clusters/TlsCertificateManagement/ClusterId.h>
#include <clusters/TlsCertificateManagement/Commands.h>
#include <clusters/TlsClientManagement/AttributeIds.h>
#include <clusters/TlsClientManagement/Attributes.h>
#include <clusters/TlsClientManagement/ClusterId.h>
#include <clusters/TlsClientManagement/Commands.h>
#include <credentials/GroupDataProviderImpl.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/TimerDelegateMock.h>
#include <lib/support/tests/ExtraPwTestMacros.h>
#include <messaging/tests/MessagingContext.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/DiagnosticDataProvider.h>
#include <protocols/interaction_model/StatusCode.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

namespace {

CHIP_ERROR CopyTestString(char * buf, size_t bufSize, const char * source)
{
    VerifyOrReturnError(strlen(source) < bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    Platform::CopyString(buf, bufSize, source);
    return CHIP_NO_ERROR;
}

class StubDeviceInstanceInfoProvider : public DeviceLayer::DeviceInstanceInfoProvider
{
public:
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "TestVendor"); }
    CHIP_ERROR GetVendorId(uint16_t & vendorId) override
    {
        vendorId = 0xFFF1;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "TestProduct"); }
    CHIP_ERROR GetProductId(uint16_t & productId) override
    {
        productId = 0x8001;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "PART1"); }
    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "https://example.com"); }
    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "TestLabel"); }
    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "SN12345"); }
    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override
    {
        year  = 2026;
        month = 1;
        day   = 1;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override
    {
        hardwareVersion = 1;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override { return CopyTestString(buf, bufSize, "1.0"); }
    CHIP_ERROR GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan) override
    {
        static constexpr uint8_t kUniqueId[16] = { 1 };
        return CopySpanToMutableSpan(ByteSpan(kUniqueId), uniqueIdSpan);
    }
};

class StubTimeSyncDelegate : public TimeSynchronization::DefaultTimeSyncDelegate
{
public:
    CHIP_ERROR UpdateTimeFromPlatformSource(Callback::Callback<TimeSynchronization::OnTimeSyncCompletion> * callback) override
    {
        ++mUpdateCalls;
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    size_t mUpdateCalls = 0;
};

class ProviderAttributeReader
{
public:
    explicit ProviderAttributeReader(CodeDrivenDataModelProvider & provider) : mProvider(provider) {}

    template <typename T>
    CHIP_ERROR Read(EndpointId endpoint, ClusterId cluster, AttributeId attribute, T & value,
                    FabricIndex accessingFabric = kUndefinedFabricIndex)
    {
        mOperations.push_back(std::make_unique<Testing::ReadOperation>(endpoint, cluster, attribute));
        Testing::ReadOperation & operation = *mOperations.back();
        if (accessingFabric != kUndefinedFabricIndex)
        {
            operation.SetSubjectDescriptor(Access::SubjectDescriptor{ .fabricIndex = accessingFabric });
        }

        std::unique_ptr<AttributeValueEncoder> encoder = operation.StartEncoding();
        VerifyOrReturnError(encoder != nullptr, CHIP_ERROR_NO_MEMORY);

        ReturnErrorOnFailure(mProvider.ReadAttribute(operation.GetRequest(), *encoder).GetUnderlyingError());
        ReturnErrorOnFailure(operation.FinishEncoding());

        std::vector<Testing::DecodedAttributeData> attributeData;
        ReturnErrorOnFailure(operation.GetEncodedIBs().Decode(attributeData));
        VerifyOrReturnError(attributeData.size() == 1u, CHIP_ERROR_INCORRECT_STATE);
        return DataModel::Decode(attributeData[0].dataReader, value);
    }

private:
    CodeDrivenDataModelProvider & mProvider;
    std::vector<std::unique_ptr<Testing::ReadOperation>> mOperations;
};

class TestRootNodeFeatures : public Testing::LoopbackMessagingContext
{
public:
    void SetUp() override
    {
        LoopbackMessagingContext::SetUp();
        VerifyOrReturn(!HasFailure());

        mGroupDataProvider.SetStorageDelegate(&mTestContext.StorageDelegate());
        mGroupDataProvider.SetSessionKeystore(&mSessionKeystore);
        ASSERT_SUCCESS(mGroupDataProvider.Init());

        CASESessionManagerConfig caseConfig;
        caseConfig.sessionInitParams.sessionManager    = &GetSecureSessionManager();
        caseConfig.sessionInitParams.exchangeMgr       = &GetExchangeManager();
        caseConfig.sessionInitParams.fabricTable       = &GetFabricTable();
        caseConfig.sessionInitParams.groupDataProvider = &mGroupDataProvider;
        caseConfig.clientPool                          = &mCASEClientPool;
        caseConfig.sessionSetupPool                    = &mSessionSetupPool;
        ASSERT_SUCCESS(mCASESessionManager.Init(&GetSystemLayer(), caseConfig));

        mCommissioningWindowManager.Init({
            .fabricTable     = GetFabricTable(),
            .sessionManager  = GetSecureSessionManager(),
            .exchangeManager = GetExchangeManager(),
            .failSafeContext = mFailSafeContext,
#if CONFIG_NETWORK_LAYER_BLE
            .bleLayer = mBleLayer,
#endif // CONFIG_NETWORK_LAYER_BLE
        });
    }

    void TearDown() override
    {
        mCommissioningWindowManager.Shutdown();
        mCASESessionManager.Shutdown();
        mGroupDataProvider.Finish();
        LoopbackMessagingContext::TearDown();
    }

protected:
    RootNode::Context MakeRootNodeContext()
    {
        return RootNode::Context{
            .commissioningWindowManager          = mCommissioningWindowManager,
            .configurationManager                = DeviceLayer::ConfigurationMgr(),
            .deviceControlServer                 = DeviceLayer::DeviceControlServer::DeviceControlSvr(),
            .fabricTable                         = GetFabricTable(),
            .accessControl                       = mAccessControl,
            .persistentStorage                   = mTestContext.StorageDelegate(),
            .failSafeContext                     = mFailSafeContext,
            .deviceInstanceInfoProvider          = mDeviceInstanceInfoProvider,
            .platformManager                     = DeviceLayer::PlatformMgr(),
            .groupDataProvider                   = mGroupDataProvider,
            .sessionManager                      = GetSecureSessionManager(),
            .dnssdServer                         = DnssdServer::Instance(),
            .deviceLoadStatusProvider            = mTestContext.GetInteractionModelEngine(),
            .diagnosticDataProvider              = DeviceLayer::GetDiagnosticDataProvider(),
            .testEventTriggerDelegate            = nullptr,
            .dacProvider                         = *Credentials::Examples::GetExampleDACProvider(),
            .eventManagement                     = EventManagement::GetInstance(),
            .timerDelegate                       = mTimerDelegate,
            .minGuaranteedSubscriptionsPerFabric = 0,
#if CHIP_CONFIG_TERMS_AND_CONDITIONS_REQUIRED
            .termsAndConditionsProvider = TermsAndConditionsManager::GetInstance(),
#endif // CHIP_CONFIG_TERMS_AND_CONDITIONS_REQUIRED
        };
    }

    bool HasServerCluster(EndpointId endpoint, ClusterId clusterId)
    {
        ReadOnlyBufferBuilder<DataModel::ServerClusterEntry> builder;
        EXPECT_SUCCESS(mProvider.ServerClusters(endpoint, builder));
        const auto clusters = builder.TakeBuffer();
        return std::any_of(clusters.begin(), clusters.end(),
                           [clusterId](const auto & entry) { return entry.clusterId == clusterId; });
    }

    std::vector<DataModel::EndpointEntry> GetEndpoints()
    {
        ReadOnlyBufferBuilder<DataModel::EndpointEntry> builder;
        EXPECT_SUCCESS(mProvider.Endpoints(builder));
        const auto buffer = builder.TakeBuffer();
        return std::vector<DataModel::EndpointEntry>(buffer.begin(), buffer.end());
    }

    Testing::TestServerClusterContext mTestContext;
    CodeDrivenDataModelProvider mProvider{ mTestContext.StorageDelegate(), mTestContext.AttributePersistenceProvider() };
    TimerDelegateMock mTimerDelegate;
    StubDeviceInstanceInfoProvider mDeviceInstanceInfoProvider;
    Crypto::DefaultSessionKeystore mSessionKeystore;
    Credentials::GroupDataProviderImpl mGroupDataProvider;
    Access::AccessControl mAccessControl;
    FailSafeContext mFailSafeContext;
    CommissioningWindowManager mCommissioningWindowManager;
#if CONFIG_NETWORK_LAYER_BLE
    Ble::BleLayer mBleLayer;
#endif // CONFIG_NETWORK_LAYER_BLE
    CASEClientPool<2> mCASEClientPool;
    OperationalSessionSetupPool<2> mSessionSetupPool;
    CASESessionManager mCASESessionManager;
};

using CameraRootNode = RootNodeWith<PowerSourceFeature, TimeSynchronizationFeature, TlsFeature>;

TEST_F(TestRootNodeFeatures, RegistersAndReadsAllRootNodeFeatures)
{
    StubTimeSyncDelegate timeSyncDelegate;
    CameraRootNode rootNode(MakeRootNodeContext(),
                            PowerSourceFeature::Context{
                                .enabled          = true,
                                .description      = "AC Mains"_span,
                                .wiredCurrentType = PowerSource::WiredCurrentTypeEnum::kAc,
                                .order            = 1,
                            },
                            TimeSynchronizationFeature::Context{
                                .enabled                = true,
                                .caseSessionManager     = mCASESessionManager,
                                .interactionModelEngine = mTestContext.GetInteractionModelEngine(),
                                .delegate               = &timeSyncDelegate,
                            },
                            TlsFeature::Context{
                                .enabled        = true,
                                .maxRootCerts   = 6,
                                .maxClientCerts = 7,
                                .maxEndpoints   = 8,
                            });

    ASSERT_SUCCESS(rootNode.Register(kRootEndpointId, mProvider));
    ASSERT_SUCCESS(mProvider.Startup(mTestContext.ImContext()));

    EXPECT_TRUE(HasServerCluster(kRootEndpointId, PowerSource::Id));
    EXPECT_TRUE(HasServerCluster(kRootEndpointId, TimeSynchronization::Id));
    EXPECT_TRUE(HasServerCluster(kRootEndpointId, TlsCertificateManagement::Id));
    EXPECT_TRUE(HasServerCluster(kRootEndpointId, TlsClientManagement::Id));

    ProviderAttributeReader reader(mProvider);

    // PowerSource attributes
    PowerSource::PowerSourceStatusEnum psStatus = PowerSource::PowerSourceStatusEnum::kUnspecified;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::Status::Id, psStatus));
    EXPECT_EQ(psStatus, PowerSource::PowerSourceStatusEnum::kActive);

    uint8_t psOrder = 0;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::Order::Id, psOrder));
    EXPECT_EQ(psOrder, 1u);

    CharSpan psDescription;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::Description::Id, psDescription));
    EXPECT_TRUE(psDescription.data_equal("AC Mains"_span));

    PowerSource::WiredCurrentTypeEnum currentType = PowerSource::WiredCurrentTypeEnum::kDc;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::WiredCurrentType::Id, currentType));
    EXPECT_EQ(currentType, PowerSource::WiredCurrentTypeEnum::kAc);

    PowerSource::Attributes::EndpointList::TypeInfo::DecodableType endpointList;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::EndpointList::Id, endpointList));
    auto epIter = endpointList.begin();
    ASSERT_TRUE(epIter.Next());
    EXPECT_EQ(epIter.GetValue(), kRootEndpointId);
    EXPECT_FALSE(epIter.Next());
    EXPECT_SUCCESS(epIter.GetStatus());

    uint32_t psFeatureMap = 0;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, PowerSource::Id, PowerSource::Attributes::FeatureMap::Id, psFeatureMap));
    EXPECT_EQ(psFeatureMap, to_underlying(PowerSource::Feature::kWired));

    // TimeSynchronization attributes
    TimeSynchronization::GranularityEnum granularity = TimeSynchronization::GranularityEnum::kSecondsGranularity;
    ASSERT_SUCCESS(
        reader.Read(kRootEndpointId, TimeSynchronization::Id, TimeSynchronization::Attributes::Granularity::Id, granularity));
    EXPECT_EQ(granularity, TimeSynchronization::GranularityEnum::kNoTimeGranularity);

    uint32_t tsFeatureMap = 0;
    ASSERT_SUCCESS(
        reader.Read(kRootEndpointId, TimeSynchronization::Id, TimeSynchronization::Attributes::FeatureMap::Id, tsFeatureMap));
    uint32_t expectedTsFeatures =
        to_underlying(TimeSynchronization::Feature::kTimeZone) | to_underlying(TimeSynchronization::Feature::kNTPClient);
#if TIME_SYNC_ENABLE_TSC_FEATURE
    expectedTsFeatures |= to_underlying(TimeSynchronization::Feature::kTimeSyncClient);
#endif // TIME_SYNC_ENABLE_TSC_FEATURE
    EXPECT_EQ(tsFeatureMap, expectedTsFeatures);
    EXPECT_EQ(rootNode.GetFeature<TimeSynchronizationFeature>().Cluster().GetDelegate(), &timeSyncDelegate);

    // TlsCertificateManagement & TlsClientManagement attributes
    uint8_t maxRootCerts = 0;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, TlsCertificateManagement::Id,
                               TlsCertificateManagement::Attributes::MaxRootCertificates::Id, maxRootCerts));
    EXPECT_EQ(maxRootCerts, 6u);

    uint8_t maxClientCerts = 0;
    ASSERT_SUCCESS(reader.Read(kRootEndpointId, TlsCertificateManagement::Id,
                               TlsCertificateManagement::Attributes::MaxClientCertificates::Id, maxClientCerts));
    EXPECT_EQ(maxClientCerts, 7u);

    uint8_t maxEndpoints = 0;
    ASSERT_SUCCESS(
        reader.Read(kRootEndpointId, TlsClientManagement::Id, TlsClientManagement::Attributes::MaxProvisioned::Id, maxEndpoints));
    EXPECT_EQ(maxEndpoints, 8u);

    rootNode.Unregister(mProvider);
    EXPECT_EQ(rootNode.GetEndpointId(), kInvalidEndpointId);
    EXPECT_TRUE(GetEndpoints().empty());
    EXPECT_SUCCESS(mProvider.Shutdown());
}

TEST_F(TestRootNodeFeatures, DisabledFeaturesAreNotRegistered)
{
    CameraRootNode rootNode(MakeRootNodeContext(),
                            PowerSourceFeature::Context{
                                .enabled          = false,
                                .description      = "Unused"_span,
                                .wiredCurrentType = PowerSource::WiredCurrentTypeEnum::kDc,
                            },
                            TimeSynchronizationFeature::Context{
                                .enabled                = false,
                                .caseSessionManager     = mCASESessionManager,
                                .interactionModelEngine = mTestContext.GetInteractionModelEngine(),
                            },
                            TlsFeature::Context{
                                .enabled = false,
                            });

    ASSERT_SUCCESS(rootNode.Register(kRootEndpointId, mProvider));
    ASSERT_SUCCESS(mProvider.Startup(mTestContext.ImContext()));

    EXPECT_FALSE(rootNode.GetFeature<PowerSourceFeature>().IsEnabled());
    EXPECT_FALSE(rootNode.GetFeature<TimeSynchronizationFeature>().IsEnabled());
    EXPECT_FALSE(rootNode.GetFeature<TlsFeature>().IsEnabled());

    EXPECT_FALSE(HasServerCluster(kRootEndpointId, PowerSource::Id));
    EXPECT_FALSE(HasServerCluster(kRootEndpointId, TimeSynchronization::Id));
    EXPECT_FALSE(HasServerCluster(kRootEndpointId, TlsCertificateManagement::Id));
    EXPECT_FALSE(HasServerCluster(kRootEndpointId, TlsClientManagement::Id));

    rootNode.Unregister(mProvider);
    EXPECT_TRUE(GetEndpoints().empty());
    EXPECT_SUCCESS(mProvider.Shutdown());
}

TEST_F(TestRootNodeFeatures, TimeSynchronizationUsesDefaultDelegateWhenNull)
{
    RootNodeWith<TimeSynchronizationFeature> rootNode(MakeRootNodeContext(),
                                                      TimeSynchronizationFeature::Context{
                                                          .enabled                = true,
                                                          .caseSessionManager     = mCASESessionManager,
                                                          .interactionModelEngine = mTestContext.GetInteractionModelEngine(),
                                                          .delegate               = nullptr,
                                                      });

    ASSERT_SUCCESS(rootNode.Register(kRootEndpointId, mProvider));
    ASSERT_SUCCESS(mProvider.Startup(mTestContext.ImContext()));

    EXPECT_NE(rootNode.GetFeature<TimeSynchronizationFeature>().Cluster().GetDelegate(), nullptr);

    rootNode.Unregister(mProvider);
    EXPECT_SUCCESS(mProvider.Shutdown());
}

TEST_F(TestRootNodeFeatures, TlsDelegatesManageCertificatesEndpointsAndDependencies)
{
    RootNodeWith<TlsFeature> rootNode(MakeRootNodeContext(),
                                      TlsFeature::Context{
                                          .enabled        = true,
                                          .maxRootCerts   = 5,
                                          .maxClientCerts = 5,
                                          .maxEndpoints   = 5,
                                      });

    ASSERT_SUCCESS(rootNode.Register(kRootEndpointId, mProvider));
    ASSERT_SUCCESS(mProvider.Startup(mTestContext.ImContext()));

    auto & tlsFeature     = rootNode.GetFeature<TlsFeature>();
    auto & certDelegate   = tlsFeature.CertificateDelegate();
    auto & clientDelegate = tlsFeature.ClientDelegate();

    const FabricIndex kFabric                 = GetAliceFabricIndex();
    static constexpr uint8_t kRootCertBytes[] = { 0x30, 0x82, 0x01, 0x01, 0xAA, 0xBB };

    TlsCertificateManagement::Commands::ProvisionRootCertificate::DecodableType rootReq;
    rootReq.certificate = ByteSpan(kRootCertBytes);
    rootReq.caid.SetNull();

    Tls::TLSCAID caid = 0;
    ASSERT_EQ(certDelegate.ProvisionRootCert(kRootEndpointId, kFabric, rootReq, caid),
              Protocols::InteractionModel::Status::Success);

    bool foundRoot = false;
    ASSERT_SUCCESS(certDelegate.FindRootCert(kRootEndpointId, kFabric, caid, [&](auto & cert) -> CHIP_ERROR {
        foundRoot = cert.certificate.HasValue() && cert.certificate.Value().data_equal(ByteSpan(kRootCertBytes));
        return CHIP_NO_ERROR;
    }));
    EXPECT_TRUE(foundRoot);

    bool lookedUpRoot = false;
    ASSERT_SUCCESS(certDelegate.LookupRootCert(kRootEndpointId, kFabric, ByteSpan(kRootCertBytes), [&](auto & cert) -> CHIP_ERROR {
        lookedUpRoot = (cert.caid == caid);
        return CHIP_NO_ERROR;
    }));
    EXPECT_TRUE(lookedUpRoot);

    TlsClientManagement::Commands::ProvisionEndpoint::DecodableType epReq;
    epReq.hostname = ByteSpan(reinterpret_cast<const uint8_t *>("camera.example.com"), 18);
    epReq.port     = 443;
    epReq.caid     = caid;
    epReq.ccdid.SetNull();
    epReq.endpointID.SetNull();

    uint16_t provisionedEndpointId = 0;
    auto epStatus                  = clientDelegate.ProvisionEndpoint(kRootEndpointId, kFabric, epReq, provisionedEndpointId);
    EXPECT_TRUE(epStatus.IsSuccess());

    bool foundEndpoint = false;
    ASSERT_SUCCESS(
        clientDelegate.FindProvisionedEndpointByID(kRootEndpointId, kFabric, provisionedEndpointId, [&](auto & ep) -> CHIP_ERROR {
            foundEndpoint = (ep.caid == caid && ep.port == 443);
            return CHIP_NO_ERROR;
        }));
    EXPECT_TRUE(foundEndpoint);

    // Dependency checker must reject removing a root cert referenced by a provisioned endpoint.
    EXPECT_NE(clientDelegate.RootCertCanBeRemoved(kRootEndpointId, kFabric, caid), CHIP_NO_ERROR);

    // Remove endpoint, then dependency check passes and root cert can be removed.
    EXPECT_EQ(clientDelegate.RemoveProvisionedEndpointByID(kRootEndpointId, kFabric, provisionedEndpointId),
              Protocols::InteractionModel::Status::Success);
    EXPECT_SUCCESS(clientDelegate.RootCertCanBeRemoved(kRootEndpointId, kFabric, caid));
    EXPECT_EQ(certDelegate.RemoveRootCert(kRootEndpointId, kFabric, caid), Protocols::InteractionModel::Status::Success);

    // Re-provision endpoint and verify RemoveFabric clears it using injected DataModel::Provider.
    epStatus = clientDelegate.ProvisionEndpoint(kRootEndpointId, kFabric, epReq, provisionedEndpointId);
    EXPECT_TRUE(epStatus.IsSuccess());
    clientDelegate.RemoveFabric(kFabric);

    size_t remainingEndpoints = 0;
    ASSERT_SUCCESS(clientDelegate.ForEachEndpoint(kRootEndpointId, kFabric, [&](auto &) -> CHIP_ERROR {
        ++remainingEndpoints;
        return CHIP_NO_ERROR;
    }));
    EXPECT_EQ(remainingEndpoints, 0u);

    rootNode.Unregister(mProvider);
    EXPECT_SUCCESS(mProvider.Shutdown());
}

} // namespace
