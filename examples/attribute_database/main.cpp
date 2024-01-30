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

    for (int i = 0; i < 5; i++)
    {
        auto id = db->IndexOf(Endpoint::Id(i));
        ChipLogProgress(NotSpecified, "Id    %5d -> Index %5d%s", i, (int) id.Raw(), id.IsValid() ? "" : " (INVALID)");

        auto idx = db->IdForPath(Endpoint::Index(i));
        ChipLogProgress(NotSpecified, "Index %5d -> Id    %5d%s", i, (int) idx.Raw(), idx.IsValid() ? "" : " (INVALID)");
    }

    {
        // Endpoint 65534 is a thing in all-clusters app :(
        constexpr size_t kTestId = 0xFFFE;
        auto id                  = db->IndexOf(Endpoint::Id(kTestId));
        ChipLogProgress(NotSpecified, "Id    %5d -> Index %5d (%s)", (int) kTestId, (int) id.Raw(),
                        id.IsValid() ? "VALID" : "INVALID");
    }

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
