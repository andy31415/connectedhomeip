/*
 *
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

#include "lib/core/CHIPError.h"
#include "protocols/interaction_model/StatusCode.h"
#include "pw_unit_test/framework_backend.h"
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/StringBuilderAdapters.h>
#include <lib/support/CodeUtils.h>

#include <pw_unit_test/framework.h>

using chip::app::DataModel::ActionReturnStatus;
using chip::Protocols::InteractionModel::ClusterStatusCode;
using chip::Protocols::InteractionModel::Status;

TEST(TestActionReturnStatus, TestEquality)
{
    // equality should happen between equivalent statuses and chip_errors
    ASSERT_EQ(ActionReturnStatus(Status::UnsupportedRead), Status::UnsupportedRead);
    ASSERT_EQ(ActionReturnStatus(Status::UnsupportedWrite), CHIP_IM_GLOBAL_STATUS(UnsupportedWrite));

    ASSERT_EQ(ActionReturnStatus(CHIP_IM_GLOBAL_STATUS(Busy)), Status::Busy);
    ASSERT_EQ(ActionReturnStatus(CHIP_IM_GLOBAL_STATUS(Busy)), CHIP_IM_GLOBAL_STATUS(Busy));

    ASSERT_EQ(ActionReturnStatus(CHIP_IM_CLUSTER_STATUS(123)), CHIP_IM_CLUSTER_STATUS(123));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificFailure(123)), CHIP_IM_CLUSTER_STATUS(123));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificFailure(123)), ClusterStatusCode::ClusterSpecificFailure(123));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificSuccess(123)), ClusterStatusCode::ClusterSpecificSuccess(123));
}

TEST(TestActionReturnStatus, TestIsError)
{
    ASSERT_TRUE(ActionReturnStatus(CHIP_IM_CLUSTER_STATUS(123)).IsError());
    ASSERT_TRUE(ActionReturnStatus(CHIP_ERROR_INTERNAL).IsError());
    ASSERT_TRUE(ActionReturnStatus(CHIP_ERROR_NO_MEMORY).IsError());
    ASSERT_TRUE(ActionReturnStatus(Status::UnsupportedRead).IsError());
    ASSERT_TRUE(ActionReturnStatus(ClusterStatusCode::ClusterSpecificFailure(123)).IsError());

    ASSERT_FALSE(ActionReturnStatus(Status::Success).IsError());
    ASSERT_FALSE(ActionReturnStatus(ClusterStatusCode::ClusterSpecificSuccess(123)).IsError());
    ASSERT_FALSE(ActionReturnStatus(CHIP_NO_ERROR).IsError());
}

TEST(TestActionReturnStatus, TestUnderlyingError)
{
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificFailure(123)).GetUnderlyingError(), CHIP_IM_CLUSTER_STATUS(123));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificSuccess(123)).GetUnderlyingError(), CHIP_NO_ERROR);
    ASSERT_EQ(ActionReturnStatus(Status::Busy).GetUnderlyingError(), CHIP_IM_GLOBAL_STATUS(Busy));
    ASSERT_EQ(ActionReturnStatus(CHIP_ERROR_INTERNAL).GetUnderlyingError(), CHIP_ERROR_INTERNAL);
}

TEST(TestActionReturnStatus, TestStatusCode)
{
    ASSERT_EQ(ActionReturnStatus(CHIP_ERROR_INTERNAL).GetStatusCode(), ClusterStatusCode(Status::Failure));
    ASSERT_EQ(ActionReturnStatus(Status::Busy).GetStatusCode(), ClusterStatusCode(Status::Busy));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificSuccess(123)).GetStatusCode(),
              ClusterStatusCode::ClusterSpecificSuccess(123));
    ASSERT_EQ(ActionReturnStatus(ClusterStatusCode::ClusterSpecificFailure(123)).GetStatusCode(),
              ClusterStatusCode::ClusterSpecificFailure(123));
    ASSERT_EQ(ActionReturnStatus(CHIP_IM_CLUSTER_STATUS(0x12)).GetStatusCode(), ClusterStatusCode::ClusterSpecificFailure(0x12));
    ASSERT_EQ(ActionReturnStatus(CHIP_IM_GLOBAL_STATUS(Timeout)).GetStatusCode(), ClusterStatusCode(Status::Timeout));
}