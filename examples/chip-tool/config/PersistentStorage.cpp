/*
 *   Copyright (c) 2020 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */
#include "PersistentStorage.h"

#include <platform/KeyValueStoreManager.h>

using namespace chip::Logging;
using namespace chip::DeviceLayer::PersistedStorage;

constexpr const char kDefaultSectionName[] = "Default";
constexpr const char kPortKey[]            = "ListenPort";
constexpr const char kLoggingKey[]         = "LoggingLevel";
constexpr LogCategory kDefaultLoggingLevel = kLogCategory_Detail;

CHIP_ERROR PersistentStorage::SyncGetKeyValue(const char * key, void * value, uint16_t & size)
{
    return KeyValueStoreMgr().Get(key, value, size);
}

CHIP_ERROR PersistentStorage::SyncSetKeyValue(const char * key, const void * value, uint16_t size)
{
    return KeyValueStoreMgr().Put(key, value, size);
}

CHIP_ERROR PersistentStorage::SyncDeleteKeyValue(const char * key)
{
    return KeyValueStoreMgr().Delete(key);
}

uint16_t PersistentStorage::GetListenPort()
{
    uint16_t chipListenPort;
    if (SyncGetKeyValue(kPortKey, &chipListenPort, static_cast<uint16_t>(sizeof(chipListenPort))) == CHIP_NO_ERROR)
    {
        return chipListenPort;
    }

    // By default chip-tool listens on CHIP_PORT + 1. This is done in order to avoid
    // having 2 servers listening on CHIP_PORT when one runs an accessory server locally.
    return CHIP_PORT + 1;
}

LogCategory PersistentStorage::GetLoggingLevel()
{

    char value[9];
    uint16_t size = static_cast<uint16_t>(sizeof(value));

    if (SyncGetKeyValue(kLoggingKey, value, size) == CHIP_NO_ERROR)
    {
        if (strcasecmp(value, "none") == 0)
        {
            return kLogCategory_None;
        }
        else if (strcasecmp(value, "error") == 0)
        {
            return kLogCategory_Error;
        }
        else if (strcasecmp(value, "progress") == 0)
        {
            return kLogCategory_Progress;
        }
        else if (strcasecmp(value, "detail") == 0)
        {
            return kLogCategory_Detail;
        }
        else
        {
            ChipLogError(DeviceLayer, "Invalid/unsupported log level configuration.");
        }
    }

    return kDefaultLoggingLevel;
}
