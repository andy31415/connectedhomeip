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

#include "Server.h"

#include <errno.h>
#include <utility>

#include <lib/dnssd/minimal_mdns/core/DnsHeader.h>
#include <platform/CHIPDeviceLayer.h>

namespace mdns {
namespace Minimal {
namespace {

class ShutdownOnError
{
public:
    ShutdownOnError(Server * s) : mServer(s) {}
    ~ShutdownOnError()
    {
        if (mServer != nullptr)
        {
            mServer->Shutdown();
        }
    }

    CHIP_ERROR ReturnSuccess()
    {
        mServer = nullptr;
        return CHIP_NO_ERROR;
    }

private:
    Server * mServer;
};

} // namespace

namespace BroadcastIpAddresses {

// Get standard mDNS Broadcast addresses

void GetIpv6Into(chip::Inet::IPAddress & dest)
{
    if (!chip::Inet::IPAddress::FromString("FF02::FB", dest))
    {
        ChipLogError(Discovery, "Failed to parse standard IPv6 broadcast address");
    }
}

void GetIpv4Into(chip::Inet::IPAddress & dest)
{
    if (!chip::Inet::IPAddress::FromString("224.0.0.251", dest))
    {
        ChipLogError(Discovery, "Failed to parse standard IPv4 broadcast address");
    }
}

} // namespace BroadcastIpAddresses

namespace {

CHIP_ERROR JoinMulticastGroup(chip::Inet::InterfaceId interfaceId, chip::Inet::UDPEndPoint * endpoint,
                              chip::Inet::IPAddressType addressType)
{

    chip::Inet::IPAddress address;

    if (addressType == chip::Inet::IPAddressType::kIPv6)
    {
        BroadcastIpAddresses::GetIpv6Into(address);
#if INET_CONFIG_ENABLE_IPV4
    }
    else if (addressType == chip::Inet::IPAddressType::kIPv4)
    {
        BroadcastIpAddresses::GetIpv4Into(address);
#endif // INET_CONFIG_ENABLE_IPV4
    }
    else
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    return endpoint->JoinMulticastGroup(interfaceId, address);
}

#if CHIP_ERROR_LOGGING
const char * AddressTypeStr(chip::Inet::IPAddressType addressType)
{
    switch (addressType)
    {
    case chip::Inet::IPAddressType::kIPv6:
        return "IPv6";
#if INET_CONFIG_ENABLE_IPV4
    case chip::Inet::IPAddressType::kIPv4:
        return "IPv4";
#endif // INET_CONFIG_ENABLE_IPV4
    default:
        return "UNKNOWN";
    }
}
#endif

} // namespace

Server::~Server()
{
    Shutdown();
}

void Server::Shutdown()
{
    ShutdownEndpoints();
    mIsInitialized = false;
}

void Server::ShutdownEndpoints()
{
    if (mIpv6Endpoint != nullptr)
    {
        mIpv6Endpoint->Free();
        mIpv6Endpoint = nullptr;
    }

#if INET_CONFIG_ENABLE_IPV4
    if (mIpv4Endpoint != nullptr)
    {
        mIpv4Endpoint->Free();
        mIpv4Endpoint = nullptr;
    }
#endif
}

bool Server::IsListening() const
{
    // technically we have a IPv4 endpoint as well, however that
    // is only optional. The IPv6 endpoint is what we care about.
    return (mIpv6Endpoint != nullptr);
}

CHIP_ERROR Server::Listen(chip::Inet::EndPointManager<chip::Inet::UDPEndPoint> * udpEndPointManager, ListenIterator * it,
                          uint16_t port)
{
    ShutdownEndpoints(); // ensure everything starts fresh

    chip::Inet::InterfaceId interfaceId = chip::Inet::InterfaceId::Null();
    chip::Inet::IPAddressType addressType;

    ShutdownOnError autoShutdown(this);

    // Listen for IPv6 (always)
    ReturnErrorOnFailure(udpEndPointManager->NewEndPoint(&mIpv6Endpoint));
    ReturnErrorOnFailure(mIpv6Endpoint->Bind(chip::Inet::IPAddressType::kIPv6, chip::Inet::IPAddress::Any, port));
    ReturnErrorOnFailure(mIpv6Endpoint->Listen(OnUdpPacketReceived, nullptr /*OnReceiveError*/, this));

#if INET_CONFIG_ENABLE_IPV4
    // Also listen for IPv4 (if IPv4 is enabled)
    ReturnErrorOnFailure(udpEndPointManager->NewEndPoint(&mIpv4Endpoint));
    ReturnErrorOnFailure(mIpv4Endpoint->Bind(chip::Inet::IPAddressType::kIPv4, chip::Inet::IPAddress::Any, port));
    ReturnErrorOnFailure(mIpv4Endpoint->Listen(OnUdpPacketReceived, nullptr /*OnReceiveError*/, this));
#endif

    mListenInterfacesCount = 0;

    // ensure we are in the multicast groups required
    while (it->Next(&interfaceId, &addressType))
    {
        CHIP_ERROR err = JoinMulticastGroup(interfaceId,
#if INET_CONFIG_ENABLE_IPV4
                                            (addressType == chip::Inet::IPAddressType::kIPv4) ? mIpv4Endpoint :
#endif
                                                                                              mIpv6Endpoint,
                                            addressType);

        if (err != CHIP_NO_ERROR)
        {
            char interfaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
            interfaceId.GetInterfaceName(interfaceName, sizeof(interfaceName));

            // Log only as non-fatal error. We may be able to join to different multicast groups, so this error is
            // not fatal
            ChipLogError(DeviceLayer, "MDNS failed to join multicast group on %s for address type %s: %" CHIP_ERROR_FORMAT,
                         interfaceName, AddressTypeStr(addressType), err.Format());
            continue;
        }
        if (mListenInterfacesCount < sizeof(mListenInterfaces) / sizeof(mListenInterfaces[0]))
        {
            mListenInterfaces[mListenInterfacesCount].interfaceId = interfaceId;
            mListenInterfaces[mListenInterfacesCount].addressType = addressType;
            mListenInterfacesCount++;
        }
        else
        {
            char interfaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
            interfaceId.GetInterfaceName(interfaceName, sizeof(interfaceName));

            ChipLogError(DeviceLayer, "Insufficient resources to keep track of listen interfaces on %s / %s", interfaceName,
                         AddressTypeStr(addressType));
        }
    }

    if (!mIsInitialized)
    {
#if !CHIP_DEVICE_LAYER_NONE
        chip::DeviceLayer::ChipDeviceEvent event{};
        event.Type = chip::DeviceLayer::DeviceEventType::kDnssdInitialized;
        chip::DeviceLayer::PlatformMgr().PostEventOrDie(&event);
#endif
        mIsInitialized = true;
    }

    return autoShutdown.ReturnSuccess();
}

CHIP_ERROR Server::DirectSend(chip::System::PacketBufferHandle && data, const chip::Inet::IPAddress & addr, uint16_t port,
                              chip::Inet::InterfaceId interface)
{
#if INET_CONFIG_ENABLE_IPV4
    if (addr.Type() == chip::Inet::IPAddressType::kIPv4)
    {
        VerifyOrReturnError(mIpv4Endpoint != nullptr, CHIP_ERROR_NOT_CONNECTED);
        return mIpv4Endpoint->SendTo(addr, port, std::move(data));
    }
#endif

    VerifyOrReturnError(mIpv6Endpoint != nullptr, CHIP_ERROR_NOT_CONNECTED);
    return mIpv6Endpoint->SendTo(addr, port, std::move(data));
}

CHIP_ERROR Server::BroadcastUnicastQuery(chip::System::PacketBufferHandle && data, uint16_t port)
{
    return BroadcastImpl(std::move(data), port);
}

CHIP_ERROR Server::BroadcastUnicastQuery(chip::System::PacketBufferHandle && data, uint16_t port, chip::Inet::InterfaceId interface,
                                         chip::Inet::IPAddressType addressType)
{
    // FIXME: send to specific interface and address type ONLY!
    return BroadcastImpl(std::move(data), port);
}

CHIP_ERROR Server::BroadcastSend(chip::System::PacketBufferHandle && data, uint16_t port, chip::Inet::InterfaceId interface,
                                 chip::Inet::IPAddressType addressType)
{
    // FIXME: send to specific interface and address type ONLY!
    return BroadcastImpl(std::move(data), port);
}

CHIP_ERROR Server::BroadcastSend(chip::System::PacketBufferHandle && data, uint16_t port)
{
    return BroadcastImpl(std::move(data), port);
}

bool Server::IsListeningOn(const chip::Inet::InterfaceId interfaceId, chip::Inet::IPAddressType addressType)
{

    for (size_t i = 0; i < mListenInterfacesCount; i++)
    {
        const ListenInterfaceInfo info = mListenInterfaces[i];

        if ((info.interfaceId == interfaceId) && (info.addressType == addressType))
        {
            return true;
        }
    }

    return false;
}

CHIP_ERROR Server::BroadcastImpl(chip::System::PacketBufferHandle && data, uint16_t port)
{
    // Broadcast requires sending data multiple times, each of which may error
    // out, yet broadcast only has a single error code.
    //
    // The general logic of error handling is:
    //   - if no send done at all, return error
    //   - if at least one broadcast succeeds, assume success overall
    //   + some internal consistency validations for state error.
    unsigned successes   = 0;
    unsigned failures    = 0;
    CHIP_ERROR lastError = CHIP_ERROR_NO_ENDPOINT;

    for (size_t i = 0; i < mListenInterfacesCount; i++)
    {
        const ListenInterfaceInfo info = mListenInterfaces[i];
        CHIP_ERROR err                 = CHIP_NO_ERROR;

        /// The same packet needs to be sent over potentially multiple interfaces.
        /// LWIP does not like having a pbuf sent over serparate interfaces, hence we create a copy
        /// for sending via `CloneData`
        ///
        /// TODO: this wastes one copy of the data and that could be optimized away
        chip::System::PacketBufferHandle tempBuf = data.CloneData();
        if (tempBuf.IsNull())
        {
            // Not enough memory available to clone pbuf
            err = CHIP_ERROR_NO_MEMORY;
        }
        else if (info.addressType == chip::Inet::IPAddressType::kIPv6)
        {
            err = mIpv6Endpoint->SendTo(mIpv6BroadcastAddress, port, std::move(tempBuf), info.interfaceId);
        }
#if INET_CONFIG_ENABLE_IPV4
        else if (info.addressType == chip::Inet::IPAddressType::kIPv4)
        {
            err = mIpv4Endpoint->SendTo(mIpv4BroadcastAddress, port, std::move(tempBuf), info.interfaceId);
        }
#endif
        else
        {
            // This is a general error of internal consistency: every address has a known type. Fail completely otherwise.
            return CHIP_ERROR_INCORRECT_STATE;
        }

        if (err == CHIP_NO_ERROR)
        {
            successes++;
        }
        else
        {
            failures++;
            lastError = err;
#if CHIP_DETAIL_LOGGING
            char ifaceName[chip::Inet::InterfaceId::kMaxIfNameLength];
            err = info.interfaceId.GetInterfaceName(ifaceName, sizeof(ifaceName));
            if (err != CHIP_NO_ERROR)
            {
                strcpy(ifaceName, "???");
            }
            ChipLogDetail(Discovery, "Warning: Attempt to mDNS broadcast failed on %s: %s", ifaceName, lastError.AsString());
#endif
        }
    }

    if (failures != 0)
    {
        // if we had failures, log if the final status was success or failure, to make log reading
        // easier. Some mDNS failures may be expected (e.g. for interfaces unavailable)
        if (successes != 0)
        {
            ChipLogDetail(Discovery, "mDNS broadcast had only partial success: %u successes and %u failures.", successes, failures);
        }
        else
        {
            ChipLogProgress(Discovery, "mDNS broadcast full failed in %u separate send attempts.", failures);
        }
    }

    if (!successes)
    {
        return lastError;
    }

    return CHIP_NO_ERROR;
}

void Server::OnUdpPacketReceived(chip::Inet::UDPEndPoint * endPoint, chip::System::PacketBufferHandle && buffer,
                                 const chip::Inet::IPPacketInfo * info)
{
    Server * srv = static_cast<Server *>(endPoint->mAppState);
    if (!srv->mDelegate)
    {
        return;
    }

    mdns::Minimal::BytesRange data(buffer->Start(), buffer->Start() + buffer->DataLength());
    if (data.Size() < HeaderRef::kSizeBytes)
    {
        ChipLogError(Discovery, "Packet to small for mDNS data: %d bytes", static_cast<int>(data.Size()));
        return;
    }

    if (HeaderRef(const_cast<uint8_t *>(data.Start())).GetFlags().IsQuery())
    {
        srv->mDelegate->OnQuery(data, info);
    }
    else
    {
        srv->mDelegate->OnResponse(data, info);
    }
}

} // namespace Minimal
} // namespace mdns
