/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include <platform/CHIPDeviceLayer.h>
#include <support/CodeUtils.h>
#include <support/ErrorStr.h>
#include <support/TestUtils.h>

#include <logging/log.h>
#include <settings/settings.h>

#include <dlfcn.h>

using namespace ::chip;
using namespace ::chip::DeviceLayer;

LOG_MODULE_REGISTER(runner);

static int RunTests(const char * libname)
{
    void * handle = dlopen(libname, RTLD_NOW);
    if (handle == nullptr)
    {
        LOG_INF("Open failed");
        return 1;
    }

    void * sym = dlsym(handle, "RunTests");
    if (sym == nullptr)
    {
        LOG_INF("Function not found");
        dlclose(handle);
        return 1;
    }
    int (*func)() = (int (*)()) sym;
    int status    = func();

    dlclose(handle);
    return status;
}

void main(int argc, const char ** argv)
{
    int status;
    VerifyOrDie(settings_subsys_init() == 0);

    LOG_INF("Starting CHIP tests!");
    for (int i = 0; i < argc; i++)
    {
        LOG_INF("Argument %d: %s\n", i, argv[i]);
    }

    // if (argc != 1)
    // {
    //     printf("First argument: %s", argv[1]);
    //     status = RunTests(argv[1]);
    // }
    // else
    // {
    status = RunRegisteredUnitTests();
    //  }

    LOG_INF("CHIP test status: %d", status);
    exit(status);
}
