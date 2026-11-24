#ifndef GM_PLUGIN_BLE_H
#define GM_PLUGIN_BLE_H

#include "gm_plugin_extensions.h"

/* Resolve the complete, optional native BLE GATT table. This does not open a
 * client or access the radio. The caller still validates the core Host used by
 * its plugin. Old firmware/Studio may return ENOTSUP; handle that explicitly.
 * ID 6 is an immutable full table, not a shorter prefix that can be probed by
 * reading tail pointers. Firmware must expose this complete contract or no ID 6.
 */
static inline gm_plugin_result_t gm_plugin_ble_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_ble_gatt_extension_api_t **ble)
{
    const void *found = NULL;
    gm_plugin_result_t result;
    const gm_plugin_ble_gatt_extension_api_t *table;

    if (ble == NULL) {
        return GM_PLUGIN_EINVAL;
    }
    *ble = NULL;
    if (host == NULL) {
        return GM_PLUGIN_EINVAL;
    }
    if (host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE || host->extension_get == NULL) {
        return GM_PLUGIN_ENOTSUP;
    }
    if (!GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version, GM_PLUGIN_ABI_MIN_VERSION)) {
        return GM_PLUGIN_EVERSION;
    }
    result = host->extension_get(GM_PLUGIN_EXTENSION_BLE_GATT, &found);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    table = (const gm_plugin_ble_gatt_extension_api_t *)found;
    if (table == NULL || table->get_capabilities == NULL || table->open == NULL
        || table->close == NULL || table->get_connected_devices == NULL
        || table->scan_start == NULL || table->scan_stop == NULL || table->connect == NULL
        || table->disconnect == NULL || table->discover_service == NULL || table->read == NULL
        || table->write == NULL || table->subscribe == NULL || table->pair == NULL
        || table->discover_all == NULL || table->unbond == NULL
        || table->get_session_state == NULL || table->get_bonded_devices == NULL) {
        return GM_PLUGIN_ENOTSUP;
    }
    *ble = table;
    return GM_PLUGIN_OK;
}

/* Optional ID 7: do not read beyond the immutable ID 6 table to find refresh.
 * On older hosts, absence disables explicit refresh without blocking ID 6. */
static inline gm_plugin_result_t gm_plugin_ble_refresh_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_ble_gatt_refresh_extension_api_t **refresh)
{
    if (refresh == NULL) {
        return GM_PLUGIN_EINVAL;
    }
    *refresh = NULL;
    const gm_plugin_ble_gatt_extension_api_t *ble;
    gm_plugin_result_t result = gm_plugin_ble_get(host, &ble);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    const void *found = NULL;
    result = host->extension_get(GM_PLUGIN_EXTENSION_BLE_GATT_REFRESH, &found);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    const gm_plugin_ble_gatt_refresh_extension_api_t *table =
        (const gm_plugin_ble_gatt_refresh_extension_api_t *)found;
    if (table == NULL || table->refresh_all == NULL) {
        return GM_PLUGIN_ENOTSUP;
    }
    *refresh = table;
    return GM_PLUGIN_OK;
}

/* Optional ID 8. Fall back to ID 6's plugin-owned list when unavailable. */
static inline gm_plugin_result_t gm_plugin_ble_bond_list_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_ble_gatt_bond_list_extension_api_t **bond_list)
{
    if (bond_list == NULL) {
        return GM_PLUGIN_EINVAL;
    }
    *bond_list = NULL;
    const gm_plugin_ble_gatt_extension_api_t *ble;
    gm_plugin_result_t result = gm_plugin_ble_get(host, &ble);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    const void *found = NULL;
    result = host->extension_get(GM_PLUGIN_EXTENSION_BLE_GATT_BOND_LIST, &found);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    const gm_plugin_ble_gatt_bond_list_extension_api_t *table =
        (const gm_plugin_ble_gatt_bond_list_extension_api_t *)found;
    if (table == NULL || table->get_bonded_accessories == NULL) {
        return GM_PLUGIN_ENOTSUP;
    }
    *bond_list = table;
    return GM_PLUGIN_OK;
}

#endif
