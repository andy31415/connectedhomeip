/*
 *    Copyright (c) 2024 Project CHIP Authors
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
#include <app/codegen-interaction-model/Model.h>

#include "EmberReadWriteOverride.h"

#include <access/AccessControl.h>
#include <access/SubjectDescriptor.h>
#include <app/MessageDef/ReportDataMessage.h>
#include <app/util/mock/Constants.h>
#include <app/util/mock/Functions.h>
#include <app/util/mock/MockNodeConfig.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLVDebug.h>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVWriter.h>

#include <gtest/gtest.h>
#include <vector>

// TODO: CHIP_ERROR tostring should be separated out
#include <pw_span/span.h>

using namespace chip;
using namespace chip::Test;
using namespace chip::app;
using namespace chip::app::InteractionModel;
using namespace chip::app::Clusters::Globals::Attributes;

namespace pw {

template <>
StatusWithSize ToString<CHIP_ERROR>(const CHIP_ERROR & err, pw::span<char> buffer)
{
    if (CHIP_ERROR::IsSuccess(err))
    {
        // source location probably does not matter
        return pw::string::Format(buffer, "CHIP_NO_ERROR");
    }
    return pw::string::Format(buffer, "CHIP_ERROR:<%" CHIP_ERROR_FORMAT ">", err.Format());
}

} // namespace pw

namespace {

constexpr FabricIndex kTestFabrixIndex = kMinValidFabricIndex;
constexpr NodeId kTestNodeId           = 0xFFFF'1234'ABCD'4321;

constexpr EndpointId kEndpointIdThatIsMissing = kMockEndpointMin - 1;

static_assert(kEndpointIdThatIsMissing != kInvalidEndpointId);
static_assert(kEndpointIdThatIsMissing != kMockEndpoint1);
static_assert(kEndpointIdThatIsMissing != kMockEndpoint2);
static_assert(kEndpointIdThatIsMissing != kMockEndpoint3);

constexpr Access::SubjectDescriptor kAdminSubjectDescriptor{
    .fabricIndex = kTestFabrixIndex,
    .authMode    = Access::AuthMode::kCase,
    .subject     = kTestNodeId,
};
constexpr Access::SubjectDescriptor kViewSubjectDescriptor{
    .fabricIndex = kTestFabrixIndex + 1,
    .authMode    = Access::AuthMode::kCase,
    .subject     = kTestNodeId,
};

constexpr Access::SubjectDescriptor kDenySubjectDescriptor{
    .fabricIndex = kTestFabrixIndex + 2,
    .authMode    = Access::AuthMode::kCase,
    .subject     = kTestNodeId,
};

bool operator==(const Access::SubjectDescriptor & a, const Access::SubjectDescriptor & b)
{
    if (a.fabricIndex != b.fabricIndex)
    {
        return false;
    }
    if (a.authMode != b.authMode)
    {
        return false;
    }
    if (a.subject != b.subject)
    {
        return false;
    }
    for (unsigned i = 0; i < a.cats.values.size(); i++)
    {
        if (a.cats.values[i] != b.cats.values[i])
        {
            return false;
        }
    }
    return true;
}

class MockAccessControl : public Access::AccessControl::Delegate, public Access::AccessControl::DeviceTypeResolver
{
public:
    CHIP_ERROR Check(const Access::SubjectDescriptor & subjectDescriptor, const Access::RequestPath & requestPath,
                     Access::Privilege requestPrivilege) override
    {
        if (subjectDescriptor == kAdminSubjectDescriptor)
        {
            return CHIP_NO_ERROR;
        }
        if ((subjectDescriptor == kViewSubjectDescriptor) && (requestPrivilege == Access::Privilege::kView))
        {
            return CHIP_NO_ERROR;
        }
        return CHIP_ERROR_ACCESS_DENIED;
    }

    bool IsDeviceTypeOnEndpoint(DeviceTypeId deviceType, EndpointId endpoint) override { return true; }
};

class ScopedMockAccessControl
{
public:
    ScopedMockAccessControl() { Access::GetAccessControl().Init(&mMock, mMock); }
    ~ScopedMockAccessControl() { Access::GetAccessControl().Finish(); }

private:
    MockAccessControl mMock;
};

// clang-format off
const MockNodeConfig gTestNodeConfig({
    MockEndpointConfig(kMockEndpoint1, {
        MockClusterConfig(MockClusterId(1), {
            ClusterRevision::Id, FeatureMap::Id,
        }, {
            MockEventId(1), MockEventId(2),
        }),
        MockClusterConfig(MockClusterId(2), {
            ClusterRevision::Id, FeatureMap::Id, MockAttributeId(1),
        }),
    }),
    MockEndpointConfig(kMockEndpoint2, {
        MockClusterConfig(MockClusterId(1), {
            ClusterRevision::Id, FeatureMap::Id,
        }),
        MockClusterConfig(MockClusterId(2), {
            ClusterRevision::Id,
            FeatureMap::Id,
            MockAttributeId(1),
            MockAttributeConfig(MockAttributeId(2), ZCL_ARRAY_ATTRIBUTE_TYPE),
        }),
        MockClusterConfig(MockClusterId(3), {
            ClusterRevision::Id, FeatureMap::Id, MockAttributeId(1), MockAttributeId(2), MockAttributeId(3),
        }),
    }),
    MockEndpointConfig(kMockEndpoint3, {
        MockClusterConfig(MockClusterId(1), {
            ClusterRevision::Id, FeatureMap::Id, MockAttributeId(1),
        }),
        MockClusterConfig(MockClusterId(2), {
            ClusterRevision::Id, FeatureMap::Id, MockAttributeId(1), MockAttributeId(2), MockAttributeId(3), MockAttributeId(4),
        }),
        MockClusterConfig(MockClusterId(3), {
            ClusterRevision::Id, FeatureMap::Id,
        }),
        MockClusterConfig(MockClusterId(4), {
            ClusterRevision::Id, FeatureMap::Id,
        }),
    }),
});
// clang-format on

struct UseMockNodeConfig
{
    UseMockNodeConfig(const MockNodeConfig & config) { SetMockNodeConfig(config); }
    ~UseMockNodeConfig() { ResetMockNodeConfig(); }
};

struct DecodedAttributeData
{
    chip::DataVersion dataVersion;
    ConcreteDataAttributePath attributePath;
    TLV::TLVReader dataReader;

    CHIP_ERROR DecodeFrom(const AttributeDataIB::Parser & parser)
    {
        ReturnErrorOnFailure(parser.GetDataVersion(&dataVersion));

        AttributePathIB::Parser pathParser;
        ReturnErrorOnFailure(parser.GetPath(&pathParser));
        ReturnErrorOnFailure(pathParser.GetConcreteAttributePath(attributePath, AttributePathIB::ValidateIdRanges::kNo));
        ReturnErrorOnFailure(parser.GetData(&dataReader));

        return CHIP_NO_ERROR;
    }
};

CHIP_ERROR DecodeAttributeReportIBs(ByteSpan data, std::vector<DecodedAttributeData> & decoded_items)
{
    // Espected data format:
    //   CONTAINER (anonymous)
    //     0x01 => Array (i.e. report data ib)
    //       ReportIB*
    //
    // Overally this is VERY hard to process ...
    //
    TLV::TLVReader reportIBsReader;
    reportIBsReader.Init(data);

    ReturnErrorOnFailure(reportIBsReader.Next());
    if (reportIBsReader.GetType() != TLV::TLVType::kTLVType_Structure)
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    TLV::TLVType outer1;
    reportIBsReader.EnterContainer(outer1);

    ReturnErrorOnFailure(reportIBsReader.Next());
    if (reportIBsReader.GetType() != TLV::TLVType::kTLVType_Array)
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    TLV::TLVType outer2;
    reportIBsReader.EnterContainer(outer2);

    CHIP_ERROR err = CHIP_NO_ERROR;
    while (CHIP_NO_ERROR == (err = reportIBsReader.Next()))
    {
        TLV::TLVReader attributeReportReader = reportIBsReader;
        AttributeReportIB::Parser attributeReportParser;
        ReturnErrorOnFailure(attributeReportParser.Init(attributeReportReader));

        AttributeDataIB::Parser dataParser;
        // NOTE: to also grab statuses, use GetAttributeStatus and check for CHIP_END_OF_TLV
        ReturnErrorOnFailure(attributeReportParser.GetAttributeData(&dataParser));

        DecodedAttributeData decoded;
        ReturnErrorOnFailure(decoded.DecodeFrom(dataParser));
        decoded_items.push_back(decoded);
    }

    if ((CHIP_END_OF_TLV != err) && (err != CHIP_NO_ERROR))
    {
        return CHIP_NO_ERROR;
    }

    ReturnErrorOnFailure(reportIBsReader.ExitContainer(outer2));
    ReturnErrorOnFailure(reportIBsReader.ExitContainer(outer1));

    err = reportIBsReader.Next();

    if (CHIP_ERROR_END_OF_TLV == err)
    {
        return CHIP_NO_ERROR;
    }
    if (CHIP_NO_ERROR == err)
    {
        // This is NOT ok ... we have multiple things in our buffer?
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    return err;
}

} // namespace

TEST(TestCodegenModelViaMocks, IterateOverEndpoints)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;

    // This iteration relies on the hard-coding that occurs when mock_ember is used
    EXPECT_EQ(model.FirstEndpoint(), kMockEndpoint1);
    EXPECT_EQ(model.NextEndpoint(kMockEndpoint1), kMockEndpoint2);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint2), kMockEndpoint3);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint3), kInvalidEndpointId);

    /// Some out of order requests should work as well
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint2), kMockEndpoint3);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint2), kMockEndpoint3);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint1), kMockEndpoint2);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint1), kMockEndpoint2);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint2), kMockEndpoint3);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint1), kMockEndpoint2);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint3), kInvalidEndpointId);
    ASSERT_EQ(model.NextEndpoint(kMockEndpoint3), kInvalidEndpointId);
    ASSERT_EQ(model.FirstEndpoint(), kMockEndpoint1);
    ASSERT_EQ(model.FirstEndpoint(), kMockEndpoint1);
}

TEST(TestCodegenModelViaMocks, IterateOverClusters)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;

    chip::Test::ResetVersion();

    EXPECT_FALSE(model.FirstCluster(kEndpointIdThatIsMissing).path.HasValidIds());
    EXPECT_FALSE(model.FirstCluster(kInvalidEndpointId).path.HasValidIds());

    // mock endpoint 1 has 2 mock clusters: 1 and 2
    ClusterEntry entry = model.FirstCluster(kMockEndpoint1);
    ASSERT_TRUE(entry.path.HasValidIds());
    EXPECT_EQ(entry.path.mEndpointId, kMockEndpoint1);
    EXPECT_EQ(entry.path.mClusterId, MockClusterId(1));
    EXPECT_EQ(entry.info.dataVersion, 0u);
    EXPECT_EQ(entry.info.flags.Raw(), 0u);

    chip::Test::BumpVersion();

    entry = model.NextCluster(entry.path);
    ASSERT_TRUE(entry.path.HasValidIds());
    EXPECT_EQ(entry.path.mEndpointId, kMockEndpoint1);
    EXPECT_EQ(entry.path.mClusterId, MockClusterId(2));
    EXPECT_EQ(entry.info.dataVersion, 1u);
    EXPECT_EQ(entry.info.flags.Raw(), 0u);

    entry = model.NextCluster(entry.path);
    EXPECT_FALSE(entry.path.HasValidIds());

    // mock endpoint 3 has 4 mock clusters: 1 through 4
    entry = model.FirstCluster(kMockEndpoint3);
    for (uint16_t clusterId = 1; clusterId <= 4; clusterId++)
    {
        ASSERT_TRUE(entry.path.HasValidIds());
        EXPECT_EQ(entry.path.mEndpointId, kMockEndpoint3);
        EXPECT_EQ(entry.path.mClusterId, MockClusterId(clusterId));
        entry = model.NextCluster(entry.path);
    }
    EXPECT_FALSE(entry.path.HasValidIds());

    // repeat calls should work
    for (int i = 0; i < 10; i++)
    {
        entry = model.FirstCluster(kMockEndpoint1);
        ASSERT_TRUE(entry.path.HasValidIds());
        EXPECT_EQ(entry.path.mEndpointId, kMockEndpoint1);
        EXPECT_EQ(entry.path.mClusterId, MockClusterId(1));
    }

    for (int i = 0; i < 10; i++)
    {
        ClusterEntry nextEntry = model.NextCluster(entry.path);
        ASSERT_TRUE(nextEntry.path.HasValidIds());
        EXPECT_EQ(nextEntry.path.mEndpointId, kMockEndpoint1);
        EXPECT_EQ(nextEntry.path.mClusterId, MockClusterId(2));
    }
}

TEST(TestCodegenModelViaMocks, GetClusterInfo)
{

    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;

    chip::Test::ResetVersion();

    ASSERT_FALSE(model.GetClusterInfo(ConcreteClusterPath(kInvalidEndpointId, kInvalidClusterId)).has_value());
    ASSERT_FALSE(model.GetClusterInfo(ConcreteClusterPath(kInvalidEndpointId, MockClusterId(1))).has_value());
    ASSERT_FALSE(model.GetClusterInfo(ConcreteClusterPath(kMockEndpoint1, kInvalidClusterId)).has_value());
    ASSERT_FALSE(model.GetClusterInfo(ConcreteClusterPath(kMockEndpoint1, MockClusterId(10))).has_value());

    // now get the value
    std::optional<ClusterInfo> info = model.GetClusterInfo(ConcreteClusterPath(kMockEndpoint1, MockClusterId(1)));
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->dataVersion, 0u); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(info->flags.Raw(), 0u); // NOLINT(bugprone-unchecked-optional-access)

    chip::Test::BumpVersion();
    info = model.GetClusterInfo(ConcreteClusterPath(kMockEndpoint1, MockClusterId(1)));
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->dataVersion, 1u); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(info->flags.Raw(), 0u); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TestCodegenModelViaMocks, IterateOverAttributes)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;

    // invalid paths should return in "no more data"
    ASSERT_FALSE(model.FirstAttribute(ConcreteClusterPath(kEndpointIdThatIsMissing, MockClusterId(1))).path.HasValidIds());
    ASSERT_FALSE(model.FirstAttribute(ConcreteClusterPath(kInvalidEndpointId, MockClusterId(1))).path.HasValidIds());
    ASSERT_FALSE(model.FirstAttribute(ConcreteClusterPath(kMockEndpoint1, MockClusterId(10))).path.HasValidIds());
    ASSERT_FALSE(model.FirstAttribute(ConcreteClusterPath(kMockEndpoint1, kInvalidClusterId)).path.HasValidIds());

    // should be able to iterate over valid paths
    AttributeEntry entry = model.FirstAttribute(ConcreteClusterPath(kMockEndpoint2, MockClusterId(2)));
    ASSERT_TRUE(entry.path.HasValidIds());
    ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
    ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
    ASSERT_EQ(entry.path.mAttributeId, ClusterRevision::Id);
    ASSERT_FALSE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));

    entry = model.NextAttribute(entry.path);
    ASSERT_TRUE(entry.path.HasValidIds());
    ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
    ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
    ASSERT_EQ(entry.path.mAttributeId, FeatureMap::Id);
    ASSERT_FALSE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));

    entry = model.NextAttribute(entry.path);
    ASSERT_TRUE(entry.path.HasValidIds());
    ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
    ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
    ASSERT_EQ(entry.path.mAttributeId, MockAttributeId(1));
    ASSERT_FALSE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));

    entry = model.NextAttribute(entry.path);
    ASSERT_TRUE(entry.path.HasValidIds());
    ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
    ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
    ASSERT_EQ(entry.path.mAttributeId, MockAttributeId(2));
    ASSERT_TRUE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));

    entry = model.NextAttribute(entry.path);
    ASSERT_FALSE(entry.path.HasValidIds());

    // repeated calls should work
    for (int i = 0; i < 10; i++)
    {
        entry = model.FirstAttribute(ConcreteClusterPath(kMockEndpoint2, MockClusterId(2)));
        ASSERT_TRUE(entry.path.HasValidIds());
        ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
        ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
        ASSERT_EQ(entry.path.mAttributeId, ClusterRevision::Id);
        ASSERT_FALSE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));
    }

    for (int i = 0; i < 10; i++)
    {
        entry = model.NextAttribute(ConcreteAttributePath(kMockEndpoint2, MockClusterId(2), MockAttributeId(1)));
        ASSERT_TRUE(entry.path.HasValidIds());
        ASSERT_EQ(entry.path.mEndpointId, kMockEndpoint2);
        ASSERT_EQ(entry.path.mClusterId, MockClusterId(2));
        ASSERT_EQ(entry.path.mAttributeId, MockAttributeId(2));
        ASSERT_TRUE(entry.info.flags.Has(AttributeQualityFlags::kListAttribute));
    }
}

TEST(TestCodegenModelViaMocks, GetAttributeInfo)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;

    // various non-existent or invalid paths should return no info data
    ASSERT_FALSE(
        model.GetAttributeInfo(ConcreteAttributePath(kInvalidEndpointId, kInvalidClusterId, kInvalidAttributeId)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kInvalidEndpointId, kInvalidClusterId, FeatureMap::Id)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kInvalidEndpointId, MockClusterId(1), FeatureMap::Id)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint1, kInvalidClusterId, FeatureMap::Id)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint1, MockClusterId(10), FeatureMap::Id)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint1, MockClusterId(10), kInvalidAttributeId)).has_value());
    ASSERT_FALSE(model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint1, MockClusterId(1), MockAttributeId(10))).has_value());

    // valid info
    std::optional<AttributeInfo> info =
        model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint1, MockClusterId(1), FeatureMap::Id));
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE(info->flags.Has(AttributeQualityFlags::kListAttribute)); // NOLINT(bugprone-unchecked-optional-access)

    info = model.GetAttributeInfo(ConcreteAttributePath(kMockEndpoint2, MockClusterId(2), MockAttributeId(2)));
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->flags.Has(AttributeQualityFlags::kListAttribute)); // NOLINT(bugprone-unchecked-optional-access)
}

class ADelegate : public Access::AccessControl::Delegate
{
public:
    virtual CHIP_ERROR Check(const Access::SubjectDescriptor & subjectDescriptor, const Access::RequestPath & requestPath,
                             Access::Privilege requestPrivilege)
    {
        // return CHIP_ERROR_ACCESS_DENIED;
        return CHIP_NO_ERROR;
    }
};

class AResolver : public Access::AccessControl::DeviceTypeResolver
{
public:
    bool IsDeviceTypeOnEndpoint(DeviceTypeId deviceType, EndpointId endpoint) override { return true; }
};

TEST(TestCodegenModelViaMocks, EmberAttributeReadAclDeny)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;
    ScopedMockAccessControl accessControl;

    ReadAttributeRequest readRequest;

    // operationFlags is 0 i.e. not internal
    // readFlags is 0 i.e. not fabric filtered
    // dataVersion is missing (no data version filtering)
    readRequest.subjectDescriptor = kDenySubjectDescriptor;
    readRequest.path              = ConcreteAttributePath(kMockEndpoint1, MockClusterId(1), MockAttributeId(10));

    std::optional<ClusterInfo> info = model.GetClusterInfo(readRequest.path);
    ASSERT_TRUE(info.has_value());

    DataVersion dataVersion = info->dataVersion; // NOLINT(bugprone-unchecked-optional-access)

    uint8_t tlvBuffer[1024];

    TLV::TLVWriter tlvWriter;
    tlvWriter.Init(tlvBuffer);

    AttributeReportIBs::Builder builder;
    CHIP_ERROR err = builder.Init(&tlvWriter);
    ASSERT_EQ(err, CHIP_NO_ERROR);
    AttributeValueEncoder encoder(builder, kAdminSubjectDescriptor, readRequest.path, dataVersion);

    err = model.ReadAttribute(readRequest, encoder);
    ASSERT_EQ(err, CHIP_ERROR_ACCESS_DENIED);
}

TEST(TestCodegenModelViaMocks, EmberAttributeInvalidRead)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;
    ScopedMockAccessControl accessControl;

    ReadAttributeRequest readRequest;

    // operationFlags is 0 i.e. not internal
    // readFlags is 0 i.e. not fabric filtered
    // dataVersion is missing (no data version filtering)
    readRequest.subjectDescriptor = kAdminSubjectDescriptor;
    readRequest.path              = ConcreteAttributePath(kMockEndpoint1, MockClusterId(1), MockAttributeId(10));

    std::optional<ClusterInfo> info = model.GetClusterInfo(readRequest.path);
    ASSERT_TRUE(info.has_value());

    DataVersion dataVersion = info->dataVersion; // NOLINT(bugprone-unchecked-optional-access)

    uint8_t tlvBuffer[1024];

    TLV::TLVWriter tlvWriter;
    tlvWriter.Init(tlvBuffer);

    AttributeReportIBs::Builder builder;
    CHIP_ERROR err = builder.Init(&tlvWriter);
    ASSERT_EQ(err, CHIP_NO_ERROR);
    AttributeValueEncoder encoder(builder, kAdminSubjectDescriptor, readRequest.path, dataVersion);

    err = model.ReadAttribute(readRequest, encoder);
    ASSERT_EQ(err, CHIP_IM_GLOBAL_STATUS(UnsupportedAttribute));

    // TODO: value validation here?
}

TEST(TestCodegenModelViaMocks, EmberAttributeRead)
{
    UseMockNodeConfig config(gTestNodeConfig);
    chip::app::CodegenDataModel::Model model;
    ScopedMockAccessControl accessControl;

    ReadAttributeRequest readRequest;

    // operationFlags is 0 i.e. not internal
    // readFlags is 0 i.e. not fabric filtered
    // dataVersion is missing (no data version filtering)
    readRequest.subjectDescriptor = kAdminSubjectDescriptor;
    readRequest.path              = ConcreteAttributePath(kMockEndpoint3, MockClusterId(2), MockAttributeId(3));

    std::optional<ClusterInfo> info = model.GetClusterInfo(readRequest.path);
    ASSERT_TRUE(info.has_value());

    DataVersion dataVersion = info->dataVersion; // NOLINT(bugprone-unchecked-optional-access)

    uint8_t tlvBuffer[1024];

    TLV::TLVWriter tlvWriter;
    tlvWriter.Init(tlvBuffer);
    CHIP_ERROR err = CHIP_NO_ERROR;

    TLV::TLVType outer;
    err = tlvWriter.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outer);
    ASSERT_EQ(err, CHIP_NO_ERROR);

    AttributeReportIBs::Builder builder;

    err = builder.Init(&tlvWriter, to_underlying(ReportDataMessage::Tag::kAttributeReportIBs));
    ASSERT_EQ(err, CHIP_NO_ERROR);
    AttributeValueEncoder encoder(builder, kAdminSubjectDescriptor, readRequest.path, dataVersion);

    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    Testing::SetEmberReadOutput(ByteSpan(data));

    err = model.ReadAttribute(readRequest, encoder);
    ASSERT_EQ(err, CHIP_NO_ERROR);

    builder.EndOfContainer();

    err = tlvWriter.EndContainer(outer);
    ASSERT_EQ(err, CHIP_NO_ERROR);

    err = tlvWriter.Finalize();
    ASSERT_EQ(err, CHIP_NO_ERROR);

    //// VALIDATE
    std::vector<DecodedAttributeData> attribute_data;
    err = DecodeAttributeReportIBs(ByteSpan(tlvBuffer, tlvWriter.GetLengthWritten()), attribute_data);
    ASSERT_EQ(err, CHIP_NO_ERROR);
    ASSERT_EQ(attribute_data.size(), 1u);

    const DecodedAttributeData & encodedData = attribute_data[0];
    ASSERT_EQ(encodedData.attributePath, readRequest.path);

    // data element should be a uint32 encoded as TLV
    ASSERT_EQ(encodedData.dataReader.GetType(), TLV::kTLVType_UnsignedInteger);
    uint32_t expected;
    static_assert(sizeof(expected) == sizeof(data));
    memcpy(&expected, data, sizeof(expected));
    uint32_t actual;
    err = encodedData.dataReader.Get(actual);
    ASSERT_EQ(CHIP_NO_ERROR, err);
    ASSERT_EQ(actual, expected);
}