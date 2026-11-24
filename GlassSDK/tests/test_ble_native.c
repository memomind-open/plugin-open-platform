#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_abi_asserts.h"
#include "../examples/extension/extension.c"

static gm_plugin_ble_gatt_extension_api_t table;
static gm_plugin_result_t ble_result;
static bool ble_null;
static bool refresh_supported, refresh_null;
static gm_plugin_ble_gatt_refresh_extension_api_t refresh_table;
static unsigned capabilities_calls, active_calls, discovery_calls;
static gm_plugin_result_t capabilities(gm_ble_capabilities_t *out)
{
    capabilities_calls++;
    *out = (gm_ble_capabilities_t){sizeof(*out), 64, 512, 8, 4, 1, 1};
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t open_client(gm_ble_callback_t callback, void *context, uint32_t *id)
{
    (void)callback;
    (void)context;
    active_calls++;
    *id = 17;
    return GM_PLUGIN_OK;
}
static void close_client(uint32_t id)
{
    (void)id;
    active_calls++;
}
static gm_plugin_result_t client_only(uint32_t id)
{
    (void)id;
    active_calls++;
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t scan(uint32_t id, const gm_ble_scan_filter_t *filter)
{
    (void)filter;
    return client_only(id);
}
static gm_plugin_result_t connect_device(uint32_t id, const gm_ble_device_t *device, uint32_t request)
{
    (void)device;
    (void)request;
    return client_only(id);
}
static gm_plugin_result_t disconnect_device(uint32_t id, uint32_t session)
{
    (void)session;
    return client_only(id);
}
static gm_plugin_result_t discover(uint32_t id, uint32_t session, const gm_ble_uuid_t *uuid,
                                    uint32_t request)
{
    (void)session;
    (void)uuid;
    (void)request;
    return client_only(id);
}
static gm_plugin_result_t read_value(uint32_t id, uint32_t session, uint16_t handle, uint32_t request)
{
    (void)session;
    (void)handle;
    (void)request;
    return client_only(id);
}
static gm_plugin_result_t write_value(uint32_t id, uint32_t session, uint16_t handle,
                                      const uint8_t *value, uint16_t length, bool response,
                                      uint32_t request)
{
    (void)value;
    (void)length;
    (void)response;
    return read_value(id, session, handle, request);
}
static gm_plugin_result_t subscribe_value(uint32_t id, uint32_t session, uint16_t handle,
                                          bool enable, bool indicate, uint32_t request)
{
    (void)enable;
    (void)indicate;
    return read_value(id, session, handle, request);
}
static gm_plugin_result_t session_request(uint32_t id, uint32_t session, uint32_t request)
{
    (void)session;
    (void)request;
    return client_only(id);
}
static gm_plugin_result_t bonded_devices(uint32_t id, uint32_t request)
{
    (void)request;
    return client_only(id);
}
static uint32_t random_u32(void)
{
    return 42;
}
static int32_t compress_bound(int32_t size)
{
    return size + 16;
}
static void log_message(const char *format, ...)
{
    (void)format;
}
static gm_plugin_result_t extension_get(gm_plugin_extension_id_t id, const void **out)
{
    static const gm_plugin_random_extension_api_t random_api = {random_u32};
    static const gm_plugin_lz4_extension_api_t lz4_api = {.compress_bound = compress_bound};
    static const gm_plugin_libc_extension_api_t libc_api = {.snprintf = snprintf, .strstr = strstr};
    discovery_calls++;
    *out = NULL;
    switch (id) {
        case GM_PLUGIN_EXTENSION_RANDOM:
            *out = &random_api;
            return GM_PLUGIN_OK;
        case GM_PLUGIN_EXTENSION_LZ4:
            *out = &lz4_api;
            return GM_PLUGIN_OK;
        case GM_PLUGIN_EXTENSION_LIBC:
            *out = &libc_api;
            return GM_PLUGIN_OK;
        case GM_PLUGIN_EXTENSION_BLE_GATT:
            *out = ble_null ? NULL : &table;
            return ble_result;
        case GM_PLUGIN_EXTENSION_BLE_GATT_REFRESH:
            *out = refresh_null ? NULL : &refresh_table;
            return refresh_supported ? GM_PLUGIN_OK : GM_PLUGIN_ENOTSUP;
        default:
            return GM_PLUGIN_ENOTSUP;
    }
}
static gm_plugin_ble_gatt_extension_api_t complete_table(void)
{
    gm_plugin_ble_gatt_extension_api_t api = {0};
    api.get_capabilities = capabilities;
    api.open = open_client;
    api.close = close_client;
    api.get_connected_devices = client_only;
    api.scan_start = scan;
    api.scan_stop = client_only;
    api.connect = connect_device;
    api.disconnect = disconnect_device;
    api.discover_service = discover;
    api.read = read_value;
    api.write = write_value;
    api.subscribe = subscribe_value;
    api.pair = session_request;
    api.discover_all = session_request;
    api.unbond = connect_device;
    api.get_session_state = session_request;
    api.get_bonded_devices = bonded_devices;
    return api;
}

int main(void)
{
    gm_plugin_host_api_t host = {0};
    const gm_plugin_ble_gatt_extension_api_t *found = &table;
    assert(gm_plugin_ble_get(NULL, &found) == GM_PLUGIN_EINVAL && found == NULL);
    assert(gm_plugin_ble_get(&host, NULL) == GM_PLUGIN_EINVAL);
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP && found == NULL);
    host.struct_size = sizeof(host);
    host.abi_version = GM_PLUGIN_ABI_VERSION;
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP);
    host.extension_get = extension_get;
    host.struct_size = GM_PLUGIN_MEMBER_END(gm_plugin_host_api_t, extension_get) - 1;
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP && !discovery_calls);
    host.struct_size = sizeof(host);
    host.abi_version = GM_PLUGIN_VERSION(2, 0);
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_EVERSION && !discovery_calls);
    host.abi_version = GM_PLUGIN_ABI_VERSION;
    ble_result = GM_PLUGIN_ENOTSUP;
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP && found == NULL);
    ble_result = GM_PLUGIN_EBUSY;
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_EBUSY && found == NULL);
    ble_result = GM_PLUGIN_OK;
    ble_null = true;
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP && found == NULL);
    ble_null = false;
    table = complete_table();
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_OK && found == &table);

#define MISSING(member) \
    table = complete_table(); \
    table.member = NULL; \
    assert(gm_plugin_ble_get(&host, &found) == GM_PLUGIN_ENOTSUP && found == NULL)
    MISSING(get_capabilities);
    MISSING(open);
    MISSING(close);
    MISSING(get_connected_devices);
    MISSING(scan_start);
    MISSING(scan_stop);
    MISSING(connect);
    MISSING(disconnect);
    MISSING(discover_service);
    MISSING(read);
    MISSING(write);
    MISSING(subscribe);
    MISSING(pair);
    MISSING(discover_all);
    MISSING(unbond);
    MISSING(get_session_state);
    MISSING(get_bonded_devices);
#undef MISSING

    table = complete_table();
    const gm_plugin_ble_gatt_refresh_extension_api_t *refresh = &refresh_table;
    assert(gm_plugin_ble_refresh_get(&host, NULL) == GM_PLUGIN_EINVAL);
    assert(gm_plugin_ble_refresh_get(NULL, &refresh) == GM_PLUGIN_EINVAL && !refresh);
    assert(gm_plugin_ble_refresh_get(&host, &refresh) == GM_PLUGIN_ENOTSUP && !refresh);
    refresh_supported = true;
    assert(gm_plugin_ble_refresh_get(&host, &refresh) == GM_PLUGIN_ENOTSUP && !refresh);
    refresh_table.refresh_all = session_request;
    refresh_null = true;
    assert(gm_plugin_ble_refresh_get(&host, &refresh) == GM_PLUGIN_ENOTSUP && !refresh);
    refresh_null = false;
    assert(gm_plugin_ble_refresh_get(&host, &refresh) == GM_PLUGIN_OK && refresh == &refresh_table);
    assert(!active_calls && sizeof(table) == 17U * sizeof(void (*)(void)));
    gm_plugin_descriptor_t plugin = {0};
    plugin.struct_size = sizeof(plugin);
    host.log = log_message;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    assert(plugin.struct_size == sizeof(plugin) && !active_calls && !capabilities_calls);
    assert(plugin.on_load(NULL) == GM_PLUGIN_OK && capabilities_calls == 1 && !active_calls);
    ble_result = GM_PLUGIN_ENOTSUP;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    assert(plugin.on_load(NULL) == GM_PLUGIN_OK && capabilities_calls == 1 && !active_calls);
    ble_result = GM_PLUGIN_EIO;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_EIO);
    puts("BLE SDK: PASS (ABI/discovery/optional extension example/no radio side effects)");
    return 0;
}
