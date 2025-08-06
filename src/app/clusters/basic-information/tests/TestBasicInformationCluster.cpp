/*
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <pw_unit_test/framework.h>

#include <app/clusters/basic-information/BasicInformationCluster.h>
#include <app/clusters/testing/AttributeTesting.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/tests/ReadTesting.h>
#include <app/data-model-provider/tests/WriteTesting.h>
#include <app/data-model/Decode.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <clusters/BasicInformation/Attributes.h>
#include <clusters/BasicInformation/Enums.h>
#include <clusters/BasicInformation/Metadata.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/ReadOnlyBuffer.h>
#include <lib/support/Span.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/NetworkCommissioning.h>
#include <protocols/interaction_model/Constants.h>

namespace {

using namespace chip;
using namespace chip::app::Testing;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::BasicInformation;
using namespace chip::app::Clusters::BasicInformation::Attributes;

using chip::app::AttributeValueDecoder;
using chip::app::DataModel::AcceptedCommandEntry;
using chip::app::DataModel::ActionReturnStatus;
using chip::app::DataModel::AttributeEntry;
using chip::Platform::CopyString;
using chip::Protocols::InteractionModel::Status;
using chip::Test::TestServerClusterContext;

// initialize memory as ReadOnlyBufferBuilder may allocate
struct TestBasicInformationCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { chip::Platform::MemoryShutdown(); }
};

// Implements a fake device info provider, with everything hardcoded.
class FakeDeviceInfoProvider : public DeviceInstanceInfoProvider
{
public:
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "FakeVendor");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetVendorId(uint16_t & vendorId) override
    {
        vendorId = 1234;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "FakeProduct");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductId(uint16_t & productId) override
    {
        productId = 5678;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "PN123456");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "http://fake.example.com");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "FakeLabel");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "SN987654");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override
    {
        year  = 2025;
        month = 1;
        day   = 15;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override
    {
        hardwareVersion = 2;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override
    {
        CopyString(buf, bufSize, "2.0");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan) override
    {
        const uint8_t uniqueId[] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        VerifyOrReturnError(uniqueIdSpan.size() >= sizeof(uniqueId), CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(uniqueIdSpan.data(), uniqueId, sizeof(uniqueId));
        uniqueIdSpan.reduce_size(sizeof(uniqueId));
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductFinish(app::Clusters::BasicInformation::ProductFinishEnum * finish) override
    {
        *finish = app::Clusters::BasicInformation::ProductFinishEnum::kSatin;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductPrimaryColor(app::Clusters::BasicInformation::ColorEnum * primaryColor) override
    {
        *primaryColor = app::Clusters::BasicInformation::ColorEnum::kBlue;
        return CHIP_NO_ERROR;
    }
};

/// has to be global: setters of this info are "sticky" (cannot be reset back to nullptr)
FakeDeviceInfoProvider gInfoProvider;

/// Ensures that the basic info instance was startup/shutdown correctly given the
/// internal context of this class.
struct StartupClusterScope
{
    TestServerClusterContext context;

    StartupClusterScope()
    {
        SetDeviceInstanceInfoProvider(&gInfoProvider);
        VerifyOrDie(BasicInformationCluster::Instance().Startup(context.Get()) == CHIP_NO_ERROR);
    }
    ~StartupClusterScope() { BasicInformationCluster::Instance().Shutdown(); }
};

TEST_F(TestBasicInformationCluster, TestAttributes)
{

    // save and restore the flags at the end of the test
    struct SaveFlags
    {
        SaveFlags() { mFlags = BasicInformationCluster::Instance().OptionalAttributes(); }
        ~SaveFlags() { BasicInformationCluster::Instance().OptionalAttributes() = mFlags; }

    private:
        BitFlags<OptionalBasicInformationAttributes> mFlags;
    } scopedFlagsSave;

    // check without optional attributes
    {
        BasicInformationCluster::Instance().OptionalAttributes().ClearAll();

        ReadOnlyBufferBuilder<AttributeEntry> builder;
        ASSERT_EQ(BasicInformationCluster::Instance().Attributes({ kRootEndpointId, BasicInformation::Id }, builder),
                  CHIP_NO_ERROR);

        ReadOnlyBufferBuilder<AttributeEntry> expectedBuilder;
        ASSERT_EQ(expectedBuilder.AppendElements({
                      DataModelRevision::kMetadataEntry, VendorName::kMetadataEntry, VendorID::kMetadataEntry,
                      ProductName::kMetadataEntry, ProductID::kMetadataEntry, NodeLabel::kMetadataEntry, Location::kMetadataEntry,
                      HardwareVersion::kMetadataEntry, HardwareVersionString::kMetadataEntry, SoftwareVersion::kMetadataEntry,
                      SoftwareVersionString::kMetadataEntry, CapabilityMinima::kMetadataEntry, SpecificationVersion::kMetadataEntry,
                      MaxPathsPerInvoke::kMetadataEntry, ConfigurationVersion::kMetadataEntry,
                      UniqueID::kMetadataEntry, // required in latest spec
                  }),
                  CHIP_NO_ERROR);
        ASSERT_EQ(expectedBuilder.ReferenceExisting(app::DefaultServerCluster::GlobalAttributes()), CHIP_NO_ERROR);
        ASSERT_TRUE(Testing::EqualAttributeSets(builder.TakeBuffer(), expectedBuilder.TakeBuffer()));
    }

    // Check that disabling unique id works
    {
        BasicInformationCluster::Instance().OptionalAttributes().ClearAll().Set(
            OptionalBasicInformationAttributes::kDisableMandatoryUniqueIDOnPurpose); // allowed in early spec version. Force this.

        ReadOnlyBufferBuilder<AttributeEntry> builder;
        ASSERT_EQ(BasicInformationCluster::Instance().Attributes({ kRootEndpointId, BasicInformation::Id }, builder),
                  CHIP_NO_ERROR);

        ReadOnlyBufferBuilder<AttributeEntry> expectedBuilder;
        ASSERT_EQ(expectedBuilder.AppendElements({
                      DataModelRevision::kMetadataEntry,
                      VendorName::kMetadataEntry,
                      VendorID::kMetadataEntry,
                      ProductName::kMetadataEntry,
                      ProductID::kMetadataEntry,
                      NodeLabel::kMetadataEntry,
                      Location::kMetadataEntry,
                      HardwareVersion::kMetadataEntry,
                      HardwareVersionString::kMetadataEntry,
                      SoftwareVersion::kMetadataEntry,
                      SoftwareVersionString::kMetadataEntry,
                      CapabilityMinima::kMetadataEntry,
                      SpecificationVersion::kMetadataEntry,
                      MaxPathsPerInvoke::kMetadataEntry,
                      ConfigurationVersion::kMetadataEntry,
                  }),
                  CHIP_NO_ERROR);
        ASSERT_EQ(expectedBuilder.ReferenceExisting(app::DefaultServerCluster::GlobalAttributes()), CHIP_NO_ERROR);
        ASSERT_TRUE(Testing::EqualAttributeSets(builder.TakeBuffer(), expectedBuilder.TakeBuffer()));
    }

    // All attributes
    {
        BasicInformationCluster::Instance()
            .OptionalAttributes()
            .ClearAll()
            .Set(OptionalBasicInformationAttributes::kManufacturingDate)
            .Set(OptionalBasicInformationAttributes::kPartNumber)
            .Set(OptionalBasicInformationAttributes::kProductURL)
            .Set(OptionalBasicInformationAttributes::kProductLabel)
            .Set(OptionalBasicInformationAttributes::kSerialNumber)
            .Set(OptionalBasicInformationAttributes::kLocalConfigDisabled)
            .Set(OptionalBasicInformationAttributes::kReachable)
            .Set(OptionalBasicInformationAttributes::kProductAppearance);

        ReadOnlyBufferBuilder<AttributeEntry> builder;
        ASSERT_EQ(BasicInformationCluster::Instance().Attributes({ kRootEndpointId, BasicInformation::Id }, builder),
                  CHIP_NO_ERROR);

        ReadOnlyBufferBuilder<AttributeEntry> expectedBuilder;
        ASSERT_EQ(expectedBuilder.AppendElements({
                      DataModelRevision::kMetadataEntry,
                      VendorName::kMetadataEntry,
                      VendorID::kMetadataEntry,
                      ProductName::kMetadataEntry,
                      ProductID::kMetadataEntry,
                      NodeLabel::kMetadataEntry,
                      Location::kMetadataEntry,
                      HardwareVersion::kMetadataEntry,
                      HardwareVersionString::kMetadataEntry,
                      SoftwareVersion::kMetadataEntry,
                      SoftwareVersionString::kMetadataEntry,
                      CapabilityMinima::kMetadataEntry,
                      SpecificationVersion::kMetadataEntry,
                      MaxPathsPerInvoke::kMetadataEntry,
                      ConfigurationVersion::kMetadataEntry,
                      UniqueID::kMetadataEntry,
                      ManufacturingDate::kMetadataEntry,
                      PartNumber::kMetadataEntry,
                      ProductURL::kMetadataEntry,
                      ProductLabel::kMetadataEntry,
                      SerialNumber::kMetadataEntry,
                      LocalConfigDisabled::kMetadataEntry,
                      Reachable::kMetadataEntry,
                      ProductAppearance::kMetadataEntry,
                  }),
                  CHIP_NO_ERROR);
        ASSERT_EQ(expectedBuilder.ReferenceExisting(app::DefaultServerCluster::GlobalAttributes()), CHIP_NO_ERROR);
        ASSERT_TRUE(Testing::EqualAttributeSets(builder.TakeBuffer(), expectedBuilder.TakeBuffer()));
    }
}

TEST_F(TestBasicInformationCluster, TestWriteNodeLabelConstraint)
{
    StartupClusterScope scope;

    WriteOperation write(kRootEndpointId, BasicInformation::Id, NodeLabel::Id);
    write.SetSubjectDescriptor(kAdminSubjectDescriptor);

    // NodeLabel max length is 32. We try to write a 33-char span
    AttributeValueDecoder decoder = write.DecoderFor("123456789012345678901234567890123"_span);
    EXPECT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), Status::ConstraintError);
}

TEST_F(TestBasicInformationCluster, TestWriteLocationConstraint)
{
    StartupClusterScope scope;

    WriteOperation write(kRootEndpointId, BasicInformation::Id, Location::Id);
    write.SetSubjectDescriptor(kAdminSubjectDescriptor);

    // Location must be exactly kFixedLocationLength == 2
    {
        AttributeValueDecoder decoder = write.DecoderFor("abc"_span);
        EXPECT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), Status::ConstraintError);
    }
    {
        AttributeValueDecoder decoder = write.DecoderFor("a"_span);
        EXPECT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), Status::ConstraintError);
    }
}

TEST_F(TestBasicInformationCluster, TestNodeLabelRW)
{
    StartupClusterScope scope;

    // Read back and verify NodeLabel
    const char * newNodeLabel = "TestLabel123";
    {
        WriteOperation write(kRootEndpointId, BasicInformation::Id, NodeLabel::Id);
        write.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto decoder = write.DecoderFor<CharSpan>(CharSpan::fromCharString(newNodeLabel));
        ASSERT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), CHIP_NO_ERROR);
    }
    {
        ReadOperation read({ kRootEndpointId, BasicInformation::Id, NodeLabel::Id });
        read.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto encoder = read.StartEncoding();
        ASSERT_EQ(BasicInformationCluster::Instance().ReadAttribute(read.GetRequest(), *encoder), CHIP_NO_ERROR);
        ASSERT_EQ(read.FinishEncoding(), CHIP_NO_ERROR);
        std::vector<DecodedAttributeData> decodedData;
        ASSERT_EQ(read.GetEncodedIBs().Decode(decodedData), CHIP_NO_ERROR);
        ASSERT_EQ(decodedData.size(), 1u);
        NodeLabel::TypeInfo::DecodableType val;
        ASSERT_EQ(chip::app::DataModel::Decode(decodedData[0].dataReader, val), CHIP_NO_ERROR);
        ASSERT_TRUE(val.data_equal(CharSpan::fromCharString(newNodeLabel)));
    }
}

TEST_F(TestBasicInformationCluster, TestLocalConfigRW)
{
    StartupClusterScope scope;

    // check that writing true to local config disabled works
    {
        WriteOperation write(kRootEndpointId, BasicInformation::Id, LocalConfigDisabled::Id);
        write.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto decoder = write.DecoderFor<bool>(true);
        ASSERT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), CHIP_NO_ERROR);
    }
    {
        ReadOperation read({ kRootEndpointId, BasicInformation::Id, LocalConfigDisabled::Id });
        read.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto encoder = read.StartEncoding();
        ASSERT_EQ(BasicInformationCluster::Instance().ReadAttribute(read.GetRequest(), *encoder), CHIP_NO_ERROR);
        ASSERT_EQ(read.FinishEncoding(), CHIP_NO_ERROR);
        std::vector<DecodedAttributeData> decodedData;
        ASSERT_EQ(read.GetEncodedIBs().Decode(decodedData), CHIP_NO_ERROR);
        ASSERT_EQ(decodedData.size(), 1u);
        LocalConfigDisabled::TypeInfo::DecodableType val;
        ASSERT_EQ(chip::app::DataModel::Decode(decodedData[0].dataReader, val), CHIP_NO_ERROR);
        ASSERT_EQ(val, true);
    }

    // check that writing false to local config disabled works
    {
        WriteOperation write(kRootEndpointId, BasicInformation::Id, LocalConfigDisabled::Id);
        write.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto decoder = write.DecoderFor<bool>(false);
        ASSERT_EQ(BasicInformationCluster::Instance().WriteAttribute(write.GetRequest(), decoder), CHIP_NO_ERROR);
    }
    {
        ReadOperation read({ kRootEndpointId, BasicInformation::Id, LocalConfigDisabled::Id });
        read.SetSubjectDescriptor(kAdminSubjectDescriptor);
        auto encoder = read.StartEncoding();
        ASSERT_EQ(BasicInformationCluster::Instance().ReadAttribute(read.GetRequest(), *encoder), CHIP_NO_ERROR);
        ASSERT_EQ(read.FinishEncoding(), CHIP_NO_ERROR);
        std::vector<DecodedAttributeData> decodedData;
        ASSERT_EQ(read.GetEncodedIBs().Decode(decodedData), CHIP_NO_ERROR);
        ASSERT_EQ(decodedData.size(), 1u);
        LocalConfigDisabled::TypeInfo::DecodableType val;
        ASSERT_EQ(chip::app::DataModel::Decode(decodedData[0].dataReader, val), CHIP_NO_ERROR);
        ASSERT_EQ(val, false);
    }
}

} // namespace
