
#include <platform/CHIPDeviceLayer.h>
#include <platform/Linux/bluez/AdapterIterator.h>
#include <platform/internal/BLEManager.h>
#include <support/CHIPMem.h>
#include <support/ReturnMacros.h>

using namespace chip::DeviceLayer::Internal;

/////////// Listing adapters implementation //////////

extern "C" void * pychip_ble_adapter_list_new()
{
    return static_cast<void *>(new AdapterIterator());
}

extern "C" void pychip_ble_adapter_list_delete(void * adapter)
{
    delete static_cast<AdapterIterator *>(adapter);
}

extern "C" bool pychip_ble_adapter_list_next(void * adapter)
{
    return static_cast<AdapterIterator *>(adapter)->Next();
}

extern "C" unsigned pychip_ble_adapter_list_get_index(void * adapter)
{
    /// NOTE: returning unsigned because python native has no sized values
    return static_cast<AdapterIterator *>(adapter)->GetIndex();
}

extern "C" const char * pychip_ble_adapter_list_get_address(void * adapter)
{
    return static_cast<AdapterIterator *>(adapter)->GetAddress();
}

extern "C" const char * pychip_ble_adapter_list_get_alias(void * adapter)
{
    return static_cast<AdapterIterator *>(adapter)->GetAlias();
}

extern "C" const char * pychip_ble_adapter_list_get_name(void * adapter)
{
    return static_cast<AdapterIterator *>(adapter)->GetName();
}

extern "C" bool pychip_ble_adapter_list_is_powered(void * adapter)
{
    return static_cast<AdapterIterator *>(adapter)->IsPowered();
}

///////////// Test only

#include <platform/Linux/bluez/ChipDeviceScanner.h>

namespace {

ChipDeviceScanner::Ptr test_ptr;

class TestDelegate : public ChipDeviceScannerDelegate
{
public:
    virtual ~TestDelegate() { test_ptr.reset(); }

    void OnDeviceScanned(BluezDevice1 * device, const chip::Ble::ChipBLEDeviceIdentificationInfo & info) override
    {
        printf("CHIP device was scanned:\n");
        printf("   Address:       %s\n", bluez_device1_get_address(device));
        printf("   Discriminator: %d\n", info.GetDeviceDiscriminator());
        printf("   ProductId:     %d\n", info.GetProductId());
        printf("   VendorId:      %d\n", info.GetVendorId());
    }

    void OnScanComplete() override { printf("SCAN COMPLETED\n"); }
};

TestDelegate test_delegate;

} // namespace

extern "C" void pychip_ble_test()
{
    AdapterIterator iterator;

    if (iterator.Next())
    {
        printf("Found an adapter: %s\nStarting scan...\n", iterator.GetName());
        test_ptr = ChipDeviceScanner::Create(iterator.GetAdapter(), &test_delegate);
        test_ptr->StartScan(10000);
    }
}

extern "C" void pychip_ble_test_stop()
{
    test_ptr->StopScan();
}
