/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include "AppMain.h"

#include <platform/CHIPDeviceLayer.h>

#include <attributes/database/interface.h>
#include <attributes/ember/interface.h>

using namespace chip::Attributes;

static void StopApp(chip::System::Layer *, void *)
{
    chip::DeviceLayer::PlatformMgr().StopEventLoopTask();
}

static void RunTests(chip::System::Layer * layer, void *)
{
    // chip::Attributes::Database *db = chip::Attributes::GetDatabase();
    static chip::Attributes::EmberDatabase ember_database;

    chip::Attributes::Database * db = &ember_database;

    ChipLogProgress(NotSpecified, "--------------------------- Starting Test ---------------------------");

    ChipLogProgress(NotSpecified, "Hardcoded endpoint loop test:");
    for (int i = 0; i < 5; i++)
    {
        auto id = db->IndexOf(Endpoint::Id(i));
        ChipLogProgress(NotSpecified, "  Id    %5d -> Index %5ld%s", i, (long) id.Raw(), id.IsValid() ? "" : " (INVALID)");

        auto idx = db->IdForPath(Endpoint::Index(i));
        ChipLogProgress(NotSpecified, "  Index %5d -> Id    %5ld%s", i, (long) idx.Raw(), idx.IsValid() ? "" : " (INVALID)");
    }

    {
        // Endpoint 65534 is a thing in all-clusters app :(
        constexpr size_t kTestId = 0xFFFE;
        auto id                  = db->IndexOf(Endpoint::Id(kTestId));
        ChipLogProgress(NotSpecified, "  Id    %5ld -> Index %5ld%s", (long) kTestId, (long) id.Raw(),
                        id.IsValid() ? "" : " (INVALID)");
    }

    const Endpoint::Index end_endpoint_index = db->EndpointEnd();

    ChipLogProgress(NotSpecified, "Endpoint count: %ld", (long) end_endpoint_index.Raw());
    for (Endpoint::Index endpoint_idx; endpoint_idx < end_endpoint_index; endpoint_idx++)
    {
        Endpoint::Id endpoint_id = db->IdForPath(endpoint_idx);
        ChipLogProgress(NotSpecified, "  Endpoint %ld has ID %ld%s", (long) endpoint_idx.Raw(), (long) endpoint_id.Raw(),
                        endpoint_id.IsValid() ? "" : " (INVALID)");
        ChipLogProgress(NotSpecified, "  Endpoint is %s", db->IsEnabled(endpoint_idx) ? "ENABLED" : "DISABLED");

        const Cluster::Index end_cluster_index = db->ClusterEnd(endpoint_idx);
        for (Cluster::Index cluster_idx; cluster_idx < end_cluster_index; cluster_idx++)
        {
            Cluster::IndexPath cluster_index_path(endpoint_idx, cluster_idx);
            Cluster::Path cluster_path = db->IdForPath(cluster_index_path);

            ChipLogProgress(NotSpecified, "    IDX %ld/%ld -> ID %ld%s/%ld%s",       //
                            (long) cluster_index_path.GetEndpoint().Raw(),           //
                            (long) cluster_index_path.GetCluster().Raw(),            //
                            (long) cluster_path.GetEndpoint().Raw(),                 //
                            cluster_path.GetEndpoint().IsValid() ? "" : "(INVALID)", //
                            (long) cluster_path.GetCluster().Raw(),                  //
                            cluster_path.GetCluster().IsValid() ? "" : "(INVALID)"   //
            );

            if (cluster_index_path != db->IndexOf(cluster_path))
            {
                ChipLogError(NotSpecified, "    Path invert check FAILED for this path !!!");
            }

            const Attribute::Index end_attribute_index = db->AttributeEnd(cluster_index_path);
            for (Attribute::Index attr_idx; attr_idx < end_attribute_index; attr_idx++)
            {
                Attribute::IndexPath attribute_index_path(cluster_index_path, attr_idx);
                Attribute::Path attribute_path = db->IdForPath(attribute_index_path);

                ChipLogProgress(NotSpecified, "      IDX %ld/%ld/%ld -> ID %ld%s/%ld%s/%ld%s", //
                                (long) attribute_index_path.GetEndpoint().Raw(),               //
                                (long) attribute_index_path.GetCluster().Raw(),                //
                                (long) attribute_index_path.GetAttribute().Raw(),              //
                                (long) attribute_path.GetEndpoint().Raw(),                     //
                                attribute_path.GetEndpoint().IsValid() ? "" : "(INVALID)",     //
                                (long) attribute_path.GetCluster().Raw(),                      //
                                attribute_path.GetCluster().IsValid() ? "" : "(INVALID)",      //
                                (long) attribute_path.GetAttribute().Raw(),                    //
                                attribute_path.GetAttribute().IsValid() ? "" : "(INVALID)"     //
                );

                if (attribute_index_path != db->IndexOf(attribute_path))
                {
                    ChipLogError(NotSpecified, "      Attr Path invert check FAILED for this path !!!");
                }
            }
        }
    }

    ChipLogProgress(NotSpecified, "--------------------------- Test DONE -------------------------------");

    layer->StartTimer(chip::System::Clock::Milliseconds32(10), StopApp, nullptr);
}

void ApplicationInit()
{
    chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(10), RunTests, nullptr);
}

void ApplicationShutdown() {}

int main(int argc, char * argv[])
{
    VerifyOrDie(ChipLinuxAppInit(argc, argv) == 0);

    // TODO: schedule test operations? How to trigger it?
    //       we may want some RPC support for this or some shell.
    ChipLinuxAppMainLoop();

    return 0;
}
