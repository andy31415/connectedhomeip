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
#include <app/server-cluster/DefaultServerCluster.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <clusters/BasicInformation/Enums.h>
#include <clusters/BasicInformation/Metadata.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>
#include <lib/support/ReadOnlyBuffer.h>
#include <lib/support/Span.h>
#include <platform/NetworkCommissioning.h>
#include <protocols/interaction_model/Constants.h>

namespace {

using namespace chip;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::BasicInformation;
using namespace chip::app::Clusters::BasicInformation::Attributes;

using chip::app::AttributeValueDecoder;
using chip::app::DataModel::AcceptedCommandEntry;
using chip::app::DataModel::ActionReturnStatus;
using chip::app::DataModel::AttributeEntry;
using chip::app::Testing::kAdminSubjectDescriptor;
using chip::app::Testing::WriteOperation;
using chip::Protocols::InteractionModel::Status;
using chip::Test::TestServerClusterContext;

// initialize memory as ReadOnlyBufferBuilder may allocate
struct TestBasicInformationCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { chip::Platform::MemoryShutdown(); }
};

/// Ensures that the basic info instance was startup/shutdown correctly given the
/// internal context of this class.
struct StartupClusterScope
{
    TestServerClusterContext context;
    StartupClusterScope() { VerifyOrDie(BasicInformationCluster::Instance().Startup(context.Get()) == CHIP_NO_ERROR); }
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

} // namespace
