#ifndef GM_BLE_ABI_ASSERTS_H
#define GM_BLE_ABI_ASSERTS_H

#include "gm_plugin_ble.h"

#define BLE_CHECK(name, expression) typedef char ble_check_##name[(expression) ? 1 : -1]
BLE_CHECK(id, GM_PLUGIN_EXTENSION_BLE_GATT == 6U);
BLE_CHECK(refresh_id, GM_PLUGIN_EXTENSION_BLE_GATT_REFRESH == 7U);
BLE_CHECK(refresh_slot, offsetof(gm_plugin_ble_gatt_refresh_extension_api_t, refresh_all) == 0U);
BLE_CHECK(refresh_table, sizeof(gm_plugin_ble_gatt_refresh_extension_api_t) == sizeof(void (*)(void)));
BLE_CHECK(bond_list_id, GM_PLUGIN_EXTENSION_BLE_GATT_BOND_LIST == 8U);
BLE_CHECK(bond_list_slot, offsetof(gm_plugin_ble_gatt_bond_list_extension_api_t, get_bonded_accessories) == 0U);
BLE_CHECK(bond_list_table, sizeof(gm_plugin_ble_gatt_bond_list_extension_api_t) == sizeof(void (*)(void)));
BLE_CHECK(uuid, sizeof(gm_ble_uuid_t) == 17U);
BLE_CHECK(device, sizeof(gm_ble_device_t) == 42U);
BLE_CHECK(filter, sizeof(gm_ble_scan_filter_t) == 56U);
BLE_CHECK(capabilities, sizeof(gm_ble_capabilities_t) == 10U);
BLE_CHECK(state, sizeof(gm_ble_session_state_t) == 10U);
BLE_CHECK(event_device, offsetof(gm_ble_event_t, device) == 20U);
BLE_CHECK(event_uuid, offsetof(gm_ble_event_t, uuid) == 62U);
BLE_CHECK(event_handle, offsetof(gm_ble_event_t, handle) == 80U);
BLE_CHECK(event_data, offsetof(gm_ble_event_t, data) == 96U);
BLE_CHECK(event_state_number, GM_BLE_EVENT_SESSION_STATE == 13U);
BLE_CHECK(event_bond_number, GM_BLE_EVENT_BONDED_DEVICE == 14U);
BLE_CHECK(op_connect_number, GM_BLE_OP_CONNECT == 1U);
BLE_CHECK(connect_slot_unavailable, GM_BLE_CONNECT_SLOT_UNAVAILABLE == 2U);
BLE_CHECK(connect_legacy_slot_alias, GM_BLE_CONNECT_LE_SLOTS == GM_BLE_CONNECT_SLOT_UNAVAILABLE);
BLE_CHECK(connect_bond_limit, GM_BLE_CONNECT_BOND_LIMIT == 3U);
BLE_CHECK(op_discover_all_number, GM_BLE_OP_DISCOVER_ALL == 10U);
BLE_CHECK(op_unbond_number, GM_BLE_OP_UNBOND == 11U);
BLE_CHECK(op_state_number, GM_BLE_OP_GET_SESSION_STATE == 12U);
BLE_CHECK(op_bonds_number, GM_BLE_OP_GET_BONDED_DEVICES == 13U);
BLE_CHECK(state_disconnected, GM_BLE_SESSION_DISCONNECTED == 0U);
BLE_CHECK(state_disconnecting, GM_BLE_SESSION_DISCONNECTING == 3U);
BLE_CHECK(session_stable_identity, GM_BLE_SESSION_STABLE_IDENTITY == 1U);
BLE_CHECK(max_write, GM_BLE_WRITE_MAX == 64U);
BLE_CHECK(max_value, GM_BLE_VALUE_MAX == 512U);
BLE_CHECK(max_attributes, GM_BLE_GATT_MAX_ATTRIBUTES == 160U);
BLE_CHECK(discovery_catalog_limit, GM_BLE_DISCOVERY_CATALOG_LIMIT == 1U);
BLE_CHECK(max_bonds, GM_BLE_BOND_MAX == 4U);
BLE_CHECK(max_bonded_accessories, GM_BLE_BONDED_ACCESSORY_MAX == 5U);
BLE_CHECK(bond_system_hogp, GM_BLE_BOND_SYSTEM_HOGP == 1U);

#define BLE_SLOT(member, index) \
    BLE_CHECK(slot_##member, offsetof(gm_plugin_ble_gatt_extension_api_t, member) \
        == (index) * sizeof(void (*)(void)))
BLE_SLOT(get_capabilities, 0U);
BLE_SLOT(open, 1U);
BLE_SLOT(close, 2U);
BLE_SLOT(get_connected_devices, 3U);
BLE_SLOT(scan_start, 4U);
BLE_SLOT(scan_stop, 5U);
BLE_SLOT(connect, 6U);
BLE_SLOT(disconnect, 7U);
BLE_SLOT(discover_service, 8U);
BLE_SLOT(read, 9U);
BLE_SLOT(write, 10U);
BLE_SLOT(subscribe, 11U);
BLE_SLOT(pair, 12U);
BLE_SLOT(discover_all, 13U);
BLE_SLOT(unbond, 14U);
BLE_SLOT(get_session_state, 15U);
BLE_SLOT(get_bonded_devices, 16U);
BLE_CHECK(table, sizeof(gm_plugin_ble_gatt_extension_api_t) == 17U * sizeof(void (*)(void)));
#undef BLE_SLOT
#undef BLE_CHECK

#endif
