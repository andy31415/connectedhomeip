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

#include <cmdline.h>
#include <dlfcn.h>

void native_get_test_cmd_line_args(int * argc, char *** argv);

using namespace ::chip;
using namespace ::chip::DeviceLayer;

LOG_MODULE_REGISTER(runner);

namespace {

int RunTests(const char * libname)
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

} // namespace

void zephyr_app_main()
{
    int status;
    VerifyOrDie(settings_subsys_init() == 0);

    int argc;
    char ** argv;
    native_get_test_cmd_line_args(&argc, &argv);

    LOG_INF("Starting CHIP tests: arguments!");
    for (int i = 0; i < argc; i++)
    {
        LOG_INF("Test Argument %d: %s\n", i, argv[i]);
    }

    if (argc == 1)
    {
        printf("First argument: %s", argv[0]);
        status = RunTests(argv[0]);
    }
    else
    {
        status = RunRegisteredUnitTests();
    }

    LOG_INF("CHIP test status: %d", status);
    exit(status);
}
