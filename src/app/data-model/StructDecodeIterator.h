/*
 *    Copyright (c) 2025 Project CHIP Authors
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

#include <lib/core/CHIPError.h>
#include <lib/core/TLVReader.h>

#include <cstdint>
#include <variant>

namespace chip {
namespace app {
namespace Clusters {
namespace detail {

// Helper  class to make structure decoding more compact for structure
class StructDecodeIterator {
  public:
    // may return a context tag, a CHIP_ERROR (end iteration)
    using EntryElement = std::variant<uint8_t, CHIP_ERROR>;

    StructDecodeIterator(TLV::TLVReader &reader) : mReader(reader){}

    // Iterate through structure elements. Returns one of:
    //   - uint8_t CONTEXT TAG (keep iterating)
    //   - CHIP_ERROR (including CHIP_NO_ERROR) which should be a final
    //     return value (stop iterating)
    EntryElement Next() {
       if (!mEntered) {
          VerifyOrReturnError(TLV::kTLVType_Structure == mReader.GetType(), CHIP_ERROR_WRONG_TLV_TYPE);
          ReturnErrorOnFailure(mReader.EnterContainer(mOuter));
          mEntered = true;
       }

       while (true) {
          CHIP_ERROR err = mReader.Next();
          if (err != CHIP_NO_ERROR) {
             VerifyOrReturnError(err == CHIP_ERROR_END_OF_TLV, err);
             break;
          }

          const TLV::Tag tag = mReader.GetTag();
          if (!TLV::IsContextTag(tag)) {
            continue;
          }

          // we know context tags are 8-bit
          return static_cast<uint8_t>(TLV::TagNumFromTag(tag));
       }

       return mReader.ExitContainer(mOuter);
    }

  private:
    bool mEntered = false;
    TLV::TLVType mOuter;
    TLV::TLVReader &mReader;
};

} // namespace detail
} // namespace Clusters
} // namespace app
} // namespace chip
