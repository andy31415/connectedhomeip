#include <lib/dnssd/IncrementalResolve.h>
#include <lib/dnssd/ServiceNaming.h>

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

constexpr QNamePart kOperationalSuffix[]   = { kOperationalServiceName, kOperationalProtocol, kLocalDomain };
constexpr QNamePart kCommisionableSuffix[] = { kCommissionableServiceName, kCommissionProtocol, kLocalDomain };
constexpr QNamePart kCommissionerSuffix[]  = { kCommissionerServiceName, kCommissionProtocol, kLocalDomain };

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
    else if (name == kCommisionableSuffix)
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

} // namespace

CHIP_ERROR IncrementalResolver::InitializeParsing(mdns::Minimal::SerializedQNameIterator name, const mdns::Minimal::SrvRecord & srv)
{
    switch (ComputeServiceNameType(name))
    {
    case ServiceNameType::kOperational:
        mSpecificResolutionData.Set<OperationalNodeData>();

        // TODO: parse name
        break;
    case ServiceNameType::kCommissioner:
    case ServiceNameType::kCommissionable:
        mSpecificResolutionData.Set<CommissionNodeData>();

        // TODO: parse instance name
        break;
    default:
        mSpecificResolutionData = SpecificParseData();
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    mCommonResolutionData.port = srv.GetPort();
    // FIXME: Grab the reference for AAAA

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

CHIP_ERROR IncrementalResolver::OnRecord(const mdns::Minimal::ResourceData & data)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR IncrementalResolver::OnPtrRecord(const mdns::Minimal::ResourceData & data)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR IncrementalResolver::OnTxtRecord(const mdns::Minimal::ResourceData & data)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR IncrementalResolver::OnIpAddress(mdns::Minimal::SerializedQNameIterator name, const Inet::IPAddress & addr)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Dnssd
} // namespace chip
