// nn::bluetooth BLE (Scarlet/Violet: the Pokemon GO Plus + accessory). The host answers as a console whose Bluetooth
// works but never finds a smart device: Initialize succeeds, every Acquire*Event binds an event that is never
// signalled, a scan finds nothing, and connections fail. In the lifted SDK each of these opens the bt service the host
// does not have. The GATT object constructors, destructors and SetValue/GetUuid are plain game memory and stay lifted.
#include <stdint.h>
#include <string.h>

#include "host.h"

#define BT_R_NOT_CONNECTED ((1u << 9) | 113u)   // a failure Result for calls that need a connected device

// void InitializeBle()
static void ov_ble_init(Cpu *c) { (void)c; rt_log("bluetooth: BLE without devices on this host"); }

// Result Acquire*Event(os::SystemEventType *): bound, never signalled
static void ov_ble_event(Cpu *c) {
    uint64_t ev = arg_x(c, 0);
    if (!ev) rt_fatal("nn::bluetooth::Acquire*Event: null event");
    kern_bind_system_event(c, ev, 0);
    ret_x(c, 0);
}

// Result calls that succeed and change nothing (scan start/stop, notification (un)registration)
static void ov_ble_ok(Cpu *c) { ret_x(c, 0); }

// calls that need a connected device: none is ever connected
static void ov_ble_not_connected(Cpu *c) { ret_x(c, BT_R_NOT_CONNECTED); }

// int GetBleScanResult(BleScanResult *, int) / GetBleConnectionInfoList(BleConnectionInfo *, int): nothing found
static void ov_ble_none(Cpu *c) { ret_x(c, 0); }

// Result GetBleScanParameter(GattAttributeUuid *, int): an empty uuid
static void ov_ble_scan_parameter(Cpu *c) {
    void *uuid = arg_p(c, 0);
    if (uuid) memset(uuid, 0, 0x14);   // GattAttributeUuid: u32 length, 16 bytes
    ret_x(c, 0);
}

// void GetGattOperationResult(BleClientGattOperationInfo *): never called without a connection; leave it empty
static void ov_ble_operation_result(Cpu *c) { (void)c; }

HOST_OVERRIDE("_ZN2nn9bluetooth13InitializeBleEv", ov_ble_init);
HOST_OVERRIDE("_ZN2nn9bluetooth19AcquireBleScanEventEPNS_2os15SystemEventTypeE", ov_ble_event);
HOST_OVERRIDE("_ZN2nn9bluetooth24AcquireBleMtuConfigEventEPNS_2os15SystemEventTypeE", ov_ble_event);
HOST_OVERRIDE("_ZN2nn9bluetooth28AcquireBleGattOperationEventEPNS_2os15SystemEventTypeE", ov_ble_event);
HOST_OVERRIDE("_ZN2nn9bluetooth31AcquireBleServiceDiscoveryEventEPNS_2os15SystemEventTypeE", ov_ble_event);
HOST_OVERRIDE("_ZN2nn9bluetooth37AcquireBleConnectionStateChangedEventEPNS_2os15SystemEventTypeE", ov_ble_event);
HOST_OVERRIDE("_ZN2nn9bluetooth23StartBleScanSmartDeviceERKNS0_17GattAttributeUuidE", ov_ble_ok);
HOST_OVERRIDE("_ZN2nn9bluetooth22StopBleScanSmartDeviceEv", ov_ble_ok);
HOST_OVERRIDE("_ZN2nn9bluetooth33RegisterGattOperationNotificationERKNS0_17GattAttributeUuidE", ov_ble_ok);
HOST_OVERRIDE("_ZN2nn9bluetooth35UnregisterGattOperationNotificationERKNS0_17GattAttributeUuidE", ov_ble_ok);
HOST_OVERRIDE("_ZN2nn9bluetooth16GetBleScanResultEPNS0_13BleScanResultEi", ov_ble_none);
HOST_OVERRIDE("_ZN2nn9bluetooth24GetBleConnectionInfoListEPNS0_17BleConnectionInfoEi", ov_ble_none);
HOST_OVERRIDE("_ZN2nn9bluetooth19GetBleScanParameterEPNS0_17GattAttributeUuidEi", ov_ble_scan_parameter);
HOST_OVERRIDE("_ZN2nn9bluetooth22GetGattOperationResultEPNS0_26BleClientGattOperationInfoE", ov_ble_operation_result);
HOST_OVERRIDE("_ZN2nn9bluetooth19ConnectToGattServerERKNS0_7AddressE", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth24DisconnectFromGattServerEj", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth15ConfigureBleMtuEjt", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth22ReadGattCharacteristicERKNS0_18GattCharacteristicE", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth23WriteGattCharacteristicERKNS0_18GattCharacteristicE", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth19WriteGattDescriptorERKNS0_14GattDescriptorE", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth36EnableGattCharacteristicNotificationERKNS0_18GattCharacteristicEb", ov_ble_not_connected);
HOST_OVERRIDE("_ZN2nn9bluetooth14GetGattServiceEPNS0_11GattServiceERKNS0_17GattAttributeUuidEj", ov_ble_none);
HOST_OVERRIDE("_ZNK2nn9bluetooth11GattService18GetCharacteristicsEPNS0_18GattCharacteristicEi", ov_ble_none);
HOST_OVERRIDE("_ZNK2nn9bluetooth18GattCharacteristic14GetDescriptorsEPNS0_14GattDescriptorEi", ov_ble_none);
HOST_OVERRIDE("_ZNK2nn9bluetooth14GattDescriptor17GetCharacteristicEPNS0_18GattCharacteristicE", ov_ble_none);
