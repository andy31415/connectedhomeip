#include <lib/dnssd/IncrementalResolve.h>

namespace chip {
namespace Dnssd {

CHIP_ERROR IncrementalResolver::InitializeParsing(mdns::Minimal::SerializedQNameIterator name, const mdns::Minimal::SrvRecord & srv)
{
    // FIXME: Implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
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
