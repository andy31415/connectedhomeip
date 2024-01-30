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
#pragma once

#include <attributes/database/paths.h>
#include <attributes/database/type_safe_id.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

namespace chip {
namespace Attributes {

/// Represents possible data types for attributes.
///
/// Generally this is auto-generated from ember attribute types.
/// `zcl/data-model/chip/chip-types.xml` or from the spec.
///
/// TODO: Actual values in this array are generally expected to be abstracted away
///       as these seem hard-coded implementation specvific types without strong
///       backing from the specification (e.g. no reason to have char and long_char)
enum class DataType : uint8_t
{
    kNoDataAttributeType          = 0x00, // No data
    kBooleanAttributeType         = 0x10, // Boolean
    kBitmap8AttributeType         = 0x18, // 8-bit bitmap
    kBitmap16AttributeType        = 0x19, // 16-bit bitmap
    kBitmap32AttributeType        = 0x1B, // 32-bit bitmap
    kBitmap64AttributeType        = 0x1F, // 64-bit bitmap
    kInt8UAttributeType           = 0x20, // Unsigned 8-bit integer
    kInt16UAttributeType          = 0x21, // Unsigned 16-bit integer
    kInt24UAttributeType          = 0x22, // Unsigned 24-bit integer
    kInt32UAttributeType          = 0x23, // Unsigned 32-bit integer
    kInt40UAttributeType          = 0x24, // Unsigned 40-bit integer
    kInt48UAttributeType          = 0x25, // Unsigned 48-bit integer
    kInt56UAttributeType          = 0x26, // Unsigned 56-bit integer
    kInt64UAttributeType          = 0x27, // Unsigned 64-bit integer
    kInt8SAttributeType           = 0x28, // Signed 8-bit integer
    kInt16SAttributeType          = 0x29, // Signed 16-bit integer
    kInt24SAttributeType          = 0x2A, // Signed 24-bit integer
    kInt32SAttributeType          = 0x2B, // Signed 32-bit integer
    kInt40SAttributeType          = 0x2C, // Signed 40-bit integer
    kInt48SAttributeType          = 0x2D, // Signed 48-bit integer
    kInt56SAttributeType          = 0x2E, // Signed 56-bit integer
    kInt64SAttributeType          = 0x2F, // Signed 64-bit integer
    kEnum8AttributeType           = 0x30, // 8-bit enumeration
    kEnum16AttributeType          = 0x31, // 16-bit enumeration
    kPriorityAttributeType        = 0x32, // Priority
    kStatusAttributeType          = 0x33, // Status Code
    kSingleAttributeType          = 0x39, // Single precision
    kDoubleAttributeType          = 0x3A, // Double precision
    kOctetStringAttributeType     = 0x41, // Octet String
    kCharStringAttributeType      = 0x42, // Character String
    kLongOctetStringAttributeType = 0x43, // Long Octet String
    kLongCharStringAttributeType  = 0x44, // Long Character String
    kArrayAttributeType           = 0x48, // List
    kStructAttributeType          = 0x4C, // Structure
    kGroupIdAttributeType         = 0xC0, // Group ID
    kEndpointNoAttributeType      = 0xC1, // Endpoint Number
    kVendorIdAttributeType        = 0xC2, // Vendor ID
    kDevtypeIdAttributeType       = 0xC3, // Device Type ID
    kFabricIdAttributeType        = 0xC4, // Fabric ID
    kFabricIdxAttributeType       = 0xC5, // Fabric Index
    kEntryIdxAttributeType        = 0xC6, // Entry Index
    kDataVerAttributeType         = 0xC7, // Data Version
    kEventNoAttributeType         = 0xC8, // Event Number
    kSemtagAttributeType          = 0xC9, // Semantic Tag
    kNamespaceAttributeType       = 0xCA, // Namespace
    kTagAttributeType             = 0xCB, // Tag
    kSystimeUsAttributeType       = 0xD0, // System Time Microseconds
    kSystimeMsAttributeType       = 0xD1, // System Time Milliseconds
    kElapsedSAttributeType        = 0xD2, // Elapsed Time Seconds
    kTemperatureAttributeType     = 0xD8, // Temperature
    kPowerMwAttributeType         = 0xD9, // Power milliwatts
    kAmperageMaAttributeType      = 0xDA, // Amperage milliamps
    kVoltageMvAttributeType       = 0xDB, // Voltage millivolts
    kEnergyMwhAttributeType       = 0xDC, // Energy milliwatt-hours
    kTodAttributeType             = 0xE0, // Time of day
    kDateAttributeType            = 0xE1, // Date
    kEpochUsAttributeType         = 0xE3, // Epoch Microseconds
    kEpochSAttributeType          = 0xE4, // Epoch Seconds
    kPosixMsAttributeType         = 0xE5, // Posix Time Milliseconds
    kPercentAttributeType         = 0xE6, // Percentage units 1%
    kPercent100ThsAttributeType   = 0xE7, // Percentage units 0.01%
    kClusterIdAttributeType       = 0xE8, // Cluster ID
    kAttribIdAttributeType        = 0xE9, // Attribute ID
    kFieldIdAttributeType         = 0xEB, // Field ID
    kEventIdAttributeType         = 0xEC, // Event ID
    kCommandIdAttributeType       = 0xED, // Command ID
    kActionIdAttributeType        = 0xEE, // Action ID
    kTransIdAttributeType         = 0xEF, // Transaction ID
    kNodeIdAttributeType          = 0xF0, // Node ID
    kIpadrAttributeType           = 0xF2, // IP Address
    kIpv4AdrAttributeType         = 0xF3, // IPv4 Address
    kIpv6AdrAttributeType         = 0xF4, // IPv6 Address
    kIpv6PreAttributeType         = 0xF5, // IPv6 Prefix
    kHwadrAttributeType           = 0xF6, // Hardware Address
    kUnknownAttributeType         = 0xFF, // Unknown
};

/// Defines a generic interface to access some abstract storage for cluster
/// data.
///
/// The current design assumes a flat index-driven design, however the intent
/// for the future is for this to eventually be updated to be more dynamic.
///
/// NOTE: usage and iteration by indices does not work well with dynamic clusters
///       and as such should be used sparringly as it is subject to change
///
///       A path generally uniquely identifies a endpoint/cluster/attribute (i.e. no
///       endpoint contains two copies of the same cluster and no cluster contains two
///       instances of the same attribute)
class Database
{
public:
    enum class WriteType {
        kNormal,       // validates data types and read-only restrictions
        kNoValidation, // allows writing of data even if exposed externally as read-only.
    };

    virtual ~Database() = default;

    virtual Endpoint::Index IndexOf(Endpoint::Id)         = 0;
    virtual Cluster::IndexPath IndexOf(Cluster::Path)     = 0;
    virtual Attribute::IndexPath IndexOf(Attribute::Path) = 0;

    virtual Endpoint::Id IdForPath(Endpoint::Index)         = 0;
    virtual Cluster::Path IdForPath(Cluster::IndexPath)     = 0;
    virtual Attribute::Path IdForPath(Attribute::IndexPath) = 0;

    /// Returns an index that is one past the maximum index
    /// facilitates iteration over all available items in a group.
    virtual Endpoint::Index EndpointEnd()                     = 0;
    virtual Cluster::Index ClusterEnd(Endpoint::Index)        = 0;
    virtual Attribute::Index AttributeEnd(Cluster::IndexPath) = 0;

    /// When iterating over items, one can check if a specific item is enabled
    virtual bool IsEnabled(Endpoint::Id)    = 0;
    virtual bool IsEnabled(Endpoint::Index) = 0;

    /// Read the attribute at the given `path`.
    ///
    /// Returns the value inside `data` (which will be resized to the right size) and provides
    /// the `type` that was used to encode as binary data into `data`.
    virtual CHIP_ERROR Read(Attribute::Path path, MutableByteSpan & data, DataType & type) = 0;

    /// Write the attribute at the specified `path`.
    ///
    /// The `data` contains the binary data to write while `type` contains what type of data is stored
    /// within the byte buffer.
    virtual CHIP_ERROR Write(Attribute::Path path, WriteType write_type, MutableByteSpan data, DataType type) = 0;
};

/// Singleton implementation for fetching the instances of the active attribute database
Database * GetDatabase();

/// Sets the attribute database to a new value.
///
/// Returns the old value if one had been set before (or nullptr if the database has not been initialized)
Database * SetDatabase(Database * new_value);

} // namespace Attributes
} // namespace chip
