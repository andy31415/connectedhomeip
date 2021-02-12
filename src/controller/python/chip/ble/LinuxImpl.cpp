
#include <platform/CHIPDeviceLayer.h>
#include <platform/internal/BLEManager.h>
#include <support/CHIPMem.h>
#include <support/ReturnMacros.h>

using namespace chip::DeviceLayer::Internal;

namespace {

// static constexpr const char * kPythonBleDeviceName = "CHIP-PYTHON";

// The device number for CHIPoBLE, without 'hci' prefix.
// Devices can be found using:
//   hcitool dev
constexpr uint32_t kHciInstance = 0;

// TODO: expose a way to list available HCI interfaces and their parameters

class TestDelegate : public BleScanDelegate
{
public:
    void DeviceScanned(const chip::Ble::ChipBLEDeviceIdentificationInfo & info) override { printf("FOUND A DEVICE:\n"); }
};
TestDelegate testDelegate;

} // namespace

extern "C" void test()
{
    auto & impl = BLEMgrImpl();
    impl.StartScanning(&testDelegate);
}

extern "C" CHIP_ERROR pychip_ble_init()
{
    // TODO: move this to something common to the entire python stack
    ReturnErrorOnFailure(chip::DeviceLayer::PlatformMgr().InitChipStack());
    ReturnErrorOnFailure(chip::Platform::MemoryInit());

    chip::DeviceLayer::ConnectivityMgr().SetBLEDeviceName(nullptr); // Use default device name (CHIP-XXXX)

    chip::DeviceLayer::Internal::BLEMgrImpl().ConfigureBle(kHciInstance, true /* isCentral */);

    chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false); // do not want to advertise anything

    // TODO: is this needed at all ?
    //
    // Register a function to receive events from the CHIP device layer.  Note that calls to
    // this function will happen on the CHIP event loop thread, not the app_main thread.
    // PlatformMgr().AddEventHandler(CHIPDeviceManager::CommonDeviceEventHandler, reinterpret_cast<intptr_t>(cb));

    // Start a task to run the CHIP Device event loop.
    ReturnErrorOnFailure(chip::DeviceLayer::PlatformMgr().StartEventLoopTask());

    return CHIP_NO_ERROR;
}