/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
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

#include <lib/dnssd/Resolver.h>
#include <lib/dnssd/minimal_mdns/Parser.h>
#include <lib/dnssd/minimal_mdns/RecordData.h>
#include <lib/dnssd/minimal_mdns/core/QName.h>
#include <lib/support/BitFlags.h>
#include <lib/support/Variant.h>

namespace chip {
namespace Dnssd {

/// Incrementally accumulates data from DNSSD packets. It is generally geared
/// towards processing minmdns records.
///
/// It all starts with processing SRV records which define the type of record
/// (could be operational, commissionable or commissioner), after which the
/// additional data is accumulated, specifically TXT information and A/AAAA
///
/// Class can also be used to determine what additional data is missing from a
/// record so that additional DNSSD queries can be made recursively (e.g. if
/// only a SRV/TXT records are available, ask for AAAA records).
class IncrementalResolver
{
public:
    // Elements that the incremental resolve still needs
    enum class RequiredInformation : uint8_t
    {
        kSrvInitialization = 0x01, // server being initialized
        kIpAddress         = 0x02, // IP address missing
    };
    using RequiredInformationFlags = BitFlags<RequiredInformation>;

    IncrementalResolver() {}

    /// Checks if object has been initialized using the `InitializeParsing`
    /// method.
    bool IsActive() const { return mSpecificResolutionData.Valid(); }

    bool IsActiveCommissionParse() const { return mSpecificResolutionData.Is<CommissionNodeData>(); }
    bool IsActiveOperationalParse() const { return mSpecificResolutionData.Is<OperationalNodeData>(); }

    /// Start parsing a new record. SRV records are the records CHIP is mainly
    /// interested on, after which TXT and A/AAAA are looked for.
    CHIP_ERROR InitializeParsing(mdns::Minimal::SerializedQNameIterator name, const mdns::Minimal::SrvRecord & srv);

    /// Notify that a new record is being processed
    /// Will handle filtering and processing of data to determine
    /// if the entry is relevant for the current resolver.
    CHIP_ERROR OnRecord(const mdns::Minimal::ResourceData & data);

    /// Return what additional data is required until the object can be extracted
    ///
    /// If `!GetREquiredInformation().HasAny()` the parsed information is ready
    /// to be processed.
    RequiredInformationFlags GetRequiredInformation() const;

    // TODO:
    //   - clear current state and extract actual commission
    //     data result
    //   - have some for of timeout for processing (here? can we separate?)

private:
    /// Notify that a PTR record can be parsed.
    ///
    /// Input data MUST have GetType() == QType::PTR
    CHIP_ERROR OnPtrRecord(const mdns::Minimal::ResourceData & data);

    /// Notify that a PTR record can be parsed.
    ///
    /// Input data MUST have GetType() == QType::TXT
    CHIP_ERROR OnTxtRecord(const mdns::Minimal::ResourceData & data);

    /// Notify that a new IP addres has been found for the given name.
    /// This is to be called on both A (if IPv4 support is enabled) and AAAA
    /// addresses.
    CHIP_ERROR OnIpAddress(mdns::Minimal::SerializedQNameIterator name, const Inet::IPAddress & addr);

    using SpecificParseData = Variant<OperationalNodeData, CommissionNodeData>;

    CommonResolutionData mCommonResolutionData;
    SpecificParseData mSpecificResolutionData;
};

} // namespace Dnssd
} // namespace chip