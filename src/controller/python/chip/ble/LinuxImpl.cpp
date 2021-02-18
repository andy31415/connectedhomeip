
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
