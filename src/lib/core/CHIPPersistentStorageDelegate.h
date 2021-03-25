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

#pragma once

#include <core/CHIPCore.h>
#include <support/DLLUtil.h>

namespace chip {

class DLL_EXPORT PersistentStorageDelegate
{
public:
    virtual ~PersistentStorageDelegate() {}

    /**
     * @brief
     *   This is a synchronous Get API, where the value is returned via the output
     *   buffer. This API should be used sparingly, since it may block for
     *   some duration.
     *
     * @param[in]      key Key to lookup
     * @param[out]     value Value for the key.  This will always be
     *                 null-terminated if the function succeeds.
     * @param[in, out] size Input value buffer size, output size of buffer
     *                 needed to store the value.
     *                 For null-terminated strings, this will include the
     *                 '\0' at the end.
     *
     *                 The output size could be larger than input value. In
     *                 such cases, the user should allocate the buffer large
     *                 enough (>= output size), and call the API again.  In this
     *                 case SyncGetKeyValue will place as many bytes as it can in
     *                 the buffer and return CHIP_ERROR_NO_MEMORY.
     *
     *                 If value is null, the input size is treated as 0.
     *
     * @return CHIP_ERROR_KEY_NOT_FOUND there is no value for the given key.
     * @return CHIP_ERROR_NO_MEMORY if the input buffer is not big enough for
     *                              the value.
     */
    virtual CHIP_ERROR SyncGetKeyValue(const char * key, void * value, uint16_t & size) = 0;

    /**
     * @brief
     *   Set the value for the key to a byte buffer.
     *
     * @param[in] key Key to be set
     * @param[in] value Value to be set
     * @param[in] size Size of the Value
     */
    virtual CHIP_ERROR SyncSetKeyValue(const char * key, const void * value, uint16_t size) = 0;

    /**
     * @brief
     *   Deletes the value for the key
     *
     * @param[in] key Key to be deleted
     */
    virtual CHIP_ERROR SyncDeleteKeyValue(const char * key) = 0;
};

} // namespace chip
