#include <lib/dnssd/IncrementalResolve.h>

#include <lib/dnssd/ServiceNaming.h>
#include <lib/dnssd/minimal_mdns/core/RecordWriter.h>
#include <lib/support/CHIPMemString.h>

namespace chip {
namespace Dnssd {

using namespace mdns::Minimal;

namespace {

enum class ServiceNameType
{
    kInvalid, // not a matter service name
    kOperational,
    kCommissioner,
    kCommissionable,
};

// Common prefix to check for all operational/commissioner/commissionable name
// parts
constexpr char kMatterNamePrefixPart[] = "_matter";

constexpr QNamePart kOperationalSuffix[]           = { kOperationalServiceName, kOperationalProtocol, kLocalDomain };
constexpr QNamePart kCommissionableSuffix[]        = { kCommissionableServiceName, kCommissionProtocol, kLocalDomain };
constexpr QNamePart kCommissionerSuffix[]          = { kCommissionerServiceName, kCommissionProtocol, kLocalDomain };
constexpr QNamePart kCommissionableSubTypeSuffix[] = { kSubtypeServiceNamePart, kCommissionableServiceName, kCommissionProtocol,
                                                       kLocalDomain };
constexpr QNamePart kCommissionerSubTypeSuffix[]   = { kSubtypeServiceNamePart, kCommissionerServiceName, kCommissionProtocol,
                                                     kLocalDomain };

ServiceNameType ComputeServiceNameType(SerializedQNameIterator name)
{
    //   <fabric>-<node>._matter._tcp.local  (operational)
    //   <instance>._matterc._udp.local  (commissionable)
    //   <instance>._matterd._udp.local  (commissioner)

    // Get fabric/node or instance name first
    // This skips over that value
    if (!name.Next() || !name.IsValid())
    {
        // missing required components - empty service name
        return ServiceNameType::kInvalid;
    }

    if (name == kOperationalSuffix)
    {
        return ServiceNameType::kOperational;
    }
    else if (name == kCommissionableSuffix)
    {
        return ServiceNameType::kCommissionable;
    }
    else if (name == kCommissionerSuffix)
    {
        return ServiceNameType::kCommissioner;
    }
    else
    {
        return ServiceNameType::kInvalid;
    }
}

/// Checks if the name is of the form <something>._sub._matter(c|d)._udp.local
bool IsCommissionSubtype(SerializedQNameIterator name)
{
    if (!name.Next() || !name.IsValid())
    {
        // subtype should be a prefix
        return false;
    }

    return (name == kCommissionerSubTypeSuffix) || (name == kCommissionableSubTypeSuffix);
}

} // namespace

CHIP_ERROR StoredServerName::Set(SerializedQNameIterator value)
{
    chip::Encoding::BigEndian::BufferWriter output(mBuffer, sizeof(mBuffer));
    RecordWriter writer(&output);

    writer.WriteQName(value);

    if (!writer.Fit())
    {
        Clear();
        return CHIP_ERROR_NO_MEMORY;
    }

    return CHIP_NO_ERROR;
}

SerializedQNameIterator StoredServerName::Get() const
{
    return SerializedQNameIterator(BytesRange(mBuffer, mBuffer + sizeof(mBuffer)), mBuffer);
}

CHIP_ERROR IncrementalResolver::InitializeParsing(mdns::Minimal::SerializedQNameIterator name, const mdns::Minimal::SrvRecord & srv)
{
    switch (ComputeServiceNameType(name))
    {
    case ServiceNameType::kOperational:
        mSpecificResolutionData.Set<OperationalNodeData>();
        {
            // Operational addresses start with peer node information
            SerializedQNameIterator nameCopy = name;
            if (!nameCopy.Next() || !nameCopy.IsValid())
            {
                mSpecificResolutionData = SpecificParseData();
                return CHIP_ERROR_INVALID_ARGUMENT;
            }

            CHIP_ERROR err =
                ExtractIdFromInstanceName(nameCopy.Value(), &mSpecificResolutionData.Get<OperationalNodeData>().peerId);
            if (err != CHIP_NO_ERROR)
            {
                mSpecificResolutionData = SpecificParseData();
                return err;
            }
        }
        break;
    case ServiceNameType::kCommissioner:
    case ServiceNameType::kCommissionable:
        mSpecificResolutionData.Set<CommissionNodeData>();
        break;
    default:
        mSpecificResolutionData = SpecificParseData();
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    mCommonResolutionData.port = srv.GetPort();

    //
    SerializedQNameIterator serverName = srv.GetName();

    // TODO: Chip code historically seems to assume that the host name is of the
    // form "<MAC or 802.15.4 Extended Address in hex>.local" and only saves the first part.
    //
    // This may not be true in the general case, however it does save some storage
    // space by only considering the prefix as the distinguishing server name
    if (!serverName.Next() || !serverName.IsValid())
    {
        mSpecificResolutionData = SpecificParseData();
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // only save the first part: the MAC or 802.15.4 Extended Address in hex
    Platform::CopyString(mCommonResolutionData.hostName, serverName.Value());

    return CHIP_NO_ERROR;
}

IncrementalResolver::RequiredInformationFlags IncrementalResolver::GetRequiredInformation() const
{
    RequiredInformationFlags flags;

    if (!mSpecificResolutionData.Valid())
    {
        flags.Set(RequiredInformation::kSrvInitialization);
    }
    else
    {
        if (mCommonResolutionData.numIPs == 0)
        {
            flags.Set(RequiredInformation::kIpAddress);
        }
    }

    return flags;
}

bool IncrementalResolver::IsSrvHostName(SerializedQNameIterator name)
{
    // FIXME: implement
    return true;
}

CHIP_ERROR IncrementalResolver::OnRecord(const ResourceData & data, BytesRange packetRange)
{
    if (!IsActive())
    {
        return CHIP_NO_ERROR; // nothing to parse
    }

    switch (data.GetType())
    {
    case QType::PTR:
        return OnPtrRecord(data, packetRange);
    case QType::TXT: {
        // TODO: find if this packet belongs to the current server
        break;
    }
    case QType::A: {
        if (!IsSrvHostName(data.GetName()))
        {
            ChipLogDetail(Discovery, "IP address received for different host name.");
            return CHIP_NO_ERROR;
        }

        Inet::IPAddress addr;
        if (!ParseARecord(data.GetData(), &addr))
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }

        return OnIpAddress(addr);
    }
    case QType::AAAA: {
        if (!IsSrvHostName(data.GetName()))
        {
            ChipLogDetail(Discovery, "IP address received for different host name.");
            return CHIP_NO_ERROR;
        }

        Inet::IPAddress addr;
        if (!ParseAAAARecord(data.GetData(), &addr))
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }

        return OnIpAddress(addr);
    }
    case QType::SRV: // SRV handled on creation, ignored for 'additional data'
    default:
        // Other types not interesting during parsing
        return CHIP_NO_ERROR;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR IncrementalResolver::OnPtrRecord(const ResourceData & data, BytesRange packetRange)
{
    // Here we handle subtype expectations. Data is of the form:
    // <subtype>._sub._mattrc._udp.local or
    // <subtype>._sub._mattrd._udp.local
    //
    // If these hold, then we have to check if PTR points at the current record and
    // if yes, the subtype matches and information can be extracted.

    if (!IsActiveCommissionParse())
    {
        ChipLogDetail(Discovery, "PTR record for non-commission resolution.");
        return CHIP_NO_ERROR;
    }

    if (!IsCommissionSubtype(data.GetName()))
    {
        ChipLogDetail(Discovery, "PTR record that is not a sub-type for commissioning.");
        return CHIP_NO_ERROR;
    }

    SerializedQNameIterator qname;

    if (!ParsePtrRecord(data.GetData(), packetRange, &qname))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // TODO: why are we not validating the string here? what is the purpose
    // of copying and preserving the instance name here?
    Platform::CopyString(mSpecificResolutionData.Get<CommissionNodeData>().instanceName, qname.Value());

    // TODO:
    //    - can we attach sub data information to the packet? Is it parseable?

    return CHIP_NO_ERROR;
}

CHIP_ERROR IncrementalResolver::OnTxtRecord(const ResourceData & data, BytesRange packetRange)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR IncrementalResolver::OnIpAddress(const Inet::IPAddress & addr)
{
    if (mCommonResolutionData.numIPs >= ArraySize(mCommonResolutionData.ipAddress))
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    mCommonResolutionData.ipAddress[mCommonResolutionData.numIPs++] = addr;
    return CHIP_NO_ERROR;
}

} // namespace Dnssd
} // namespace chip
