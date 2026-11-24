/* Reuse the maintained Host/LVGL/call-UI fixture; execute the real plugin. */
#define TEST_PLUGIN_SOURCE "../examples/ble-gatt-lab/ble-gatt-lab.c"
#define main legacy_call_ui_test
#include "test_examples_call.c"
#undef main
#include <stdarg.h>

static gm_ble_callback_t ble_callback;
static void *ble_context;
static bool supported = true;
static bool bond_list_supported, last_bond_list_was_extended;
static gm_plugin_result_t open_result, submit_result;
static unsigned opens, closes, calls, exits, labels_created, text_updates;
static unsigned write_calls;
static unsigned refresh_calls;
static unsigned allocations_live;
static unsigned allocation_attempts;
static size_t last_allocation_size;
static bool allocation_failure;
static unsigned scan_new_logs, scan_name_logs, scan_summary_logs, raw_scan_logs;
static unsigned notification_logs, notification_loss_logs;
static char last_scan_log[256];
static char last_result_log[256];

static void lab_log(const char *format, ...)
{
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    scan_new_logs += strstr(message, "[ble_lab] scan new ") != NULL;
    scan_name_logs += strstr(message, "[ble_lab] scan name ") != NULL;
    scan_summary_logs += strstr(message, "[ble_lab] scan end ") != NULL
                      || strstr(message, "[ble_lab] scan select ") != NULL;
    raw_scan_logs += strstr(message, "[ble_lab] event=1 ") != NULL;
    notification_logs += strstr(message, "[ble_lab] event=8 ") != NULL;
    notification_loss_logs += strstr(message, "[ble_lab] notify loss ") != NULL;
    if (strstr(message, "[ble_lab] scan ")) {
        snprintf(last_scan_log, sizeof(last_scan_log), "%s", message);
    }
    if (strstr(message, "[ble_lab] event=7 ")) {
        snprintf(last_result_log, sizeof(last_result_log), "%s", message);
    }
}

static void *lab_alloc(size_t length)
{
    assert(length <= DEVICE_STORAGE_BYTES);
    allocation_attempts++;
    last_allocation_size = length;
    if (allocation_failure) {
        return NULL;
    }
    void *memory = malloc(length);
    allocations_live += memory != NULL;
    return memory;
}
static void lab_free(void *memory)
{
    assert(memory != NULL && allocations_live > 0);
    allocations_live--;
    free(memory);
}
static unsigned fail_label;
static uint32_t clock_ms = 100;
static uint8_t last_operation;
static uint16_t last_handle, last_length;
static uint32_t last_id;
static bool last_enable, last_indicate, last_response;
static uint8_t last_bytes[64];
static gm_ble_device_t last_device;
static gm_ble_uuid_t last_service_uuid;
static unsigned char label_objects[8];
static char label_texts[8][512];

static gm_plugin_lvgl_obj_t *lab_label(gm_plugin_lvgl_obj_t *parent)
{
    assert(parent);
    if (++labels_created == fail_label) {
        return NULL;
    }
    assert(labels_created <= sizeof(label_objects));
    return (void *)&label_objects[labels_created - 1U];
}
static void lab_text(gm_plugin_lvgl_obj_t *object, const char *text)
{
    unsigned index = (unsigned)((unsigned char *)object - label_objects);
    assert(index < sizeof(label_objects));
    snprintf(label_texts[index], sizeof(label_texts[index]), "%s", text);
    text_updates++;
}
static uint32_t lab_now(void)
{
    return clock_ms;
}
static void lab_exit(void)
{
    exits++;
}
static gm_plugin_result_t lab_caps(gm_ble_capabilities_t *out)
{
    *out = (gm_ble_capabilities_t){sizeof(*out), 64, 512, 8, 4, 1, 1};
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t lab_open(gm_ble_callback_t callback, void *context, uint32_t *id)
{
    opens++;
    if (open_result != GM_PLUGIN_OK) {
        return open_result;
    }
    ble_callback = callback;
    ble_context = context;
    *id = opens;
    return GM_PLUGIN_OK;
}
static void lab_close(uint32_t id)
{
    assert(id);
    closes++;
    ble_callback = NULL;
}
static gm_plugin_result_t request(uint8_t operation, uint32_t id)
{
    calls++;
    last_operation = operation;
    last_id = id;
    return submit_result;
}
static gm_plugin_result_t lab_connected(uint32_t client)
{
    assert(client == s_lab.client);
    return request(LOCAL_CONNECTED_LIST, 0);
}
static gm_plugin_result_t lab_scan(uint32_t client, const gm_ble_scan_filter_t *filter)
{
    assert(client == s_lab.client && filter->duration_ms == 30000);
    assert(!filter->service.length && !filter->name_prefix[0] && filter->minimum_rssi == -95);
    return request(200, 0);
}
static gm_plugin_result_t lab_scan_stop(uint32_t client)
{
    assert(client == s_lab.client);
    return request(201, 0);
}
static gm_plugin_result_t lab_connect(uint32_t client, const gm_ble_device_t *device, uint32_t id)
{
    assert(client == s_lab.client);
    last_device = *device;
    return request(GM_BLE_OP_CONNECT, id);
}
static gm_plugin_result_t lab_disconnect(uint32_t client, uint32_t session)
{
    assert(client == s_lab.client && session == s_lab.session);
    return request(GM_BLE_OP_DISCONNECT, 0);
}
static gm_plugin_result_t lab_all(uint32_t client, uint32_t session, uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    return request(GM_BLE_OP_DISCOVER_ALL, id);
}
static gm_plugin_result_t lab_refresh(uint32_t client, uint32_t session, uint32_t id)
{
    refresh_calls++;
    return lab_all(client, session, id);
}
static gm_plugin_result_t lab_service(uint32_t client, uint32_t session, const gm_ble_uuid_t *uuid,
                                      uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    assert(uuid->length == 2 || uuid->length == 16);
    last_service_uuid = *uuid;
    return request(GM_BLE_OP_DISCOVER, id);
}
static gm_plugin_result_t lab_read(uint32_t client, uint32_t session, uint16_t handle, uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    last_handle = handle;
    return request(GM_BLE_OP_READ, id);
}
static gm_plugin_result_t lab_write(uint32_t client, uint32_t session, uint16_t handle,
                                    const uint8_t *bytes, uint16_t length, bool response,
                                    uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session && length <= sizeof(last_bytes));
    last_handle = handle;
    last_length = length;
    write_calls++;
    last_response = response;
    memcpy(last_bytes, bytes, length);
    return request(response ? GM_BLE_OP_WRITE : GM_BLE_OP_WRITE_NO_RESPONSE, id);
}
static gm_plugin_result_t lab_subscribe(uint32_t client, uint32_t session, uint16_t handle,
                                        bool enable, bool indicate, uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    last_handle = handle;
    last_enable = enable;
    last_indicate = indicate;
    return request(enable ? GM_BLE_OP_SUBSCRIBE : GM_BLE_OP_UNSUBSCRIBE, id);
}
static gm_plugin_result_t lab_pair(uint32_t client, uint32_t session, uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    return request(GM_BLE_OP_PAIR, id);
}
static gm_plugin_result_t lab_state(uint32_t client, uint32_t session, uint32_t id)
{
    assert(client == s_lab.client && session == s_lab.session);
    return request(GM_BLE_OP_GET_SESSION_STATE, id);
}
static gm_plugin_result_t lab_bonds(uint32_t client, uint32_t id)
{
    assert(client == s_lab.client);
    last_bond_list_was_extended = false;
    return request(GM_BLE_OP_GET_BONDED_DEVICES, id);
}
static gm_plugin_result_t lab_bonded_accessories(uint32_t client, uint32_t id)
{
    assert(client == s_lab.client);
    last_bond_list_was_extended = true;
    return request(GM_BLE_OP_GET_BONDED_DEVICES, id);
}
static gm_plugin_result_t lab_unbond(uint32_t client, const gm_ble_device_t *identity, uint32_t id)
{
    assert(client == s_lab.client);
    assert(identity->address_type == 2 || identity->address_type == 3);
    last_device = *identity;
    return request(GM_BLE_OP_UNBOND, id);
}
static const gm_plugin_ble_gatt_extension_api_t ble_api = {
    lab_caps,    lab_open,       lab_close,   lab_connected, lab_scan,  lab_scan_stop,
    lab_connect, lab_disconnect, lab_service, lab_read,      lab_write, lab_subscribe,
    lab_pair,    lab_all,        lab_unbond,  lab_state,     lab_bonds};
static gm_plugin_result_t lab_get(gm_plugin_extension_id_t id, const void **out)
{
    static const gm_plugin_ble_gatt_refresh_extension_api_t refresh_api = {lab_refresh};
    static const gm_plugin_ble_gatt_bond_list_extension_api_t bond_list_api = {lab_bonded_accessories};
    if (id == GM_PLUGIN_EXTENSION_BLE_GATT) {
        *out = supported ? &ble_api : NULL;
        return supported ? GM_PLUGIN_OK : GM_PLUGIN_ENOTSUP;
    }
    if (id == GM_PLUGIN_EXTENSION_BLE_GATT_REFRESH) {
        *out = &refresh_api;
        return GM_PLUGIN_OK;
    }
    if (id == GM_PLUGIN_EXTENSION_BLE_GATT_BOND_LIST) {
        *out = bond_list_supported ? &bond_list_api : NULL;
        return bond_list_supported ? GM_PLUGIN_OK : GM_PLUGIN_ENOTSUP;
    }
    return t_discover(id, out);
}
static gm_ble_event_t event_for(uint16_t type)
{
    gm_ble_event_t event = {0};
    event.struct_size = sizeof(event);
    event.type = type;
    event.session_id = s_lab.session;
    event.request_id = s_lab.pending_id;
    event.operation = s_lab.pending_op;
    if (type == GM_BLE_EVENT_RESULT) {
        event.handle = s_lab.pending_handle;
    }
    return event;
}
static void emit(gm_ble_event_t *event)
{
    assert(ble_callback);
    ble_callback(ble_context, event);
}
static void result(gm_plugin_result_t rc, uint8_t subscribed)
{
    gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
    event.result = rc;
    event.subscribed = subscribed;
    event.handle = last_handle;
    emit(&event);
}
static void connect_peer(void)
{
    reset_list(LIST_SCAN);
    gm_ble_device_t peer = {.address = {1}, .address_type = 1};
    memcpy(peer.name, "Test peer", 10);
    collect_device(&peer);
    s_lab.list_valid = true;
    s_lab.selected_device = 0;
    run_action(MENU_CONNECT);
    assert(!s_lab.connected && s_lab.pending_op == GM_BLE_OP_CONNECT);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device = peer;
    event.device.connected = 1;
    event.session_id = 41;
    event.mtu = 247;
    emit(&event);
    assert(s_lab.connected && s_lab.session == 41 && !s_lab.catalog_valid);
}
static void metadata(uint16_t type, uint16_t handle, uint16_t value)
{
    gm_ble_event_t event = event_for(type);
    event.operation = 0; /* Targeted discovery metadata really uses zero. */
    event.handle = handle;
    event.value_handle = value;
    event.end_handle = 9;
    event.properties = 0x3e;
    event.uuid.length = 2;
    event.uuid.bytes[0] = 0x37;
    event.uuid.bytes[1] = 0x2a;
    emit(&event);
}
static void discover_catalog(bool targeted)
{
    if (targeted) {
        run_action(MENU_COMMON_SERVICES);
        assert(s_lab.page == PAGE_COMMON_SERVICES);
        activate();
        assert(last_operation == GM_BLE_OP_DISCOVER);
    } else {
        run_action(MENU_DISCOVER_ALL);
    }
    metadata(GM_BLE_EVENT_SERVICE, 1, 0);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 3, 4);
    metadata(GM_BLE_EVENT_DESCRIPTOR, 5, 4);
    assert(!s_lab.catalog_valid && s_lab.catalog_count == 3);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_valid);
    s_lab.page = PAGE_CATALOG;
    s_lab.selection = 1;
    activate();
    assert(s_lab.selected_attribute == 1);
}
static void coexist_lifecycle_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    assert(strstr(s_lab.message, "GATT connected") && strstr(s_lab.message, "No auto checks"));
    discover_catalog(true);
    uint32_t client = s_lab.client, session = s_lab.session;
    uint8_t catalog_count = s_lab.catalog_count;
    unsigned before = calls, closed_before = closes;
    plugin->on_suspend(plugin->context);
    assert(s_lab.suspended && s_lab.connected && s_lab.catalog_valid);
    assert(s_lab.client == client && s_lab.session == session && closes == closed_before);
    /* The plugin must tolerate queued events while suspended or on resume;
     * this fixture does not claim radio delivery continues under every overlay. */
    gm_ble_event_t event = event_for(GM_BLE_EVENT_NOTIFICATION);
    uint8_t value[] = {42};
    event.data = value;
    event.data_length = sizeof(value);
    emit(&event);
    assert(s_lab.value[0] == 42 && s_lab.catalog_count == catalog_count);
    plugin->on_resume(plugin->context);
    plugin->on_loop(plugin->context, 0);
    assert(s_lab.connected && s_lab.client == client && s_lab.session == session);
    assert(calls == before && closes == closed_before); /* No reconnect/pair on resume. */

    run_action(MENU_STATE);
    gm_ble_session_state_t state = {
        sizeof(state), GM_BLE_SESSION_CONNECTED, 1, 1, 0, 1, 0, 1};
    event = event_for(GM_BLE_EVENT_SESSION_STATE);
    event.data = (const uint8_t *)&state;
    event.data_length = sizeof(state);
    emit(&event);
    assert(strstr(s_lab.message, "shared HOGP=1"));
    assert(s_lab.session == session && closes == closed_before);
    run_action(MENU_STATE);
    state.shared_hogp = 0;
    event = event_for(GM_BLE_EVENT_SESSION_STATE);
    event.data = (const uint8_t *)&state;
    event.data_length = sizeof(state);
    emit(&event);
    assert(strstr(s_lab.message, "generic GATT") && strstr(s_lab.message, "shared HOGP=0"));
    plugin->on_stop(plugin->context);
    assert(closes == closed_before + 1 && !s_lab.client);
    plugin->on_unload(plugin->context);
}
static void confirm(uint8_t menu)
{
    unsigned before = calls;
    run_action(menu);
    assert(s_lab.page == PAGE_CONFIRM && s_lab.selection == 0 && calls == before);
    s_lab.selection = 1;
    activate();
    assert(calls == before + 1);
}
static void key(gm_plugin_button_t button, gm_plugin_button_action_t action)
{
    gm_plugin_event_t event = {0};
    event.struct_size = sizeof(event);
    event.type = GM_PLUGIN_EVENT_BUTTON;
    event.data.button.button = button;
    event.data.button.action = action;
    assert(plugin_event(&s_lab, &event));
}

static void auto_connect_peer(void)
{
    reset_list(LIST_SCAN);
    gm_ble_device_t peer = {.address = {1}, .address_type = 1};
    strcpy(peer.name, "Auto peer");
    collect_device(&peer);
    s_lab.list_valid = true;
    s_lab.page = PAGE_DEVICES;
    s_lab.selection = 0;
    unsigned before = calls;
    activate();
    assert(s_lab.page == PAGE_MENU && !s_lab.confirm_enabled && calls == before + 1);
    s_lab.devices[0].address[0] = 99; /* Submission already copied the chosen device. */
    assert(last_operation == GM_BLE_OP_CONNECT && last_device.address[0] == 1);
    assert(s_lab.auto_stage == AUTO_PAIR && s_lab.pending_op == GM_BLE_OP_CONNECT);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device = peer;
    event.device.connected = 1;
    event.mtu = 247;
    event.session_id = 123;
    emit(&event);
    assert(s_lab.connected);
}
static void auto_state(bool encrypted)
{
    gm_ble_session_state_t snapshot = {
        sizeof(snapshot), GM_BLE_SESSION_CONNECTED, encrypted, 1, 1, 0, 0, 2};
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SESSION_STATE);
    event.data = (const uint8_t *)&snapshot;
    event.data_length = sizeof(snapshot);
    emit(&event);
}
static void auto_metadata(uint16_t type, uint16_t handle, uint16_t end, uint16_t value,
                          uint16_t uuid, uint8_t properties)
{
    gm_ble_event_t event = event_for(type);
    event.handle = handle;
    event.end_handle = end;
    event.value_handle = value;
    event.uuid.length = 2;
    event.uuid.bytes[0] = (uint8_t)uuid;
    event.uuid.bytes[1] = (uint8_t)(uuid >> 8);
    event.properties = properties;
    emit(&event);
}
static void automation_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    assert(!s_lab.advanced && page_count() == sizeof(basic_menu));
    reset_list(LIST_SCAN);
    gm_ble_device_t peer = {.address = {1}};
    collect_device(&peer);
    s_lab.list_valid = true;
    s_lab.page = PAGE_DEVICES;
    unsigned before = calls;
    plugin->on_loop(plugin->context, 0); /* No connection before explicit selection. */
    assert(calls == before && !s_lab.auto_stage);

    unsigned writes_before = write_calls;
    auto_connect_peer();
    assert(last_operation == GM_BLE_OP_PAIR && s_lab.pending_op == GM_BLE_OP_PAIR);
    result(GM_PLUGIN_OK, 0);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_GET_SESSION_STATE);
    auto_state(false);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before); /* No query flood, no discovery before encryption. */
    clock_ms += SECURITY_POLL_MS;
    plugin->on_loop(plugin->context, 0);
    auto_state(true);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_DISCOVER_ALL);
    auto_metadata(GM_BLE_EVENT_SERVICE, 1, 9, 0, 0x1812, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 3, 9, 4, 0x2a4d, 0x12);
    auto_metadata(GM_BLE_EVENT_SERVICE, 20, 29, 0, 0x180d, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 22, 25, 23, 0x2a37, 0x1a);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 26, 29, 27, 0x2a38, 0x20);
    auto_metadata(GM_BLE_EVENT_SERVICE, 40, 49, 0, 0x180d, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 42, 49, 43, 0x2a37, 0x12);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before && !s_lab.catalog_valid); /* No partial catalog use. */
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_all && s_lab.catalog_count == 7);
    plugin->on_loop(plugin->context, 0);
    assert(calls == before + 1 && last_operation == GM_BLE_OP_READ && last_handle == 23);
    uint8_t value = 44;
    gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
    event.handle = 23;
    event.data = &value;
    event.data_length = 1;
    emit(&event);
    plugin->on_loop(plugin->context, 0); /* Read enumeration completes. */
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_SUBSCRIBE && last_handle == 23 &&
           last_enable && !last_indicate);
    /* Browse B while the automatic Subscribe for A is still in flight. */
    menu_select(MENU_GATT);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.page == PAGE_GATT);
    unsigned row;
    for (row = 0; row < gatt_count(); row++) {
        int slot = gatt_index(row);
        if (slot >= 0 && s_lab.catalog[slot].value == 27) {
            break;
        }
    }
    assert(row < gatt_count());
    s_lab.selection = (uint16_t)row;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.page == PAGE_CHARACTERISTIC && selected_characteristic()->value == 27);
    result(GM_PLUGIN_OK, 1);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_UNSUBSCRIBE && !last_enable &&
           last_handle == 23 && s_lab.pending_handle == last_handle);
    event = event_for(GM_BLE_EVENT_RESULT);
    event.handle = 27;
    emit(&event);
    assert(s_lab.pending_op == GM_BLE_OP_UNSUBSCRIBE);
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.pending_op && s_lab.auto_stage == AUTO_SUBSCRIBE && s_lab.connected);
    assert(s_lab.page == PAGE_CHARACTERISTIC && selected_characteristic()->value == 27);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_SUBSCRIBE && last_enable && !last_indicate);
    result(GM_PLUGIN_OK, 1);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_SUBSCRIBE && last_handle == 27 && last_indicate);
    result(GM_PLUGIN_OK, 1);
    plugin->on_loop(plugin->context, 0);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_GET_SESSION_STATE);
    auto_state(true);
    plugin->on_loop(plugin->context, 0);
    assert(last_operation == GM_BLE_OP_GET_BONDED_DEVICES);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.auto_stage == AUTO_NONE && s_lab.auto_reads == 1 && s_lab.auto_subscriptions == 2);
    assert(!s_lab.auto_errors && strstr(s_lab.message, "Auto done"));
    assert(write_calls == writes_before && s_lab.connected);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before); /* Monitor, do not loop/reconnect/replay. */
    run_action(MENU_GATT);
    assert(calls == before && s_lab.catalog_all && s_lab.catalog_count == 7);
    assert(s_lab.page == PAGE_GATT && page_count() == 9); /* Render without rediscovery. */
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    event = event_for(GM_BLE_EVENT_NOTIFICATION);
    event.data = &value;
    event.data_length = 1;
    emit(&event);
    assert(s_lab.value[0] == 44 && s_lab.notifications > 0);

    /* Protected or unknown owning services must not be auto-subscribed. */
    s_lab.catalog[2].uuid.bytes[0] = 0x12;
    s_lab.auto_stage = AUTO_SUBSCRIBE;
    s_lab.auto_index = s_lab.auto_subscriptions = 0;
    before = calls;
    auto_pump();
    assert(calls == before && s_lab.auto_stage == AUTO_STATE);
    s_lab.catalog[2].type = GM_BLE_EVENT_ATTRIBUTE;
    s_lab.auto_stage = AUTO_SUBSCRIBE;
    auto_pump();
    assert(calls == before && s_lab.auto_stage == AUTO_STATE);
    s_lab.catalog[2].type = GM_BLE_EVENT_SERVICE;
    s_lab.catalog[2].uuid.bytes[0] = 0x0d;

    /* Submission refusal is not retried; ATT permission errors remain visible. */
    s_lab.auto_stage = AUTO_READ;
    s_lab.auto_index = 0;
    submit_result = GM_PLUGIN_EBUSY;
    auto_pump();
    assert(!s_lab.auto_stage && !s_lab.pending_op && s_lab.connected);
    submit_result = GM_PLUGIN_OK;
    s_lab.auto_stage = AUTO_READ;
    s_lab.auto_index = 0;
    s_lab.auto_errors = 0;
    auto_pump();
    event = event_for(GM_BLE_EVENT_RESULT);
    event.result = GM_PLUGIN_EIO;
    event.att_error = 5;
    emit(&event);
    assert(s_lab.auto_stage == AUTO_READ && s_lab.auto_errors == 1 && !s_lab.pending_op);
    s_lab.auto_stage = AUTO_DISCOVER;
    auto_pump();
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.auto_stage && !s_lab.catalog_valid);

    /* A complete list with no safe auto action stays browseable. */
    s_lab.auto_stage = AUTO_DISCOVER;
    auto_pump();
    auto_metadata(GM_BLE_EVENT_SERVICE, 1, 9, 0, 0x1812, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 3, 9, 4, 0x2a4d, 0x10);
    auto_metadata(GM_BLE_EVENT_SERVICE, 20, 29, 0, 0xfff0, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 22, 29, 23, 0xfff1, 0x08);
    before = calls;
    result(GM_PLUGIN_OK, 0);
    auto_pump();
    assert(calls == before && !s_lab.auto_stage && s_lab.catalog_all);
    assert(strstr(s_lab.message, "no complete catalog/eligible service"));

    /* Security failure and optimistic pair success cannot start discovery. */
    run_action(MENU_REOPEN);
    auto_connect_peer();
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.auto_stage);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before);
    run_action(MENU_REOPEN);
    auto_connect_peer();
    result(GM_PLUGIN_OK, 0);
    for (unsigned i = 0; i < SECURITY_POLLS; i++) {
        clock_ms += SECURITY_POLL_MS;
        plugin->on_loop(plugin->context, 0);
        assert(last_operation == GM_BLE_OP_GET_SESSION_STATE);
        auto_state(false);
    }
    assert(!s_lab.auto_stage && strstr(s_lab.message, "encryption not confirmed"));

    run_action(MENU_REOPEN);
    auto_connect_peer();
    event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(!s_lab.auto_stage && !s_lab.pending_op && !s_lab.connected && calls == before);

    /* Disconnect interrupts an in-flight auto operation, not just idle gaps. */
    run_action(MENU_REOPEN);
    auto_connect_peer();
    run_action(MENU_DISCONNECT);
    assert(!s_lab.auto_stage && s_lab.pending_op == GM_BLE_OP_DISCONNECT);
    event = event_for(GM_BLE_EVENT_RESULT);
    event.operation = GM_BLE_OP_PAIR;
    emit(&event); /* Old completion cannot restart automation. */
    result(GM_PLUGIN_OK, 0);
    event = event_for(GM_BLE_EVENT_CONNECTION);
    emit(&event);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before && !s_lab.connected);

    /* A call UI overlay pauses new actions without drawing over system UI. */
    run_action(MENU_REOPEN);
    t_notify(true);
    auto_connect_peer();
    assert(!s_lab.pending_op && s_lab.auto_stage == AUTO_PAIR);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(calls == before);
    t_notify(false);
    plugin->on_loop(plugin->context, 0);
    assert(s_lab.pending_op == GM_BLE_OP_PAIR);
    event = event_for(GM_BLE_EVENT_DATA_LOSS);
    emit(&event);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    assert(!s_lab.auto_stage && !s_lab.client && calls == before);
    assert(write_calls == writes_before);
    plugin->on_stop(plugin->context);
}

static void live_scan_connect_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    unsigned new_before = scan_new_logs;
    unsigned name_before = scan_name_logs;
    unsigned summaries_before = scan_summary_logs;
    unsigned raw_before = raw_scan_logs;
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 70;
    event.device.address[0] = 7;
    event.device.address_type = 1;
    emit(&event);
    assert(strstr(last_scan_log, "00:00:00:00:00:07"));
    strcpy(event.device.name, "Live peer");
    emit(&event);
    event.device.name[0] = 0;
    emit(&event);
    for (unsigned i = 0; i < 200; i++) {
        event.device.rssi = (int8_t)(-30 - i % 40);
        emit(&event);
    }
    assert(scan_new_logs == new_before + 1 && scan_name_logs == name_before + 1);
    assert(raw_scan_logs == raw_before && s_lab.scan_reports == 203);
    assert(s_lab.device_count == 1 && !strcmp(s_lab.devices[0].name, "Live peer"));
    assert(s_lab.scanning && !s_lab.list_valid);

    unsigned before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT);
    assert(last_device.address[0] == 7 && last_device.address_type == 1);
    assert(!strcmp(last_device.name, "Live peer"));
    assert(s_lab.pending_op == GM_BLE_OP_CONNECT && s_lab.auto_stage == AUTO_PAIR);
    assert(s_lab.scanning && !s_lab.scan_stopping && !s_lab.confirm_enabled);
    assert(s_lab.page == PAGE_MENU && menu_action(0) == MENU_STOP_SCAN);
    assert(scan_summary_logs == summaries_before + 1);
    assert(strstr(last_scan_log, "scan select reports=203 peers=1 partial=0"));

    /* Hide the queued rows but track the original scan until it actually ends. */
    uint32_t pending = s_lab.pending_id;
    char connect_hint[sizeof(s_lab.message)];
    strcpy(connect_hint, s_lab.message);
    event.device.address[0] = 99;
    emit(&event);
    event.type = GM_BLE_EVENT_SCAN_ENDED;
    event.result = GM_PLUGIN_ENOMEM;
    event.scan_id++;
    emit(&event);
    assert(s_lab.scanning && s_lab.pending_id == pending);
    event.scan_id--;
    emit(&event);
    assert(s_lab.device_count == 1 && s_lab.pending_id == pending);
    assert(s_lab.pending_op == GM_BLE_OP_CONNECT && !s_lab.scanning);
    assert(scan_summary_logs == summaries_before + 2);
    assert(!strcmp(s_lab.message, connect_hint));
    run_action(MENU_AUTO_CONNECT);
    assert(calls == before + 1); /* A second request cannot replace the target. */

    run_action(MENU_REOPEN);
    run_action(MENU_SCAN);
    event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 71;
    event.device.address[0] = 8;
    emit(&event);
    submit_result = GM_PLUGIN_EBUSY;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.scanning && !s_lab.pending_op && !s_lab.auto_stage);
    assert(s_lab.page == PAGE_DEVICES && !s_lab.confirm_enabled);
    submit_result = GM_PLUGIN_OK;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.scanning && s_lab.pending_op == GM_BLE_OP_CONNECT);

    /* A queued Connect can fail before firmware stops scanning. The Demo ends
     * it explicitly, but does not try to start a new scan before SCAN_ENDED. */
    event = event_for(GM_BLE_EVENT_RESULT);
    event.result = GM_PLUGIN_EBUSY;
    event.properties = GM_BLE_CONNECT_BOND_LIMIT;
    before = calls;
    emit(&event);
    assert(!s_lab.pending_op && !s_lab.auto_stage && s_lab.client);
    assert(s_lab.scanning && s_lab.scan_stopping && menu_action(0) == MENU_STOP_SCAN);
    assert(calls == before + 1 && last_operation == 201);
    assert(strstr(s_lab.message, "Pairing records full"));
    event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    event.scan_id = 71;
    emit(&event);
    assert(!s_lab.scanning && menu_action(0) == MENU_SCAN);
    assert(strstr(s_lab.message, "Pairing records full"));
    run_action(MENU_SCAN);
    assert(calls == before + 2 && last_operation == 200 && s_lab.scanning);

    event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 72;
    event.device.address[0] = 9;
    emit(&event);
    activate();
    assert(s_lab.scanning && s_lab.pending_op == GM_BLE_OP_CONNECT);
    submit_result = GM_PLUGIN_EBUSY; /* Fallback STOP cannot be enqueued yet. */
    result(GM_PLUGIN_EBUSY, 0);
    assert(!s_lab.pending_op && s_lab.scanning && !s_lab.scan_stopping);
    assert(last_operation == 201 && menu_action(0) == MENU_STOP_SCAN);
    submit_result = GM_PLUGIN_OK;
    run_action(MENU_STOP_SCAN);
    assert(s_lab.scanning && s_lab.scan_stopping && last_operation == 201);
    event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    event.scan_id = 72;
    emit(&event);
    assert(!s_lab.scanning && menu_action(0) == MENU_SCAN);

    /* Management enumerations still require their successful terminal event. */
    run_action(MENU_REOPEN);
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address[0] = 9;
    event.device.address_type = 2;
    emit(&event);
    before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(calls == before && !s_lab.confirm_enabled && !s_lab.bonds_valid);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.bond_count && !s_lab.bonds_valid);
    plugin->on_stop(plugin->context);
}

static void scan_peer(uint8_t address, uint8_t type, const char *name)
{
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 90;
    event.device.address[0] = address;
    event.device.address_type = type;
    snprintf(event.device.name, sizeof(event.device.name), "%s", name);
    emit(&event);
}

static void scan_connect_watchdog_tests(gm_plugin_descriptor_t *plugin)
{
    for (unsigned scenario = 0; scenario < 3; scenario++) {
        bool connected = scenario != 0;
        labels_created = 0;
        assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
        run_action(MENU_SCAN);
        uint32_t scan_started = clock_ms;
        scan_peer(7, 0, "Late peer");
        s_lab.scan_connect_only = true;
        clock_ms += SCAN_DURATION_MS - 200;
        uint32_t connect_started = clock_ms;
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
        assert(s_lab.scanning && s_lab.pending_op == GM_BLE_OP_CONNECT);
        unsigned closed = closes;

        if (connected) {
            gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
            event.device = last_device;
            event.device.connected = 1;
            event.session_id = 41;
            emit(&event);
            assert(s_lab.connected && !s_lab.pending_op);
        }
        /* An accepted Connect near scan expiry must survive the old deadline. */
        clock_ms = scan_started + SCAN_WATCHDOG_MS;
        plugin->on_loop(plugin->context, 0);
        assert(closes == closed && s_lab.client && s_lab.scanning);
        assert(s_lab.connected == connected);
        assert(connected || s_lab.pending_op == GM_BLE_OP_CONNECT);

        if (scenario == 2) {
            /* A genuinely missing terminal event still has a bounded recovery. */
            clock_ms = connect_started + SCAN_WATCHDOG_MS;
            plugin->on_loop(plugin->context, 0);
            assert(closes == closed + 1 && !s_lab.client && !s_lab.scanning);
            assert(strstr(s_lab.message, "Timed out"));
        } else {
            if (!connected) {
                gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
                event.result = GM_PLUGIN_EBUSY;
                event.properties = GM_BLE_CONNECT_BOND_LIMIT;
                emit(&event);
            }
            gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_ENDED);
            event.scan_id = 90;
            emit(&event);
            assert(closes == closed && s_lab.client && !s_lab.scanning);
            assert(strstr(s_lab.message, connected ? "GATT connected" : "Pairing records full"));
        }
        plugin->on_stop(plugin->context);
    }
}

static void scan_connect_mode_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.page == PAGE_DEVICES && s_lab.selection == 0 && page_count() == 2);
    char label[100];
    row_text(1, label, sizeof(label));
    assert(!strcmp(label, "Mode: Auto checks"));
    unsigned before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE); /* Empty peer row. */
    assert(calls == before && s_lab.selection == 0);

    scan_peer(9, 0, "");
    scan_peer(8, 0, "No pairing peer");
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_SINGLE);
    assert(s_lab.selection == 1 && page_count() == 3);
    scan_peer(10, 0, "Another peer");
    assert(s_lab.selection == 1 && page_count() == 4); /* Stream updates cannot move the mode row. */
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    row_text(1, label, sizeof(label));
    assert(!strcmp(label, "Mode: Connect only"));
    assert(calls == before && s_lab.scanning && !s_lab.pending_op);
    row_text(s_lab.selection, label, sizeof(label));
    assert(s_lab.selection == 0 && strstr(label, "No pairing peer"));
    s_lab.auto_reads = 3;
    s_lab.auto_subscriptions = 2;
    s_lab.auto_errors = 1;
    s_lab.notifications = 4;
    s_lab.value_length = 1;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT);
    assert(last_device.address[0] == 8 && s_lab.page == PAGE_MENU && !s_lab.confirm_enabled);
    assert(!s_lab.advanced && menu_action(s_lab.selection) == MENU_VALUE);
    assert(!s_lab.auto_reads && !s_lab.auto_subscriptions && !s_lab.auto_errors);
    assert(!s_lab.notifications && !s_lab.value_length);

    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device = last_device;
    event.device.connected = 1;
    event.session_id = 41;
    event.mtu = 247;
    emit(&event);
    plugin->on_loop(plugin->context, 0);
    assert(s_lab.connected && s_lab.auto_stage == AUTO_NONE && !s_lab.pending_op);
    assert(calls == before + 1); /* No pairing, discovery, or other automatic submission. */
    assert(s_lab.scanning && strstr(s_lab.message, "GATT connected"));
    event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    event.scan_id = 90;
    emit(&event);
    assert(!s_lab.scanning && strstr(s_lab.message, "GATT connected"));
    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    run_action(MENU_SCAN);
    row_text(1, label, sizeof(label));
    assert(!strcmp(label, "Mode: Auto checks") && s_lab.selection == 0);
    scan_peer(7, 0, "Automatic peer");
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(last_device.address[0] == 7 && s_lab.auto_stage == AUTO_PAIR);
    assert(s_lab.pending_op == GM_BLE_OP_CONNECT && !s_lab.confirm_enabled);
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void scan_page_lifecycle_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    unsigned closed = closes;
    run_action(MENU_SCAN);
    scan_peer(1, 0, "Recent peer");
    unsigned before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU && calls == before + 1 && last_operation == 201);
    assert(s_lab.scanning && !s_lab.pending_op && closes == closed); /* Stop acceptance is not completion. */
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(calls == before + 1); /* Repeated Back does not post duplicate stops. */
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    event.scan_id = 90;
    emit(&event);
    assert(!s_lab.scanning && s_lab.client && s_lab.list_valid);
    before = calls;
    run_action(MENU_DEVICES);
    render();
    assert(calls == before && s_lab.device_count == 1 && page_count() == 2);
    assert(strstr(label_texts[0], "recent") && strstr(s_lab.message, "not live"));

    run_action(MENU_SCAN);
    scan_peer(2, 0, "Another peer");
    before = calls;
    run_action(MENU_BONDS);
    assert(calls == before + 1 && last_operation == 201 && s_lab.page == PAGE_BONDS);
    assert(s_lab.scanning && !s_lab.pending_op && !s_lab.bonds_valid);
    run_action(MENU_BONDS);
    assert(calls == before + 1);
    event.scan_id = 91;
    emit(&event);
    assert(calls == before + 1 && s_lab.scanning); /* Wrong scan cannot release Unpair. */
    event.scan_id = 90;
    emit(&event);
    assert(calls == before + 2 && last_operation == GM_BLE_OP_GET_BONDED_DEVICES);
    assert(!s_lab.scanning && s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && !s_lab.bond_count && strstr(s_lab.message, "No plugin bonds"));

    run_action(MENU_SCAN);
    scan_peer(3, 0, "Keep this row");
    run_action(MENU_BONDS);
    before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    emit(&event);
    assert(s_lab.page == PAGE_MENU && !s_lab.scanning && !s_lab.pending_op && calls == before);

    /* Stop rejection is visible and retryable; never fake SCAN_ENDED or close
     * an established GATT link to stop scanning. */
    connect_peer();
    run_action(MENU_SCAN);
    scan_peer(4, 0, "Retry peer");
    submit_result = GM_PLUGIN_EBUSY;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.scanning && s_lab.connected && s_lab.client && closes == closed);
    assert(strstr(s_lab.message, "Stop scan rejected"));
    run_action(MENU_BONDS);
    assert(!s_lab.pending_op && last_operation == 201 && !s_lab.bonds_valid);
    assert(strstr(s_lab.message, "Stop scan rejected"));
    submit_result = GM_PLUGIN_OK;
    run_action(MENU_BONDS);
    assert(last_operation == 201 && !s_lab.pending_op);
    emit(&event);
    assert(last_operation == GM_BLE_OP_GET_BONDED_DEVICES && s_lab.connected);
    result(GM_PLUGIN_OK, 0);
    assert(closes == closed && s_lab.connected);

    run_action(MENU_SCAN);
    scan_peer(5, 0, "Finishing scan");
    submit_result = GM_PLUGIN_ESTATE; /* Host already ended; terminal delivery is still queued. */
    run_action(MENU_BONDS);
    assert(!s_lab.pending_op && s_lab.scanning);
    submit_result = GM_PLUGIN_OK;
    emit(&event);
    assert(last_operation == GM_BLE_OP_GET_BONDED_DEVICES && s_lab.connected);
    result(GM_PLUGIN_OK, 0);
    plugin->on_stop(plugin->context);
    puts("Scan-page lifetime: PASS (stop/deferred Unpair/cancel/retry/retained rows/no link teardown)");
}

static void named_scan_order_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    scan_peer(1, 0, "");
    scan_peer(2, 0, "Beta");
    scan_peer(3, 0, "");
    scan_peer(4, 0, "Alpha");
    assert(s_lab.device_count == 4);
    assert(s_lab.devices[0].address[0] == 2 && s_lab.devices[1].address[0] == 4);
    assert(s_lab.devices[2].address[0] == 1 && s_lab.devices[3].address[0] == 3);
    s_lab.selected_device = 2; /* Saved selection is peer 1; focus is peer 3. */
    s_lab.selection = 4;
    scan_peer(3, 0, "Gamma");
    assert(s_lab.devices[2].address[0] == 3 && s_lab.devices[3].address[0] == 1);
    assert(s_lab.selection == 4 && s_lab.selected_device == 3);
    scan_peer(3, 0, "Renamed");
    for (unsigned i = 0; i < 100; i++) {
        scan_peer(3, 0, "");
    }
    assert(s_lab.device_count == 4 && !strcmp(s_lab.devices[2].name, "Renamed"));
    assert(s_lab.selection == 4 && s_lab.selected_device == 3);

    /* Equal names and addresses do not merge different address types. */
    scan_peer(2, 1, "Beta");
    assert(s_lab.device_count == 5 && s_lab.devices[3].address[0] == 2);
    assert(s_lab.devices[3].address_type == 1 && s_lab.selected_device == 4);
    scan_peer(1, 0, "Latest name");
    assert(s_lab.devices[4].address[0] == 1 && !strcmp(s_lab.devices[4].name, "Latest name"));
    scan_peer(5, 0, "");
    s_lab.selection = 6;
    scan_peer(6, 0, "New named peer");
    assert(s_lab.devices[5].address[0] == 6 && s_lab.devices[6].address[0] == 5);
    assert(s_lab.selection == 6 && s_lab.selected_device == 4);
    assert(s_lab.device_replace == 4); /* Replacement reference still points to peer 1. */
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(last_operation == GM_BLE_OP_CONNECT && last_device.address[0] == 6);
    assert(s_lab.scanning && !s_lab.confirm_enabled);
    plugin->on_stop(plugin->context);

    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    scan_peer(10, 0, "");
    scan_peer(11, 0, "Known");
    s_lab.selected_device = 1;
    s_lab.page = PAGE_MENU;
    s_lab.selection = 1;
    scan_peer(12, 0, "Another");
    assert(s_lab.selection == 1 && s_lab.selected_device == 2);
    assert(s_lab.devices[s_lab.selected_device].address[0] == 10);
    run_action(MENU_REOPEN);
    {
        reset_list(LIST_CONNECTED);
        gm_ble_device_t peer = {0};
        peer.address[0] = 1;
        collect_device(&peer);
        peer.address[0] = 2;
        strcpy(peer.name, "Named management peer");
        collect_device(&peer);
        assert(s_lab.devices[0].address[0] == 1 && s_lab.devices[1].address[0] == 2);
    }
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void named_scan_pressure_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    for (unsigned i = 0; i < DEVICE_INITIAL_CAPACITY; i++) {
        scan_peer((uint8_t)(i + 1U), 0, "Named peer");
    }
    allocation_failure = true;
    s_lab.selected_device = 0;
    scan_peer(99, 0, "New named peer");
    assert(s_lab.device_count == DEVICE_INITIAL_CAPACITY && s_lab.overflow);
    assert(s_lab.selected_device == -1); /* Eviction cannot retain a stale saved target. */
    scan_peer(100, 0, "");
    assert(s_lab.devices[15].address[0] == 100 && !s_lab.devices[15].name[0]);
    assert(s_lab.devices[14].address[0] == 99);
    s_lab.selected_device = 14;
    s_lab.selection = 15;
    scan_peer(101, 0, "Another named peer");
    assert(s_lab.selection == 15 && s_lab.selected_device == 13);
    assert(s_lab.devices[13].address[0] == 99 && s_lab.devices[14].address[0] == 101);
    assert(s_lab.devices[15].address[0] == 100 && !s_lab.devices[15].name[0]);
    scan_peer(100, 0, "Name arrived later");
    scan_peer(100, 0, "");
    assert(!strcmp(s_lab.devices[15].name, "Name arrived later"));
    scan_peer(102, 0, "");
    assert(s_lab.devices[15].address[0] == 102 && !s_lab.devices[15].name[0]);
    assert(s_lab.selection == 15 && s_lab.devices[s_lab.selection - 1U].address[0] == 100);
    assert(s_lab.devices[s_lab.selected_device].address[0] == 99);
    for (unsigned i = 0; i < 15; i++) {
        assert(s_lab.devices[i].name[0]);
    }
    allocation_failure = false;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(last_operation == GM_BLE_OP_CONNECT && last_device.address[0] == 100);
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void refresh_catalog_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    unsigned before = refresh_calls;
    run_action(MENU_GATT);
    assert(refresh_calls == before && s_lab.pending_op == GM_BLE_OP_DISCOVER_ALL);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 3);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_valid);
    unsigned before_open = calls;
    run_action(MENU_GATT);
    assert(calls == before_open); /* Opening a complete catalog has no remote side effects. */
    s_lab.selection = gatt_count();
    char label[100];
    row_text(s_lab.selection, label, sizeof(label));
    assert(!strcmp(label, "Refresh from device"));
    activate();
    assert(s_lab.page == PAGE_GATT && refresh_calls == before + 1);
    assert(s_lab.pending_op == GM_BLE_OP_DISCOVER_ALL);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 4, 5);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_valid && s_lab.catalog_count == 1 && s_lab.catalog[0].value == 5);
    const gm_plugin_ble_gatt_refresh_extension_api_t *refresh = s_lab.refresh;
    s_lab.refresh = NULL;
    before = calls;
    s_lab.selection = gatt_count();
    activate();
    assert(calls == before && !s_lab.pending_op && s_lab.catalog_valid);
    assert(strstr(s_lab.message, "unsupported"));
    s_lab.refresh = refresh;
    activate(); /* Retry the same footer action after the optional ID becomes available. */
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.catalog_valid && s_lab.catalog_count == 0);
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void catalog_allocation_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK && !allocations_live);
    connect_peer();
    unsigned baseline = allocations_live;
    unsigned attempts = allocation_attempts;
    assert(!s_lab.catalog && baseline == 1); /* The scan list is independent. */
    run_action(MENU_DISCOVER_ALL);
    assert(!s_lab.catalog && allocations_live == baseline && allocation_attempts == attempts);
    gm_ble_event_t stale = event_for(GM_BLE_EVENT_SERVICE);
    stale.session_id++;
    emit(&stale);
    assert(!s_lab.catalog && allocation_attempts == attempts);
    for (unsigned i = 0; i < CATALOG_LIMIT; i++) {
        metadata(GM_BLE_EVENT_ATTRIBUTE, (uint16_t)(i + 1), 0);
    }
    assert(s_lab.catalog_count == CATALOG_LIMIT && !s_lab.catalog_valid);
    assert(allocation_attempts == attempts + 1 && allocations_live == baseline + 1);
    assert(last_allocation_size == CATALOG_LIMIT * sizeof(*s_lab.catalog));
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_all && s_lab.catalog[CATALOG_LIMIT - 1U].handle == CATALOG_LIMIT);

    run_action(MENU_REFRESH_ALL);
    assert(!s_lab.catalog && allocations_live == baseline);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 3);
    assert(allocations_live == baseline + 1);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.catalog && !s_lab.catalog_valid && allocations_live == baseline);

    const unsigned closed = closes;
    uint32_t session = s_lab.session;
    attempts = allocation_attempts;
    for (unsigned automatic = 0; automatic < 2; automatic++) {
        if (automatic) {
            enter_page(PAGE_MENU); /* Auto results belong to the connection's main view. */
            s_lab.auto_stage = AUTO_DISCOVER;
            auto_pump();
        } else {
            run_action(MENU_DISCOVER_ALL);
        }
        gm_ble_event_t overflow = event_for(GM_BLE_EVENT_RESULT);
        overflow.result = GM_PLUGIN_ENOMEM;
        overflow.properties = GM_BLE_DISCOVERY_CATALOG_LIMIT;
        emit(&overflow);
        assert(strstr(s_lab.message, "exceeds supported capacity"));
        assert(strstr(s_lab.message, "Find common services"));
        assert(!s_lab.catalog && !s_lab.auto_stage && !s_lab.pending_op);
        assert(s_lab.client && s_lab.session == session && s_lab.connected && closes == closed);
        assert(allocations_live == baseline && allocation_attempts == attempts);
    }
    discover_catalog(true); /* Still usable via targeted discovery. */
    assert(s_lab.catalog_valid && !s_lab.catalog_all && allocations_live == baseline + 1);

    attempts = allocation_attempts;
    run_action(MENU_DISCOVER_ALL);
    allocation_failure = true;
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 3);
    allocation_failure = false;
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 4, 5);
    assert(!s_lab.catalog && allocation_attempts == attempts + 1);
    result(GM_PLUGIN_OK, 0); /* A terminal OK cannot rescue a failed local allocation. */
    assert(strstr(s_lab.message, "Not enough memory") && !s_lab.catalog_valid);
    assert(allocations_live == baseline && s_lab.connected && closes == closed);
    run_action(MENU_DISCOVER_ALL);
    result(GM_PLUGIN_ENOMEM, 0); /* Older Hosts may not distinguish capacity from OOM. */
    assert(strstr(s_lab.message, "memory or firmware limit") && !s_lab.catalog_valid);
    assert(s_lab.connected && closes == closed);

    run_action(MENU_DISCOVER_ALL);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 3);
    result(GM_PLUGIN_OK, 0);
    gm_ble_event_t down = event_for(GM_BLE_EVENT_CONNECTION);
    emit(&down);
    assert(!s_lab.connected && !s_lab.catalog && allocations_live == baseline);
    connect_peer();
    run_action(MENU_DISCOVER_ALL);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 3);
    assert(allocations_live == baseline + 1);
    clock_ms += REQUEST_TIMEOUT_MS;
    plugin->on_loop(plugin->context, 0);
    assert(!s_lab.client && !s_lab.catalog && !allocations_live);
    assert(strstr(s_lab.message, "Timed out"));
    plugin->on_stop(plugin->context);

    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    discover_catalog(false);
    assert(s_lab.catalog && allocations_live == baseline + 1);
    plugin->on_unload(plugin->context);
    plugin->on_stop(plugin->context);
    assert(!s_lab.catalog && !allocations_live);
}

static void scan_cursor_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    scan_peer(1, 0, "");
    scan_peer(2, 0, "");
    scan_peer(3, 0, "");
    s_lab.selection = 3;
    scan_peer(4, 0, "First named");
    assert(s_lab.selection == 3);
    key(GM_PLUGIN_BUTTON_UP, GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(s_lab.selection == 2);
    scan_peer(3, 0, "Name arrived later");
    assert(s_lab.selection == 2);
    key(GM_PLUGIN_BUTTON_UP, GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(s_lab.selection == 1);
    key(GM_PLUGIN_BUTTON_UP, GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(s_lab.selection == 0);
    for (unsigned i = 5; i < 200; i++) {
        scan_peer((uint8_t)i, 0, "Another named peer");
        scan_peer(2, 0, "Late name");
        assert(s_lab.selection == 0);
        assert(s_lab.devices[0].address[0] == 4);
    }
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(last_operation == GM_BLE_OP_CONNECT && last_device.address[0] == 4);
    assert(s_lab.scanning && !s_lab.confirm_enabled);
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void streaming_scan_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_SCAN);
    unsigned summaries_before = scan_summary_logs;
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 71;
    for (unsigned i = 0; i < 300; i++) {
        event.device.address[0] = (uint8_t)(i + 1);
        event.device.address[1] = (uint8_t)((i + 1) >> 8);
        snprintf(event.device.name, sizeof(event.device.name), "Peer %u", i);
        emit(&event);
    }
    assert(s_lab.device_count == 300 && s_lab.device_capacity >= 300 && !s_lab.overflow);
    unsigned closed = closes;
    clock_ms += REQUEST_TIMEOUT_MS + 1;
    plugin->on_loop(plugin->context, 0);
    assert(s_lab.client && s_lab.scanning && closes == closed);
    event.type = GM_BLE_EVENT_SCAN_ENDED;
    emit(&event);
    assert(s_lab.list_valid && !s_lab.scanning);
    assert(scan_summary_logs == summaries_before + 1);
    assert(strstr(last_scan_log, "scan end rc=0 reports=300 peers=300 partial=0"));
    s_lab.page = PAGE_DEVICES;
    s_lab.selection = 300;
    activate();
    assert(s_lab.pending_op == GM_BLE_OP_CONNECT && !s_lab.confirm_enabled);
    assert(!strcmp(last_device.name, "Peer 299"));

    run_action(MENU_REOPEN);
    run_action(MENU_SCAN);
    event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 73;
    for (unsigned i = 0; i < 1000; i++) {
        event.device.address[0] = (uint8_t)(i + 1);
        event.device.address[1] = (uint8_t)((i + 1) >> 8);
        snprintf(event.device.name, sizeof(event.device.name), "Peer %u", i);
        emit(&event);
    }
    assert(s_lab.overflow && s_lab.device_count == DEVICE_STORAGE_BYTES / sizeof(event.device));
    bool last_peer_found = false;
    for (unsigned i = 0; i < s_lab.device_count; i++) {
        last_peer_found |= !strcmp(s_lab.devices[i].name, "Peer 999");
    }
    assert(last_peer_found && s_lab.client && s_lab.scanning);

    run_action(MENU_REOPEN);
    assert(!allocations_live && !s_lab.device_capacity);
    run_action(MENU_SCAN);
    event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 72;
    for (unsigned i = 0; i < DEVICE_INITIAL_CAPACITY; i++) {
        event.device.address[0] = (uint8_t)(i + 1);
        strcpy(event.device.name, "Named peer");
        emit(&event);
    }
    event.device.name[0] = 0;
    emit(&event);
    assert(!strcmp(s_lab.devices[DEVICE_INITIAL_CAPACITY - 1].name, "Named peer"));
    allocation_failure = true;
    event.device.address[0] = 99;
    strcpy(event.device.name, "Late peer");
    emit(&event);
    assert(s_lab.overflow && s_lab.device_count == DEVICE_INITIAL_CAPACITY);
    unsigned late_row = DEVICE_INITIAL_CAPACITY - 1U;
    assert(s_lab.devices[late_row].address[0] == 99 && s_lab.devices[0].address[0] == 2);
    event.type = GM_BLE_EVENT_SCAN_ENDED;
    event.result = GM_PLUGIN_ENOMEM;
    emit(&event);
    assert(s_lab.list_valid && s_lab.client);
    s_lab.page = PAGE_DEVICES;
    s_lab.selection = (uint16_t)(late_row + 1U);
    activate();
    assert(s_lab.pending_op == GM_BLE_OP_CONNECT && !s_lab.confirm_enabled);
    assert(!strcmp(last_device.name, "Late peer"));
    allocation_failure = false;

    /* Fixed-size owned bonds do not depend on allocating a scan list. */
    run_action(MENU_REOPEN);
    allocation_failure = true;
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address_type = 2;
    emit(&event);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && s_lab.bond_count == 1 && !allocations_live);
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address_type = 2;
    for (unsigned i = 0; i <= GM_BLE_BOND_MAX; i++) {
        event.device.address[0] = (uint8_t)(i + 1U);
        emit(&event);
    }
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_overflow && !s_lab.bonds_valid && !s_lab.bond_count);
    allocation_failure = false;

    run_action(MENU_REOPEN);
    run_action(MENU_SCAN);
    clock_ms += SCAN_WATCHDOG_MS + 1;
    plugin->on_loop(plugin->context, 0);
    assert(!s_lab.client && strstr(s_lab.message, "Timed out"));
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
}

static void simple_menu_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    assert(!s_lab.advanced && page_count() == 7);
    const unsigned expected[] = {
        MENU_SCAN, MENU_SAVED_DEVICES, MENU_DISCONNECT, MENU_BONDS, MENU_VALUE, MENU_GATT, MENU_EXIT};
    char label[100];
    for (unsigned i = 0; i < 7; i++) {
        assert(menu_action(i) == expected[i]);
        row_text(i, label, sizeof(label));
        assert(label[0]);
    }
    unsigned closed_before = closes;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.scanning && s_lab.page == PAGE_DEVICES);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.device.address[0] = 9;
    strcpy(event.device.name, "Test peer");
    emit(&event);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU && menu_action(0) == MENU_STOP_SCAN);
    row_text(0, label, sizeof(label));
    assert(!strcmp(label, "Stop scan"));
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(last_operation == 201 && s_lab.scanning && closes == closed_before);
    event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    emit(&event);
    assert(!s_lab.scanning && menu_action(0) == MENU_SCAN && s_lab.device_count == 1);
    menu_select(MENU_SAVED_DEVICES);
    activate();
    assert(s_lab.page == PAGE_SAVED_DEVICES && s_lab.device_count == 1);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device = s_lab.devices[0];
    event.device.address_type = 2;
    emit(&event);
    result(GM_PLUGIN_OK, 0);
    row_text(0, label, sizeof(label));
    assert(strstr(label, "Test peer") && !strstr(label, "dBm"));
    strcpy(s_lab.bonds[0].name, "A longer BLE peer name here");
    row_text(0, label, sizeof(label));
    assert(strstr(label, s_lab.bonds[0].name));
    /* Simplified rendering must not cover a request failure with a static hint. */
    say("Request rejected (-3). See log for details.");
    render();
    assert(strstr(label_texts[2], "rejected (-3)"));
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);

    unsigned before = calls;
    run_action(MENU_ADVANCED);
    const unsigned tools[] = {MENU_COMMON_SERVICES, MENU_CATALOG, MENU_STATE, MENU_ADVANCED};
    assert(s_lab.advanced && page_count() == sizeof(tools) / sizeof(tools[0]));
    for (unsigned i = 0; i < page_count(); i++) {
        assert(menu_action(i) == tools[i]);
        row_text(i, label, sizeof(label));
        assert(label[0]);
        assert(tools[i] != MENU_READ && tools[i] != MENU_NOTIFY && tools[i] != MENU_DISCONNECT);
    }
    s_lab.page = PAGE_CATALOG;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU && s_lab.advanced);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU && !s_lab.advanced && calls == before);
    assert(closes == closed_before);

    s_lab.auto_reads = 2;
    s_lab.auto_subscriptions = 1;
    s_lab.auto_errors = 1;
    s_lab.notifications = 3;
    s_lab.value[0] = 0x2c;
    s_lab.value_length = 1;
    s_lab.value_offset = 0;
    run_action(MENU_VALUE);
    render();
    assert(strstr(label_texts[1], "2C"));
    assert(strstr(label_texts[2], "Reads=2") && strstr(label_texts[2], "Subs=1"));
    assert(strstr(label_texts[2], "Errors=1") && strstr(label_texts[2], "RX:3 Loss:0"));

    quarantine("Test cleanup; retry client.");
    assert(!s_lab.client && menu_action(0) == MENU_REOPEN);
    before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.client && menu_action(0) == MENU_SCAN && calls == before);
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void select_gatt_value(uint16_t handle)
{
    s_lab.page = PAGE_GATT;
    s_lab.gatt_view = false;
    for (unsigned row = 0; row < gatt_count(); row++) {
        int slot = gatt_index(row);
        if (s_lab.catalog[slot].type == GM_BLE_EVENT_CHARACTERISTIC &&
            s_lab.catalog[slot].value == handle) {
            s_lab.selection = row;
            activate();
            assert(s_lab.page == PAGE_CHARACTERISTIC && s_lab.gatt_view);
            return;
        }
    }
    assert(!"missing GATT characteristic");
}
static void gatt_catalog(void)
{
    auto_metadata(GM_BLE_EVENT_SERVICE, 1, 9, 0, 0xfff0, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 5, 3, 0xfff1, 0x3e);
    auto_metadata(GM_BLE_EVENT_DESCRIPTOR, 4, 4, 3, 0x2902, 0);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 6, 9, 7, 0xfff2, 2);
    auto_metadata(GM_BLE_EVENT_SERVICE, 16, 24, 0, 0x1812, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 17, 24, 18, 0x2a4d, 0x3e);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SERVICE);
    event.handle = 32;
    event.end_handle = 40;
    event.uuid = (gm_ble_uuid_t){16, {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0, 0, 0x80, 0, 0x10, 0, 0, 0x0f, 0x18, 0, 0}};
    emit(&event);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 33, 40, 34, 0x2a19, 0x12);
    auto_metadata(GM_BLE_EVENT_SERVICE, 48, 56, 0, 0xfff3, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 50, 56, 51, 0xfff4, 0x04);
    auto_metadata(GM_BLE_EVENT_SERVICE, 64, 72, 0, 0xfff5, 1);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 66, 72, 67, 0xfff6, 0x20);
    assert(!s_lab.catalog_valid && gatt_count() == 0); /* No partial metadata as operable UI. */
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.catalog_valid && s_lab.catalog_all && gatt_count() == 11);
}
static void begin_gatt_write(unsigned action)
{
    uint8_t actions[CHARACTERISTIC_ACTION_MAX];
    unsigned count = characteristic_actions(actions);
    for (unsigned i = 0; i < count; i++) {
        if (actions[i] == action) {
            s_lab.selection = i;
            unsigned before = write_calls;
            activate();
            assert(s_lab.page == PAGE_HEX && s_lab.edit_write && write_calls == before);
            s_lab.tx[0] = 0xde;
            s_lab.tx[1] = 0xad;
            s_lab.tx_length = 2;
            s_lab.selection = 6;
            activate();
            assert(s_lab.page == PAGE_CONFIRM && !s_lab.selection && write_calls == before);
            return;
        }
    }
    assert(!"missing write action");
}
static void characteristic_page_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    unsigned before = calls;
    run_action(MENU_GATT);
    assert(s_lab.page == PAGE_GATT && page_count() == 2 && calls == before);
    s_lab.selection = 1;
    activate();
    assert(s_lab.advanced && page_count() == 4); /* Extra diagnostics reachable offline. */
    run_action(MENU_COMMON_SERVICES);
    assert(s_lab.page == PAGE_MENU && calls == before && strstr(s_lab.message, "first"));
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    connect_peer();
    run_action(MENU_GATT);
    assert(last_operation == GM_BLE_OP_DISCOVER_ALL && s_lab.page == PAGE_GATT);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.catalog_valid && page_count() == 2);
    run_action(MENU_GATT);
    gatt_catalog();
    char row[96];
    row_text(1, row, sizeof(row));
    assert(strstr(row, "[RWNI]") && strstr(row, "FFF1"));
    row_text(6, row, sizeof(row));
    assert(strstr(row, "[RN]") && strstr(row, "2A19"));
    row_text(8, row, sizeof(row));
    assert(strstr(row, "[W]") && strstr(row, "FFF4"));
    row_text(10, row, sizeof(row));
    assert(strstr(row, "[I]") && strstr(row, "FFF6"));
    assert(page_count() == 13 && gatt_index(2) == 3); /* Descriptor is not a menu row. */
    s_lab.advanced = true; /* Opening from the diagnostic toolbox uses the same simple view. */
    run_action(MENU_GATT);
    assert(!s_lab.advanced && s_lab.page == PAGE_GATT);
    s_lab.selection = gatt_count() + 1;
    activate();
    assert(s_lab.advanced && s_lab.page == PAGE_MENU);
    run_action(MENU_GATT);
    s_lab.catalog_all = false; /* A successful targeted catalog is not the full-service list. */
    assert(gatt_count() == 0 && gatt_index(0) == -1);
    s_lab.catalog_all = true;
    before = calls;
    run_action(MENU_GATT);
    assert(calls == before); /* Re-entering a complete catalog does not issue discovery. */
    s_lab.selection = 0;
    activate();
    assert(calls == before && s_lab.page == PAGE_GATT);
    select_gatt_value(3);
    uint8_t actions[CHARACTERISTIC_ACTION_MAX];
    assert(characteristic_actions(actions) == 7);
    assert(actions[0] == MENU_READ && actions[1] == MENU_WRITE && actions[2] == MENU_WRITE_COMMAND);
    assert(actions[3] == MENU_NOTIFY && actions[4] == MENU_INDICATE);
    assert(actions[5] == MENU_UNSUBSCRIBE && actions[6] == MENU_VALUE);
    attribute_t *a = selected_characteristic();
    s_lab.selection = 0;
    activate();
    assert(s_lab.pending_op == GM_BLE_OP_READ && s_lab.pending_handle == 3);
    assert(a->result_state == 1 && strstr(s_lab.message, "waiting"));
    uint8_t bytes[] = {0x41, 0x42};
    gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
    event.data = bytes;
    event.data_length = sizeof(bytes);
    event.request_id++;
    emit(&event);
    assert(a->result_state == 1);
    event.request_id--;
    event.session_id++;
    emit(&event);
    assert(a->result_state == 1);
    event.session_id--;
    event.handle = 7;
    emit(&event);
    assert(a->result_state == 1);
    event.handle = 3;
    emit(&event);
    assert(a->result_state == 2 && a->read_length == 2 && s_lab.page == PAGE_CHARACTERISTIC);
    assert(strstr(s_lab.message, "Read OK 2 B: 4142"));
    render();
    assert(strstr(label_texts[0], "FFF1") && strstr(label_texts[2], "Sub:?"));

    s_lab.selection = 3;
    activate();
    assert(last_enable && !last_indicate && !a->subscription_known);
    result(GM_PLUGIN_OK, 1);
    assert(a->subscribed && a->subscription_known && !a->notifications);
    assert(strstr(s_lab.message, "confirmed ON"));
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[2], "Sub:ON Notifications:0"));
    unsigned during_burst = text_updates;
    uint32_t last_draw = clock_ms;
    uint8_t notification_data[] = {0x50, 0x42};
    event = event_for(GM_BLE_EVENT_NOTIFICATION);
    event.handle = 3;
    event.data = notification_data;
    event.data_length = sizeof(notification_data);
    for (unsigned i = 0; i < 8; i++) {
        clock_ms = last_draw + i * 20U;
        notification_data[0] = (uint8_t)(0x50U + i);
        emit(&event);
        plugin->on_loop(plugin->context, 0);
        assert(text_updates == during_burst);
        assert(a->notifications == i + 1 && s_lab.value[0] == notification_data[0]);
    }
    clock_ms = last_draw + NOTIFICATION_REDRAW_MS - 1U;
    plugin->on_loop(plugin->context, 0);
    assert(text_updates == during_burst);
    clock_ms++;
    plugin->on_loop(plugin->context, 0);
    assert(text_updates > during_burst && strstr(label_texts[2], "Notifications:8"));
    during_burst = text_updates;
    plugin->on_loop(plugin->context, 0); /* No repeated redraw without fresh data. */
    assert(text_updates == during_burst);

    s_lab.selection = 6; /* Latest data */
    activate();
    plugin->on_loop(plugin->context, 0);
    assert(s_lab.page == PAGE_VALUE && strstr(label_texts[1], "5742"));
    during_burst = text_updates;
    notification_data[0] = 0xAB;
    emit(&event);
    plugin->on_loop(plugin->context, 0);
    assert(text_updates == during_burst && s_lab.value[0] == 0xAB);
    assert(strstr(label_texts[1], "5742"));
    t_notify(true);
    clock_ms += NOTIFICATION_REDRAW_MS;
    plugin->on_loop(plugin->context, 0);
    assert(text_updates == during_burst && a->notifications == 9);
    t_notify(false);
    plugin->on_loop(plugin->context, 0);
    assert(text_updates > during_burst && strstr(label_texts[1], "AB42"));
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_CHARACTERISTIC);
    s_lab.selection = 4;
    activate();
    assert(last_handle == 3 && last_enable && last_indicate);
    result(GM_PLUGIN_OK, 1);
    plugin->on_loop(plugin->context, 0); /* An operation result bypasses the throttle. */
    assert(strstr(label_texts[2], "confirmed ON"));
    s_lab.selection = 5;
    activate();
    event = event_for(GM_BLE_EVENT_RESULT);
    event.result = GM_PLUGIN_EIO;
    event.att_error = 5;
    event.subscribed = 1;
    emit(&event);
    assert(a->subscribed && strstr(s_lab.message, "FAILED") && strstr(s_lab.message, "ATT=05"));
    render();
    assert(strstr(label_texts[2], "Sub:ON"));
    activate();
    result(GM_PLUGIN_OK, 0);
    assert(!a->subscribed && strstr(s_lab.message, "confirmed OFF"));

    s_lab.selection = 0;
    activate();
    event = event_for(GM_BLE_EVENT_RESULT);
    event.data = bytes;
    event.data_length = sizeof(bytes);
    select_gatt_value(7); /* An in-flight read still belongs to A. */
    attribute_t *b = selected_characteristic();
    emit(&event);
    assert(a->read_length == 2 && !b->result_state && s_lab.pending_op == 0);
    assert(!strstr(s_lab.message, "Read OK"));
    s_lab.selection = 1;
    activate();
    render();
    assert(s_lab.page == PAGE_VALUE && strstr(label_texts[1], "No latest data"));
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_CHARACTERISTIC && selected_characteristic() == b);

    select_gatt_value(3);
    begin_gatt_write(MENU_WRITE);
    before = write_calls;
    activate(); /* Default Cancel. */
    assert(s_lab.page == PAGE_CHARACTERISTIC && write_calls == before);
    begin_gatt_write(MENU_WRITE);
    s_lab.selection = 1;
    activate();
    assert(s_lab.page == PAGE_CHARACTERISTIC && last_response && last_handle == 3);
    assert(last_length == 2 && last_bytes[0] == 0xde && last_bytes[1] == 0xad);
    assert(a->result_state == 1 && !strstr(s_lab.message, "Write OK"));
    result(GM_PLUGIN_OK, 0);
    assert(strstr(s_lab.message, "Write OK (ATT response)"));
    begin_gatt_write(MENU_WRITE_COMMAND);
    s_lab.selection = 1;
    activate();
    assert(!last_response && last_handle == 3);
    result(GM_PLUGIN_OK, 0);
    select_gatt_value(51);
    assert(characteristic_actions(actions) == 2 && actions[0] == MENU_WRITE_COMMAND);
    begin_gatt_write(MENU_WRITE_COMMAND);
    s_lab.selection = 1;
    activate();
    assert(!last_response && last_handle == 51);
    result(GM_PLUGIN_OK, 0);
    assert(strstr(s_lab.message, "LOCAL SUBMISSION ONLY") && !strstr(s_lab.message, "Write OK"));

    select_gatt_value(67);
    assert(characteristic_actions(actions) == 3 && actions[0] == MENU_INDICATE);
    s_lab.selection = 0;
    activate();
    assert(last_indicate && last_enable);
    result(GM_PLUGIN_OK, 1);
    select_gatt_value(18);
    assert(characteristic_actions(actions) == 5 && actions[0] == MENU_READ);
    assert(actions[1] == MENU_NOTIFY && actions[2] == MENU_INDICATE);
    assert(actions[3] == MENU_UNSUBSCRIBE);
    assert(strstr(s_lab.message, "shared HOGP"));
    s_lab.selection = 1;
    activate();
    assert(last_handle == 18 && last_enable && !last_indicate);
    result(GM_PLUGIN_EPERM, 0); /* Live shared-link protection remains a Host decision. */
    assert(s_lab.page == PAGE_CHARACTERISTIC && strstr(s_lab.message, "FAILED"));
    select_gatt_value(34); /* Independent Bluetooth-base 128-bit BAS can be subscribed. */
    assert(characteristic_actions(actions) == 4 && actions[0] == MENU_READ);
    s_lab.selection = 1;
    activate();
    assert(last_handle == 34 && last_enable && !last_indicate);
    result(GM_PLUGIN_OK, 1);
    assert(selected_characteristic()->subscription_known && selected_characteristic()->subscribed);
    attribute_t *bas = selected_characteristic();
    uint16_t declaration = bas->handle;
    bas->handle = 0xffff; /* Unknown service must remain read-only. */
    assert(characteristic_actions(actions) == 2 && actions[0] == MENU_READ);
    bas->handle = declaration;

    select_gatt_value(3);
    s_lab.selection = 0;
    activate();
    event = event_for(GM_BLE_EVENT_RESULT);
    event.data = NULL;
    event.data_length = 0;
    t_notify(true);
    unsigned updates = text_updates;
    emit(&event);
    plugin->on_loop(plugin->context, 0);
    assert(a->read_length == 0 && strstr(s_lab.message, "Read OK 0 B"));
    assert(text_updates == updates);
    t_notify(false);
    plugin->on_loop(plugin->context, 0);
    assert(text_updates > updates);

    begin_gatt_write(MENU_WRITE);
    plugin->on_suspend(plugin->context);
    assert(!s_lab.confirm_enabled && s_lab.page == PAGE_CHARACTERISTIC);
    plugin->on_resume(plugin->context);
    before = write_calls;
    execute_confirmed();
    assert(write_calls == before);

    select_gatt_value(3);
    s_lab.selection = 1;
    activate();
    assert(s_lab.page == PAGE_HEX);
    uint16_t old_mtu = s_lab.mtu;
    s_lab.mtu = 4;
    s_lab.tx_length = 2;
    s_lab.selection = 6;
    activate();
    assert(s_lab.page == PAGE_CHARACTERISTIC && !s_lab.confirm_enabled);
    assert(write_calls == before && strstr(s_lab.message, "MTU-3"));
    s_lab.mtu = old_mtu;

    /* Refreshing metadata cannot claim that the remote CCCD is off. Explicit
     * unsubscribe remains available even while the local confirmed state is unknown. */
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    s_lab.selection = gatt_count();
    activate();
    gatt_catalog();
    select_gatt_value(3);
    a = selected_characteristic();
    assert(!a->subscription_known);
    s_lab.selection = 5;
    activate();
    assert(last_operation == GM_BLE_OP_UNSUBSCRIBE && !last_enable);
    result(GM_PLUGIN_OK, 0);
    assert(a->subscription_known && !a->subscribed);

    select_gatt_value(3);
    s_lab.selection = 0;
    activate();
    event = event_for(GM_BLE_EVENT_RESULT);
    gm_ble_event_t down = event_for(GM_BLE_EVENT_CONNECTION);
    down.device.connected = 0;
    emit(&down);
    assert(!s_lab.catalog_valid && !s_lab.gatt_view && s_lab.page == PAGE_MENU);
    emit(&event);
    assert(!s_lab.pending_op && !s_lab.catalog_valid); /* Late result cannot revive the old view. */
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void common_service_tests(gm_plugin_descriptor_t *plugin)
{
    static const struct {
        const char *label;
        uint8_t low;
    } presets[] = {
        {"Battery (0x180F)", 0x0f},
        {"Device Information (0x180A)", 0x0a},
        {"HID (0x1812)", 0x12},
    };
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_ADVANCED);
    unsigned before = calls;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    assert(s_lab.page == PAGE_MENU && s_lab.advanced && calls == before);
    assert(strstr(s_lab.message, "Connect / attach a peer first."));
    connect_peer();
    menu_select(MENU_COMMON_SERVICES);

    for (unsigned i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        before = calls;
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
        assert(s_lab.page == PAGE_COMMON_SERVICES && page_count() == 3 && calls == before);
        char label[80];
        row_text(i, label, sizeof(label));
        assert(!strcmp(label, presets[i].label));
        if (i == 0) {
            key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
            assert(s_lab.page == PAGE_MENU && s_lab.advanced && calls == before);
            key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
        }
        for (unsigned row = 0; row < i; row++) {
            key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_SINGLE);
        }
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE);
        assert(s_lab.page == PAGE_CATALOG && s_lab.pending_op == GM_BLE_OP_DISCOVER);
        assert(calls == before + 1 && last_operation == GM_BLE_OP_DISCOVER);
        assert(last_service_uuid.length == 2);
        assert(last_service_uuid.bytes[0] == presets[i].low && last_service_uuid.bytes[1] == 0x18);

        if (i == 0) {
            auto_metadata(GM_BLE_EVENT_SERVICE, 1, 4, 0, 0x180f, 1);
            auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 2, 4, 3, 0x2a19, 0x12);
            assert(!s_lab.catalog_valid);
            result(GM_PLUGIN_OK, 0);
            assert(s_lab.catalog_valid && !s_lab.catalog_all && s_lab.catalog_count == 2);
            assert(!strncmp(s_lab.message, "Service found.", strlen("Service found.")));
            row_text(0, label, sizeof(label));
            assert(strstr(label, "Service 180F"));
            row_text(1, label, sizeof(label));
            assert(strstr(label, "Characteristic 2A19"));
        } else if (i == 1) {
            result(GM_PLUGIN_EIO, 0);
            assert(!s_lab.catalog_valid && !s_lab.catalog_count);
            assert(!strncmp(s_lab.message, "Service discovery failed.", strlen("Service discovery failed.")));
            assert(!strstr(s_lab.message, "rc=") && !strstr(s_lab.message, "ATT="));
            assert(strstr(last_result_log, "op=2 ") && strstr(last_result_log, "rc=-5 ATT=0"));
        } else {
            key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
            assert(s_lab.page == PAGE_MENU && s_lab.advanced && calls == before + 1);
            result(GM_PLUGIN_EIO, 0);
            assert(!s_lab.pending_op && !strstr(s_lab.message, "Service discovery failed."));
            break;
        }
        assert(calls == before + 1); /* Results never trigger an implicit full discovery. */
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
        assert(s_lab.page == PAGE_MENU && s_lab.advanced);
    }
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void page_status_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    run_action(MENU_BONDS);
    assert(s_lab.page == PAGE_BONDS);
    result(GM_PLUGIN_OK, 0);
    assert(strstr(s_lab.message, "No plugin bonds"));
    unsigned before = calls;
    run_action(MENU_DEVICES);
    assert(s_lab.page == PAGE_DEVICES && calls == before && !device_rows());
    assert(!strstr(s_lab.message, "unpair"));
    activate();
    assert(!strstr(s_lab.message, "unpair") && calls == before);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_MENU && !strstr(s_lab.message, "unpair"));

    run_action(MENU_BONDS);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
    run_action(MENU_DEVICES); /* Empty Unpair arrives only after leaving that page. */
    emit(&event);
    render();
    assert(!s_lab.pending_op && s_lab.bonds_valid && !s_lab.bond_count);
    assert(s_lab.page == PAGE_DEVICES && !strstr(label_texts[2], "unpair"));

    const unsigned routes[] = {
        MENU_DEVICES, MENU_CATALOG, MENU_VALUE, MENU_HEX, MENU_ADVANCED, MENU_GATT};
    before = calls;
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        say("STALE PAGE MESSAGE");
        uint32_t old_view = s_lab.view_id;
        run_action(routes[i]);
        render();
        assert(s_lab.view_id != old_view && !strstr(s_lab.message, "STALE"));
        assert(!strstr(label_texts[2], "STALE"));
        say("STALE RETURN MESSAGE");
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
        render();
        assert(!strstr(s_lab.message, "STALE") && !strstr(label_texts[2], "STALE"));
    }
    assert(calls == before); /* Browsing alone adds no radio requests. */

    run_action(MENU_SCAN);
    event = event_for(GM_BLE_EVENT_SCAN_ENDED);
    run_action(MENU_CATALOG);
    emit(&event);
    assert(!s_lab.scanning && s_lab.page == PAGE_CATALOG);
    assert(!strstr(s_lab.message, "Scan ended"));

    connect_peer();
    run_action(MENU_ADVANCED);
    before = calls;
    run_action(MENU_COMMON_SERVICES);
    assert(s_lab.page == PAGE_COMMON_SERVICES && calls == before);
    char label[60];
    row_text(0, label, sizeof(label));
    assert(!strcmp(label, "Battery (0x180F)"));
    activate();
    assert(s_lab.page == PAGE_CATALOG && calls == before + 1
           && last_operation == GM_BLE_OP_DISCOVER);
    result(GM_PLUGIN_EIO, 0);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    run_action(MENU_STATE);
    gm_ble_session_state_t snapshot = {
        sizeof(snapshot), GM_BLE_SESSION_CONNECTED, 1, 1, 1, 0, 0, 0};
    event = event_for(GM_BLE_EVENT_SESSION_STATE);
    event.data = (const uint8_t *)&snapshot;
    event.data_length = sizeof(snapshot);
    run_action(MENU_GATT); /* Busy: only a view change, not another discovery. */
    emit(&event);
    assert(!s_lab.pending_op && s_lab.page == PAGE_GATT);
    assert(!strstr(s_lab.message, "STATE="));

    run_action(MENU_GATT);
    gatt_catalog();
    select_gatt_value(3);
    run_action(MENU_READ);
    event = event_for(GM_BLE_EVENT_RESULT);
    uint8_t value = 0x2c;
    event.data = &value;
    event.data_length = 1;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    run_action(MENU_DEVICES);
    emit(&event);
    assert(!s_lab.pending_op && s_lab.value[0] == 0x2c);
    assert(s_lab.page == PAGE_DEVICES && !strstr(s_lab.message, "Read OK"));
    select_gatt_value(3);
    assert(strstr(s_lab.message, "Read OK 1 B")); /* Real result remains on its own characteristic. */
    say("STALE BEFORE HEX");
    s_lab.selection = 1;
    activate();
    assert(s_lab.page == PAGE_HEX && !strstr(s_lab.message, "STALE"));
    s_lab.selection = 6;
    activate();
    assert(s_lab.page == PAGE_CONFIRM && !s_lab.message[0]);
    render();
    assert(strstr(label_texts[2], "h=0003"));
    activate(); /* Cancel, never send a write. */
    assert(s_lab.page == PAGE_CHARACTERISTIC && strstr(s_lab.message, "Cancelled"));
    say("STALE BEFORE VALUE");
    run_action(MENU_VALUE);
    assert(s_lab.page == PAGE_VALUE && !s_lab.message[0]);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(s_lab.page == PAGE_CHARACTERISTIC && strstr(s_lab.message, "Read OK"));

    /* Leaving a management query cannot carry its result into Devices. */
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_RESULT);
    event.result = GM_PLUGIN_EIO;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    run_action(MENU_DEVICES);
    emit(&event);
    assert(s_lab.page == PAGE_DEVICES && !s_lab.pending_op && !s_lab.bonds_valid);
    assert(!strstr(s_lab.message, "RESULT op="));

    run_action(MENU_CONNECTED);
    event = event_for(GM_BLE_EVENT_CONNECTED_DEVICE);
    event.device.address[0] = 9;
    emit(&event);
    event.type = GM_BLE_EVENT_CONNECTED_DEVICES_DONE;
    run_action(MENU_CATALOG);
    run_action(MENU_DEVICES); /* Same page ID, but a new visit: the old completion is not its hint. */
    emit(&event);
    assert(s_lab.page == PAGE_DEVICES && s_lab.list_valid && s_lab.device_count == 1);
    assert(!strstr(s_lab.message, "list complete"));

    enter_page(PAGE_MENU);
    s_lab.auto_stage = AUTO_BONDS;
    auto_pump();
    assert(s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES);
    run_action(MENU_CATALOG);
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.auto_stage && s_lab.page == PAGE_CATALOG);
    assert(!strstr(s_lab.message, "Auto done"));
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void device_reconnect_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    assert(menu_action(s_lab.selection) == MENU_VALUE); /* Never default to a destructive action. */
    unsigned before = calls;
    activate();
    activate();
    assert(s_lab.page == PAGE_VALUE && s_lab.connected && calls == before);

    run_action(MENU_BONDS);
    result(GM_PLUGIN_OK, 0);
    before = calls;
    run_action(MENU_DEVICES);
    assert(page_count() == 2 && calls == before);
    char label[100];
    row_text(0, label, sizeof(label));
    assert(strstr(label, "Test peer"));
    assert(s_lab.devices[0].address[0] == 1);

    run_action(MENU_BONDS);
    gm_ble_event_t bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device.address[0] = 8;
    bond.device.address_type = 2;
    strcpy(bond.device.name, "Owned bond");
    emit(&bond);
    strcpy(bond.device.name, "Updated owned bond");
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && s_lab.bond_count == 1 && s_lab.device_count == 1);
    assert(s_lab.devices[0].address[0] == 1 && s_lab.selected_device == 0);
    run_action(MENU_DEVICES);
    row_text(0, label, sizeof(label));
    assert(strstr(label, "Test peer"));
    enter_page(PAGE_BONDS);
    row_text(0, label, sizeof(label));
    assert(strstr(label, "Updated owned bond"));
    before = calls;
    activate();
    assert(s_lab.page == PAGE_CONFIRM && s_lab.confirm_enabled && s_lab.selection == 0);
    activate(); /* Cancel leaves this unrelated live session alone. */
    assert(s_lab.page == PAGE_MENU && !s_lab.confirm_enabled);
    assert(calls == before && s_lab.peer.address[0] == 1);

    run_action(MENU_BONDS);
    bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device.address[0] = 8;
    bond.device.address_type = 2;
    emit(&bond);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.bonds_valid && !s_lab.bond_count);
    assert(s_lab.list_valid && s_lab.device_count == 1 && s_lab.devices[0].address[0] == 1);

    enter_page(PAGE_MENU);
    s_lab.auto_stage = AUTO_BONDS;
    auto_pump();
    menu_select(MENU_GATT); /* Finishing the checks must not move a user's browsing cursor. */
    unsigned cursor = s_lab.selection;
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.auto_stage && s_lab.selection == cursor);
    run_action(MENU_DEVICES);
    assert(page_count() == 2 && s_lab.devices[0].address[0] == 1);

    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    assert(!s_lab.connected);
    run_action(MENU_BONDS);
    bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device.address[0] = 8;
    bond.device.address_type = 2;
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    activate();
    assert(s_lab.page == PAGE_CONFIRM && s_lab.confirm_peer.address[0] == 8);
    assert(s_lab.confirm_peer.address_type == 2 && s_lab.selected_device == 0);
    before = calls;
    activate(); /* Default Cancel must not delete the retained connect target. */
    assert(calls == before && s_lab.peer.address[0] == 1);
    before = calls;
    plugin->on_loop(plugin->context, 0);
    run_action(MENU_DEVICES);
    assert(page_count() == 2 && calls == before); /* No scan and no automatic reconnect. */
    s_lab.selection = 0;
    activate();
    assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT);
    assert(last_device.address[0] == 1 && last_device.address_type == 1);
    assert(!strcmp(last_device.name, "Test peer"));
    event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device = last_device;
    event.device.connected = 1;
    event.session_id = 42;
    event.mtu = 247;
    emit(&event);
    assert(s_lab.connected && s_lab.session == 42);

    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    run_action(MENU_CONNECTED);
    event = event_for(GM_BLE_EVENT_CONNECTED_DEVICES_DONE);
    emit(&event); /* Even an empty refresh must not hide the last successful peer. */
    run_action(MENU_DEVICES);
    assert(s_lab.device_count == 0 && page_count() == 1);
    row_text(0, label, sizeof(label));
    assert(strstr(label, "Test peer"));
    before = calls;
    s_lab.selection = 0;
    activate();
    assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT && last_device.address[0] == 1);
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void retained_peer_selection_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    run_action(MENU_SCAN);
    assert(!s_lab.device_count && page_count() == 2);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_SINGLE);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_DOUBLE); /* Connect only. */
    unsigned before = calls;
    activate();
    assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT);
    assert(s_lab.selected_device == SELECT_LAST_PEER && menu_action(s_lab.selection) == MENU_VALUE);
    assert(last_device.address[0] == 1 && !strcmp(last_device.name, "Test peer"));
    assert(s_lab.auto_stage == AUTO_NONE && s_lab.scanning);
    event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.device.address[0] = 9;
    strcpy(event.device.name, "New scan peer");
    emit(&event);
    assert(!s_lab.device_count && last_device.address[0] == 1); /* Ignore late scan tail. */
    plugin->on_stop(plugin->context);
    assert(!s_lab.peer_known && !s_lab.bond_count && !allocations_live);
    plugin->on_unload(plugin->context);
}
static void unpair_history_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    gm_ble_event_t event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    assert(s_lab.peer_known && !s_lab.connected && s_lab.session == 41);
    /* Prepare history from the forgotten peer and a public peer with equal
     * bytes. Only the random base/identity aliases are the selected target. */
    gm_ble_device_t other = {.address = {1}, .address_type = 0};
    strcpy(other.name, "Other peer");
    collect_device(&other);
    s_lab.value_length = 1;
    s_lab.value[0] = 44;
    run_action(MENU_BONDS);
    gm_ble_event_t bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device = s_lab.peer;
    bond.device.address_type = 3;
    bond.device.connected = 0;
    emit(&bond);
    bond.device = other;
    bond.device.address_type = 2;
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    s_lab.selected_bond = 0;
    confirm(MENU_UNBOND);
    result(GM_PLUGIN_EIO, 0);
    assert(s_lab.peer_known && s_lab.bond_count == 2 && s_lab.device_count == 2);
    assert(s_lab.value_length == 1 && s_lab.value[0] == 44);

    confirm(MENU_UNBOND);
    /* The completion must use the confirmed identity, not a selection that
     * navigation or another event may have moved. Old firmware sends no peer. */
    s_lab.selected_bond = 1;
    s_lab.tx[0] = 0xde;
    s_lab.tx[1] = 0xad;
    s_lab.tx_length = 2;
    s_lab.cursor = 3;
    s_lab.confirm_tx[0] = 0xde;
    s_lab.confirm_length = 1;
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.peer_known && !s_lab.session && !s_lab.value_length && !s_lab.value[0]);
    assert(s_lab.device_count == 1 && s_lab.devices[0].address_type == 0);
    assert(s_lab.bond_count == 1 && s_lab.bonds[0].address_type == 2);
    assert(s_lab.tx_length == 1 && !s_lab.cursor && !s_lab.tx[0] && !s_lab.tx[1]);
    assert(!s_lab.confirm_length && !s_lab.confirm_tx[0]);

    unsigned before = write_calls;
    run_action(MENU_HEX);
    assert(s_lab.page == PAGE_HEX && s_lab.selection == 0);
    activate(); /* Cursor +1 on the fresh blank byte. */
    assert(s_lab.cursor == 1);
    s_lab.selection = 5;
    activate(); /* Length -1 cannot remove the last editable byte. */
    assert(s_lab.tx_length == 1 && s_lab.cursor == 1);
    s_lab.selection = 6;
    activate();
    assert(s_lab.page == PAGE_MENU && write_calls == before);

    /* An APP unpair notice is independent of pending request/session IDs.
     * Only invalidates matching rows while another GATT peer remains active. */
    connect_peer();
    event = (gm_ble_event_t){.struct_size = sizeof(event),
        .type = GM_BLE_EVENT_RESULT, .operation = GM_BLE_OP_UNBOND,
        .request_id = 0, .result = GM_PLUGIN_OK, .device = other};
    begin(GM_BLE_OP_READ);
    emit(&event);
    assert(s_lab.connected && s_lab.session == 41 && s_lab.peer_known);
    assert(s_lab.pending_op == GM_BLE_OP_READ && !s_lab.bond_count);
    result(GM_PLUGIN_EIO, 0);

    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    gm_ble_event_t down = event_for(GM_BLE_EVENT_CONNECTION);
    down.device.connected = 0;
    emit(&down);
    run_action(MENU_SCAN);
    scan_peer(1, 1, "Before APP unpair");
    scan_peer(2, 0, "Unaffected");
    s_lab.selected_device = 1;
    s_lab.selection = 2; /* Unaffected peer (the mode command sits at row 1). */
    event.device = (gm_ble_device_t){.address = {1}, .address_type = 3};
    event.result = GM_PLUGIN_EIO;
    emit(&event);
    assert(s_lab.peer_known && s_lab.device_count == 2);
    event.result = GM_PLUGIN_OK;
    emit(&event);
    assert(!s_lab.peer_known && !s_lab.session && s_lab.scanning);
    assert(s_lab.device_count == 1 && s_lab.devices[0].address[0] == 2);
    assert(s_lab.selected_device == 0 && s_lab.selection == 0);
    scan_peer(1, 1, "New advertisement");
    assert(s_lab.device_count == 2 && !s_lab.peer_known);
    assert(!strcmp(s_lab.devices[1].name, "New advertisement"));

    run_action(MENU_STOP_SCAN);
    gm_ble_event_t ended = event_for(GM_BLE_EVENT_SCAN_ENDED);
    ended.scan_id = s_lab.scan_id;
    emit(&ended);
    connect_peer();
    begin(GM_BLE_OP_READ);
    emit(&event); /* Forget can be polled before the old session's link-down. */
    assert(!s_lab.peer_known && !s_lab.session && !s_lab.pending_op && !s_lab.connected);
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void unpair_bond(const gm_ble_device_t *identity)
{
    run_action(MENU_BONDS);
    gm_ble_event_t bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device = *identity;
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    s_lab.selected_bond = 0;
    confirm(MENU_UNBOND);
}

static void online_unpair_tests(gm_plugin_descriptor_t *plugin)
{
    gm_ble_device_t live_bond = {.address = {9, 8, 7, 6, 5, 0xC4}, .address_type = 3,
                                 .connected = 1};
    gm_ble_session_state_t verified = {
        .struct_size = sizeof(verified), .state = GM_BLE_SESSION_CONNECTED,
        .encrypted = 1, .bonded = 1, .managed = 1};
    for (unsigned ack_first = 0; ack_first < 2; ack_first++) {
        labels_created = 0;
        assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
        connect_peer();
        s_lab.peer.address[5] = s_lab.devices[0].address[5] = 0x41; /* Live RPA, not stable bond. */
        unpair_bond(&live_bond);
        assert(last_operation == GM_BLE_OP_GET_SESSION_STATE && s_lab.unpair_stage == UNPAIR_VERIFY);

        gm_ble_event_t proof = event_for(GM_BLE_EVENT_SESSION_STATE);
        proof.device = live_bond;
        proof.properties = GM_BLE_SESSION_STABLE_IDENTITY;
        proof.data = (const uint8_t *)&verified;
        proof.data_length = sizeof(verified);
        unsigned before = calls;
        proof.request_id++;
        emit(&proof);
        proof.request_id--;
        proof.session_id++;
        emit(&proof);
        proof.session_id--;
        assert(calls == before && s_lab.unpair_stage == UNPAIR_VERIFY);
        emit(&proof);
        assert(calls == before + 1 && last_operation == GM_BLE_OP_DISCONNECT);
        assert(s_lab.unpair_stage == UNPAIR_WAIT_DOWN && s_lab.connected);

        if (ack_first) {
            result(GM_PLUGIN_OK, 0);
            assert(s_lab.pending_op == LOCAL_DISCONNECT_WAIT);
        }
        gm_ble_event_t down = {.struct_size = sizeof(down), .type = GM_BLE_EVENT_CONNECTION,
                               .session_id = 42, .result = GM_PLUGIN_EIO};
        emit(&down);
        down.session_id = s_lab.session;
        down.operation = GM_BLE_OP_CONNECT;
        down.request_id = 98;
        emit(&down);
        assert(calls == before + 1 && s_lab.connected);
        down.operation = 0;
        down.request_id = 0;
        emit(&down);
        assert(calls == before + 2 && last_operation == GM_BLE_OP_UNBOND);
        assert(!s_lab.connected && s_lab.pending_op == GM_BLE_OP_UNBOND);
        assert(last_device.address_type == 3 && !memcmp(last_device.address, live_bond.address, 6));
        emit(&down);
        assert(calls == before + 2 && s_lab.pending_op == GM_BLE_OP_UNBOND);
        s_lab.selected_bond = -1; /* The request does not depend on the current UI selection. */
        result(GM_PLUGIN_OK, 0);
        assert(!s_lab.pending_op && !s_lab.unpair_stage && !s_lab.peer_known);
        assert(!s_lab.device_count && !s_lab.bond_count); /* Clean the verified RPA alias too. */
        plugin->on_stop(plugin->context);
    }

    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    unsigned closed = closes;
    for (unsigned denied = 0; denied < 5; denied++) {
        unpair_bond(&live_bond);
        gm_ble_session_state_t snapshot = verified;
        snapshot.shared_hogp = denied == 2;
        snapshot.busy = denied == 3;
        gm_ble_event_t proof = event_for(GM_BLE_EVENT_SESSION_STATE);
        proof.device = live_bond;
        proof.device.address_type = denied == 1 ? 2 : live_bond.address_type;
        proof.properties = denied == 0 ? 0 : GM_BLE_SESSION_STABLE_IDENTITY;
        proof.data = denied == 4 ? NULL : (const uint8_t *)&snapshot;
        proof.data_length = sizeof(snapshot);
        unsigned before = calls;
        emit(&proof);
        assert(calls == before && closes == closed && !s_lab.pending_op && !s_lab.unpair_stage);
        assert(s_lab.connected && strstr(s_lab.message, "Cannot verify"));
    }
    unpair_bond(&live_bond);
    gm_ble_event_t proof = event_for(GM_BLE_EVENT_SESSION_STATE);
    proof.device = live_bond;
    proof.properties = GM_BLE_SESSION_STABLE_IDENTITY;
    proof.data = (const uint8_t *)&verified;
    proof.data_length = sizeof(verified);
    unsigned before = calls;
    clock_ms += REQUEST_TIMEOUT_MS;
    plugin->on_loop(plugin->context, 0);
    emit(&proof); /* A late valid proof after the local authorization expired. */
    assert(calls == before && closes == closed && s_lab.connected && !s_lab.unpair_stage);

    unpair_bond(&live_bond);
    proof.request_id = s_lab.pending_id;
    emit(&proof);
    assert(last_operation == GM_BLE_OP_DISCONNECT);
    result(GM_PLUGIN_EBUSY, 0);
    assert(!s_lab.unpair_stage && s_lab.connected && strstr(s_lab.message, "No bond removed"));

    gm_ble_device_t offline_bond = live_bond;
    offline_bond.connected = 0;
    unpair_bond(&offline_bond);
    assert(last_operation == GM_BLE_OP_UNBOND && s_lab.connected);
    result(GM_PLUGIN_OK, 0);
    assert(closes == closed && s_lab.connected && s_lab.peer_known && !s_lab.bond_count);

    unpair_bond(&live_bond);
    proof.request_id = s_lab.pending_id;
    emit(&proof);
    assert(last_operation == GM_BLE_OP_DISCONNECT);
    before = calls;
    clock_ms += REQUEST_TIMEOUT_MS;
    plugin->on_loop(plugin->context, 0);
    gm_ble_event_t down = {.struct_size = sizeof(down), .type = GM_BLE_EVENT_CONNECTION,
                           .session_id = s_lab.session, .result = GM_PLUGIN_EIO};
    emit(&down);
    assert(calls == before && !s_lab.connected && !s_lab.pending_op && !s_lab.unpair_stage);
    assert(closes == closed && s_lab.client);
    plugin->on_stop(plugin->context);
    assert(!allocations_live);
    puts("Online unpair: PASS (stable identity, real detach, stale events, timeout, offline peer)");
}

static void saved_devices_tests(gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_host_api_t *host = s_lab.host;
    for (unsigned reload = 0; reload < 2; reload++) {
        labels_created = 0;
        assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
        assert(!s_lab.device_count && !s_lab.peer_known && !s_lab.bond_count);
        menu_select(menu_action(1)); /* Basic Devices, not Advanced scan history. */
        unsigned before = calls;
        activate();
        assert(calls == before + 1 && last_operation == GM_BLE_OP_GET_BONDED_DEVICES);
        assert(s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES && !s_lab.scanning);
        assert(!last_bond_list_was_extended); /* Older Hosts retain the ID 6 list. */
        assert(strstr(s_lab.message, "Loading saved GATT"));
        gm_ble_event_t event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
        event.device.address[0] = 0x34;
        event.device.address[1] = 0x12;
        event.device.address_type = 2;
        strcpy(event.device.name, "Saved GATT peer");
        emit(&event);
        assert(!s_lab.bonds_valid && page_count() == 0);
        before = calls;
        activate();
        assert(calls == before && !s_lab.confirm_enabled);
        result(GM_PLUGIN_OK, 0);
        assert(s_lab.bonds_valid && page_count() == 1 && !s_lab.pending_op);
        char label[128];
        row_text(0, label, sizeof(label));
        assert(strstr(label, "Saved GATT peer"));
        s_lab.selection = 0;
        before = calls;
        activate();
        assert(calls == before + 1 && last_operation == GM_BLE_OP_CONNECT);
        assert(last_device.address[0] == 0x34 && last_device.address_type == 2);
        assert(!s_lab.confirm_enabled && s_lab.page == PAGE_MENU);
        assert(menu_action(s_lab.selection) == MENU_VALUE);
        plugin->on_stop(plugin->context);
        plugin->on_unload(plugin->context);
        assert(!s_lab.client && !s_lab.bond_count && !allocations_live);
        plugin->struct_size = sizeof(*plugin);
        assert(gm_plugin_entry(host, plugin) == GM_PLUGIN_OK);
    }

    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    menu_select(menu_action(1));
    activate();
    result(GM_PLUGIN_OK, 0);
    assert(!page_count() && strstr(s_lab.message, "No saved GATT"));
    unsigned before = calls;
    activate();
    assert(calls == before && !s_lab.confirm_enabled);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    menu_select(menu_action(1));
    activate();
    gm_ble_event_t event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address[0] = 9;
    event.device.address_type = 3;
    emit(&event);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.bonds_valid && !s_lab.bond_count && !page_count());
    assert(!strstr(s_lab.message, "No saved GATT"));
    before = calls;
    activate();
    assert(calls == before);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    menu_select(menu_action(1));
    submit_result = GM_PLUGIN_EBUSY;
    activate();
    assert(!s_lab.pending_op && !s_lab.bonds_valid && !page_count());
    assert(!strstr(s_lab.message, "No saved GATT"));
    submit_result = GM_PLUGIN_OK;
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    menu_select(menu_action(1));
    activate();
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address_type = 2;
    emit(&event);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.page == PAGE_MENU && !strstr(s_lab.message, "Select an offline"));
    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);

    for (unsigned leave_early = 0; leave_early < 2; leave_early++) {
        labels_created = 0;
        assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
        run_action(MENU_SCAN);
        key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
        before = calls;
        menu_select(menu_action(1));
        activate();
        assert(s_lab.scanning && s_lab.bonds_after_scan && calls == before);
        if (leave_early) {
            key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
        }
        event = event_for(GM_BLE_EVENT_SCAN_ENDED);
        emit(&event);
        assert(!s_lab.scanning && calls == before + (leave_early ? 0 : 1));
        if (!leave_early) {
            assert(last_operation == GM_BLE_OP_GET_BONDED_DEVICES);
            result(GM_PLUGIN_OK, 0);
        }
        plugin->on_stop(plugin->context);
        plugin->on_unload(plugin->context);
    }
}
static void notification_source_preview_tests(gm_plugin_descriptor_t *plugin)
{
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
    connect_peer();
    run_action(MENU_DISCOVER_ALL);
    auto_metadata(GM_BLE_EVENT_SERVICE, 0x0020, 0x0040, 0, 0x1812, 0);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 0x0030, 0x0033, 0x0031, 0x2A4D, 0x10);
    auto_metadata(GM_BLE_EVENT_CHARACTERISTIC, 0x0034, 0x003F, 0x0035, 0x2A4D, 0x10);
    result(GM_PLUGIN_OK, 0);
    run_action(MENU_VALUE);
    render();

    uint8_t bytes[17] = {0x11};
    gm_ble_event_t event = event_for(GM_BLE_EVENT_NOTIFICATION);
    event.handle = 0x0031;
    event.data = bytes;
    event.data_length = sizeof(bytes);
    emit(&event);
    clock_ms += NOTIFICATION_REDRAW_MS;
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[0], "Notify value 0x0031"));
    assert(strstr(label_texts[1], "Offset 000: 11") && strstr(label_texts[1], "Offset 016:"));
    assert(strstr(label_texts[2], "Service UUID: 1812\nChar UUID: 2A4D"));

    bytes[0] = 0x22;
    event.handle = 0x0035;
    event.data_length = 1;
    emit(&event);
    clock_ms += NOTIFICATION_REDRAW_MS;
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[0], "Notify value 0x0035"));
    assert(strstr(label_texts[1], "Offset 000: 22") && strstr(label_texts[1], "RX:2 Loss:0"));
    assert(strstr(label_texts[2], "Service UUID: 1812\nChar UUID: 2A4D"));

    gm_ble_event_t loss = event_for(GM_BLE_EVENT_DATA_LOSS);
    loss.properties = GM_BLE_DATA_LOSS_NOTIFICATION_ONLY;
    loss.request_id = 5;
    emit(&loss);
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[1], "RX:2 Loss:5") && strstr(label_texts[1], "Offset 000: 22"));
    assert(!strstr(label_texts[3], "lost") && !strstr(label_texts[3], "Loss:"));

    bytes[0] = 0x33;
    event.handle = 0x0041;
    event.session_id++;
    emit(&event); /* Do not attribute stale data to this session's directory. */
    clock_ms += NOTIFICATION_REDRAW_MS;
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[0], "Notify value 0x0035"));
    event.session_id--;
    emit(&event);
    s_lab.rows = 1; /* A short display must still show the bytes. */
    clock_ms += NOTIFICATION_REDRAW_MS;
    plugin->on_loop(plugin->context, 0);
    assert(strstr(label_texts[0], "Notify value 0x0041"));
    assert(strstr(label_texts[1], "Offset 000: 33"));
    assert(strstr(label_texts[2], "Service UUID: unknown\nChar UUID: unknown"));

    /* Enumerating Connected peers must not swallow notifications. */
    run_action(MENU_CONNECTED);
    assert(s_lab.pending_op == LOCAL_CONNECTED_LIST && !s_lab.list_valid);
    bytes[0] = 0x44;
    event.handle = 0x0035;
    event.session_id++;
    emit(&event);
    assert(s_lab.notifications == 3 && s_lab.value[0] == 0x33);
    event.session_id--;
    emit(&event);
    assert(s_lab.notifications == 4 && s_lab.notification_drops == 5);
    assert(s_lab.catalog[2].notifications == 2 && s_lab.value_handle == 0x0035 &&
           s_lab.value_length == 1 && s_lab.value[0] == 0x44);
    assert(s_lab.pending_op == LOCAL_CONNECTED_LIST && !s_lab.list_valid);

    gm_ble_event_t peer = event_for(GM_BLE_EVENT_CONNECTED_DEVICE);
    peer.device.address[0] = 9;
    peer.device.connected = 1;
    emit(&peer);
    peer.type = GM_BLE_EVENT_CONNECTED_DEVICES_DONE;
    emit(&peer);
    assert(!s_lab.pending_op && s_lab.list_valid && s_lab.device_count == 1);
    assert(s_lab.connected && s_lab.session == event.session_id && s_lab.notifications == 4);

    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
}
static void connect_rejection_hints(void)
{
    const uint8_t reasons[] = {GM_BLE_CONNECT_PLUGIN_LIMIT, GM_BLE_CONNECT_SLOT_UNAVAILABLE,
                               GM_BLE_CONNECT_BOND_LIMIT, 0};
    const char *hints[] = {"GATT device is already connected", "No free BLE accessory slot",
                           "Pairing records full. Unpair an accessory first.", "Radio busy"};
    const unsigned before = calls;
    s_lab.connected = true;
    s_lab.session = 123;
    s_lab.catalog_valid = true;
    for (unsigned i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++) {
        s_lab.pending_op = GM_BLE_OP_CONNECT;
        gm_ble_event_t event = {
            .type = GM_BLE_EVENT_RESULT, .operation = GM_BLE_OP_CONNECT,
            .result = GM_PLUGIN_EBUSY, .properties = reasons[i],
        };
        completed(&event);
        assert(strstr(s_lab.message, hints[i]));
        assert(!s_lab.pending_op && calls == before);
        assert(s_lab.connected && s_lab.session == 123 && s_lab.catalog_valid);
    }
}
static void connect_bond_limit_hint_tests(gm_plugin_descriptor_t *plugin)
{
    for (unsigned automatic = 0; automatic < 2; automatic++) {
        labels_created = 0;
        assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);
        reset_list(LIST_SCAN);
        gm_ble_device_t peer = {.address = {1}, .address_type = 1};
        memcpy(peer.name, "Test peer", 10);
        collect_device(&peer);
        s_lab.list_valid = true;
        s_lab.selected_device = 0;
        connect_selected(automatic != 0);
        assert(last_operation == GM_BLE_OP_CONNECT && s_lab.pending_op == GM_BLE_OP_CONNECT);
        const unsigned before = calls;
        gm_ble_event_t event = event_for(GM_BLE_EVENT_RESULT);
        event.result = GM_PLUGIN_EBUSY;
        event.properties = GM_BLE_CONNECT_BOND_LIMIT;
        event.request_id++;
        emit(&event);
        assert(s_lab.pending_op == GM_BLE_OP_CONNECT);
        assert(!strstr(s_lab.message, "Pairing records full"));
        event.request_id--;
        emit(&event);
        assert(strstr(s_lab.message, "Pairing records full. Unpair an accessory first."));
        assert(!s_lab.pending_op && s_lab.auto_stage == AUTO_NONE && calls == before);
        assert(!s_lab.connected && s_lab.client != 0);
        plugin->on_stop(plugin->context);
        plugin->on_unload(plugin->context);
    }
}
static void extended_bond_list_tests(gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_host_api_t *host = s_lab.host;
    bond_list_supported = true;
    plugin->struct_size = sizeof(*plugin);
    assert(gm_plugin_entry(host, plugin) == GM_PLUGIN_OK && s_lab.bond_list);
    labels_created = 0;
    assert(plugin->on_start(plugin->context) == GM_PLUGIN_OK);

    run_action(MENU_SAVED_DEVICES);
    assert(last_bond_list_was_extended && s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES);
    gm_ble_event_t bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device.address_type = 2;
    strcpy(bond.device.name, "Plugin bond");
    for (unsigned i = 0; i < GM_BLE_BOND_MAX; i++) {
        bond.device.address[0] = (uint8_t)(i + 1U);
        emit(&bond);
    }
    bond.properties = GM_BLE_BOND_SYSTEM_HOGP;
    bond.device.address[0] = 0xFF;
    strcpy(bond.device.name, "Shared remote");
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && page_count() == GM_BLE_BONDED_ACCESSORY_MAX);
    char label[128];
    row_text(GM_BLE_BOND_MAX, label, sizeof(label));
    assert(strstr(label, "App-bound HOGP"));

    const unsigned before_connect = calls;
    s_lab.selection = GM_BLE_BOND_MAX;
    activate();
    assert(calls == before_connect + 1 && last_operation == GM_BLE_OP_CONNECT);
    assert(last_device.address_type == 2 && last_device.address[0] == 0xFF);
    assert(!s_lab.scanning && !s_lab.confirm_enabled);
    result(GM_PLUGIN_EIO, 0); /* A radio failure must not affect the unpair policy below. */

    run_action(MENU_BONDS);
    assert(last_bond_list_was_extended && s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES);
    bond = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    bond.device.address_type = 2;
    bond.device.address[0] = 0xFF;
    bond.properties = GM_BLE_BOND_SYSTEM_HOGP;
    strcpy(bond.device.name, "Shared remote");
    emit(&bond);
    bond.properties = 0;
    bond.device.address[0] = 1;
    strcpy(bond.device.name, "Plugin bond");
    emit(&bond);
    result(GM_PLUGIN_OK, 0);
    assert(page_count() == 2);
    row_text(0, label, sizeof(label));
    assert(strstr(label, "App-bound HOGP"));

    s_lab.selection = 0;
    const unsigned before_unpair = calls;
    activate();
    assert(calls == before_unpair && !s_lab.confirm_enabled && s_lab.page == PAGE_BONDS);
    assert(strstr(s_lab.message, "unpair in App > Third-party Devices"));
    s_lab.selection = 1;
    activate();
    confirm(MENU_UNBOND);
    assert(last_operation == GM_BLE_OP_UNBOND && last_device.address[0] == 1);
    result(GM_PLUGIN_EPERM, 0); /* Ownership can change after list publication. */
    assert(!s_lab.bonds_valid && strstr(s_lab.message, "Cannot unpair here"));

    plugin->on_stop(plugin->context);
    plugin->on_unload(plugin->context);
    bond_list_supported = false;
}
int main(void)
{
    assert(legacy_call_ui_test() == 0);
    memset(&s_lab, 0, sizeof(s_lab));
    t_old_firmware = t_active = false;
    gm_plugin_host_api_t host = {0};
    gm_plugin_descriptor_t plugin = {0};
    plugin.struct_size = sizeof(plugin);
    host.struct_size = sizeof(host);
    host.abi_version = GM_PLUGIN_ABI_VERSION;
    host.capabilities = GM_PLUGIN_CAP_BUTTON;
    host.graphics.lvgl = &t_ui;
    host.extension_get = lab_get;
    host.display_get_info = t_info;
    host.monotonic_ms = lab_now;
    host.log = lab_log;
    host.app_exit = lab_exit;
    host.alloc = lab_alloc;
    host.free = lab_free;
    t_ui.label_create = lab_label;
    t_ui.label_set_text = lab_text;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK && opens == 0 && calls == 0);
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK && opens == 1 && calls == 0);
    assert(s_lab.rows > 0 && strstr(label_texts[1], "Scan"));
    assert(!s_lab.advanced && page_count() == sizeof(basic_menu));
    run_action(MENU_ADVANCED); /* Preserve API coverage independently of the trimmed menu. */
    assert(s_lab.advanced);

    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_SINGLE);
    assert(menu_action(s_lab.selection) == MENU_CATALOG && calls == 0);
    key(GM_PLUGIN_BUTTON_UP, GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(menu_action(s_lab.selection) == MENU_COMMON_SERVICES);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(!s_lab.advanced && menu_action(s_lab.selection) == MENU_SCAN);
    activate();
    assert(s_lab.scanning && last_operation == 200);
    key(GM_PLUGIN_BUTTON_PRIMARY, GM_PLUGIN_BUTTON_ACTION_SINGLE);
    activate(); /* Toggle Connect only in the same scan; leave automatic pairing out of this fixture. */
    gm_ble_event_t event = event_for(GM_BLE_EVENT_SCAN_RESULT);
    event.scan_id = 7;
    event.device.address[0] = 1;
    strcpy(event.device.name, "Test peer");
    emit(&event);
    assert(s_lab.device_count == 1 && !s_lab.list_valid);
    unsigned before = calls;
    run_action(MENU_STOP_SCAN);
    assert(calls == before + 1 && last_operation == 201 && s_lab.scanning);
    event.type = GM_BLE_EVENT_SCAN_ENDED;
    event.result = GM_PLUGIN_ENOMEM;
    emit(&event);
    assert(s_lab.list_valid && !s_lab.scanning && s_lab.client && closes == 0);
    assert(strstr(last_scan_log, "partial=1"));
    activate();
    assert(s_lab.page == PAGE_MENU && s_lab.selected_device == 0);
    assert(last_operation == GM_BLE_OP_CONNECT && s_lab.auto_stage == AUTO_NONE);
    event = event_for(GM_BLE_EVENT_RESULT);
    event.result = GM_PLUGIN_EBUSY;
    event.session_id = 17; /* Host can reject after assigning a new session. */
    emit(&event);
    assert(!s_lab.pending_op && !s_lab.connected && s_lab.session == 17);
    assert(s_lab.client && strstr(s_lab.message, "App-first is not required"));
    connect_peer();

    run_action(MENU_DISCOVER_ALL);
    metadata(GM_BLE_EVENT_CHARACTERISTIC, 3, 4);
    event = event_for(GM_BLE_EVENT_CHARACTERISTIC);
    event.request_id++;
    emit(&event);
    assert(s_lab.catalog_count == 1);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.catalog_valid && !s_lab.catalog_count);
    discover_catalog(false);
    run_action(MENU_READ);
    uint32_t pending = s_lab.pending_id;
    run_action(MENU_STOP_SCAN);
    assert(s_lab.pending_id == pending && s_lab.pending_op == GM_BLE_OP_READ);
    uint8_t data[3] = {44, 15, 1};
    event = event_for(GM_BLE_EVENT_RESULT);
    event.operation = GM_BLE_OP_READ;
    event.data = data;
    event.data_length = sizeof(data);
    emit(&event);
    data[0] = 0;
    assert(!s_lab.pending_op && s_lab.value[0] == 44 && s_lab.value_length == 3);

    submit_result = GM_PLUGIN_EBUSY;
    run_action(MENU_NOTIFY);
    assert(!s_lab.pending_op && !s_lab.catalog[1].subscribed);
    submit_result = GM_PLUGIN_OK;
    run_action(MENU_NOTIFY);
    assert(last_enable && !last_indicate && !s_lab.catalog[1].subscribed);
    result(GM_PLUGIN_EIO, 0);
    assert(!s_lab.catalog[1].subscribed);
    run_action(MENU_NOTIFY);
    result(GM_PLUGIN_OK, 1);
    assert(s_lab.catalog[1].subscribed);
    run_action(MENU_INDICATE);
    assert(last_enable && last_indicate);
    result(GM_PLUGIN_OK, 1);
    run_action(MENU_UNSUBSCRIBE);
    assert(!last_enable);
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.catalog[1].subscribed);

    s_lab.tx_length = 2;
    s_lab.tx[0] = 0xaa;
    s_lab.tx[1] = 0x55;
    before = calls;
    run_action(MENU_WRITE);
    activate(); /* Default Cancel must not send a write. */
    assert(calls == before);
    confirm(MENU_WRITE);
    assert(last_operation == GM_BLE_OP_WRITE && last_response && last_handle == 4);
    assert(last_length == 2 && last_bytes[0] == 0xaa && last_bytes[1] == 0x55);
    result(GM_PLUGIN_OK, 0);
    confirm(MENU_WRITE_COMMAND);
    assert(!last_response && last_operation == GM_BLE_OP_WRITE_NO_RESPONSE);
    result(GM_PLUGIN_OK, 0);
    assert(strstr(s_lab.message, "LOCAL SUBMISSION ONLY"));
    confirm(MENU_PAIR);
    assert(last_operation == GM_BLE_OP_PAIR && s_lab.confirm_length == 0);
    result(GM_PLUGIN_OK, 0);
    discover_catalog(true);

    run_action(MENU_READ);
    plugin.on_suspend(plugin.context);
    clock_ms += REQUEST_TIMEOUT_MS + 1;
    plugin.on_resume(plugin.context);
    plugin.on_loop(plugin.context, 0);
    assert(s_lab.client && s_lab.pending_op == GM_BLE_OP_READ);
    result(GM_PLUGIN_EIO, 0);

    run_action(MENU_STATE);
    gm_ble_session_state_t state = {sizeof(state), 2, 1, 1, 1, 1, 0, 1};
    uint8_t unaligned[sizeof(state) + 1];
    memcpy(unaligned + 1, &state, sizeof(state));
    event = event_for(GM_BLE_EVENT_SESSION_STATE);
    event.data = unaligned + 1;
    event.data_length = sizeof(state);
    emit(&event);
    assert(!s_lab.pending_op && strstr(s_lab.message, "encrypted=1"));

    unsigned updates = text_updates;
    run_action(MENU_READ);
    t_notify(true);
    clock_ms += REQUEST_TIMEOUT_MS + 1;
    event = event_for(GM_BLE_EVENT_NOTIFICATION);
    event.data = data;
    event.data_length = 3;
    emit(&event);
    assert(s_lab.notifications == 1);
    assert(!notification_logs);
    plugin.on_loop(plugin.context, 1);
    assert(text_updates == updates && t_hidden && s_lab.client);
    t_notify(false);
    plugin.on_loop(plugin.context, 0);
    assert(text_updates > updates && !t_hidden && s_lab.pending_op == GM_BLE_OP_READ);
    {
        unsigned closed_before_loss = closes;
        unsigned loss_logs = notification_loss_logs;
        uint32_t current_client = s_lab.client;
        uint32_t current_session = s_lab.session;
        uint32_t pending_read = s_lab.pending_id;
        gm_ble_event_t loss = event_for(GM_BLE_EVENT_DATA_LOSS);
        loss.properties = GM_BLE_DATA_LOSS_NOTIFICATION_ONLY;
        loss.request_id = 3;
        emit(&loss);
        plugin.on_loop(plugin.context, 0);
        assert(notification_loss_logs == loss_logs + 1);
        assert(!strstr(label_texts[3], "lost") && !strstr(label_texts[3], "Loss:"));
        loss.request_id = 2;
        emit(&loss);
        loss.session_id++;
        emit(&loss); /* A loss for another session changes nothing. */
        plugin.on_loop(plugin.context, 0);
        assert(s_lab.notification_drops == 5 && notification_loss_logs == loss_logs + 1);
        clock_ms += NOTIFICATION_LOSS_LOG_MS;
        plugin.on_loop(plugin.context, 0);
        assert(notification_loss_logs == loss_logs + 2);
        assert(closes == closed_before_loss && s_lab.connected && s_lab.catalog_valid);
        assert(s_lab.client == current_client && s_lab.session == current_session);
        assert(s_lab.pending_op == GM_BLE_OP_READ && s_lab.pending_id == pending_read);
        assert(!notification_logs);
    }
    result(GM_PLUGIN_EIO, 0);
    run_action(MENU_PAIR);
    plugin.on_suspend(plugin.context);
    assert(!s_lab.confirm_enabled && s_lab.page == PAGE_CHARACTERISTIC);
    plugin.on_resume(plugin.context);

    run_action(MENU_DISCONNECT);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.connected && s_lab.pending_op == LOCAL_DISCONNECT_WAIT);
    event = event_for(GM_BLE_EVENT_CONNECTION);
    event.device.connected = 0;
    emit(&event);
    assert(!s_lab.connected && s_lab.session == 41 && !s_lab.catalog_valid);
    run_action(MENU_BONDS);
    assert(!s_lab.bonds_valid);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && !s_lab.bond_count);
    assert(strstr(s_lab.message, "No plugin bonds") && strstr(s_lab.message, "unpair in App"));
    render();
    assert(strstr(label_texts[2], "No plugin bonds"));
    before = calls;
    activate(); /* Repeated clicks on an empty Unpair page keep the explanation. */
    assert(calls == before && !s_lab.confirm_enabled);
    assert(strstr(s_lab.message, "No plugin bonds"));
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address[0] = 8;
    event.device.address_type = 2;
    emit(&event);
    assert(!s_lab.bonds_valid);
    result(GM_PLUGIN_EBUSY, 0);
    assert(!s_lab.bonds_valid && !s_lab.bond_count);
    assert(!strstr(s_lab.message, "No plugin bonds")); /* A failed query is not an empty success. */
    run_action(MENU_BONDS);
    event = event_for(GM_BLE_EVENT_BONDED_DEVICE);
    event.device.address[0] = 8;
    event.device.address_type = 2;
    emit(&event);
    result(GM_PLUGIN_OK, 0);
    assert(s_lab.bonds_valid && s_lab.bond_count == 1 && !strstr(s_lab.message, "No plugin bonds"));
    s_lab.page = PAGE_BONDS;
    s_lab.selection = 0;
    activate();
    confirm(MENU_UNBOND);
    assert(last_operation == GM_BLE_OP_UNBOND && last_device.address_type == 2);
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.bonds_valid);

    connect_peer();
    run_action(MENU_CONNECTED);
    event = event_for(GM_BLE_EVENT_CONNECTION);
    emit(&event);
    assert(!s_lab.connected && !s_lab.pending_op); /* Drop during enumeration. */
    run_action(MENU_CONNECTED);
    event = event_for(GM_BLE_EVENT_CONNECTED_DEVICE);
    event.device.address[0] = 1;
    event.device.connected = 1;
    emit(&event);
    event.type = GM_BLE_EVENT_CONNECTED_DEVICES_DONE;
    emit(&event);
    assert(s_lab.list_valid && s_lab.device_count == 1);

    if (!s_lab.advanced) {
        run_action(MENU_ADVANCED);
    }
    event = event_for(GM_BLE_EVENT_DATA_LOSS);
    unsigned closed = closes;
    event.properties = UINT8_MAX; /* Unknown loss still fails closed. */
    emit(&event);
    assert(closes == closed + 1 && !s_lab.client && !s_lab.catalog_valid && !s_lab.list_valid);
    assert(!s_lab.advanced && menu_action(s_lab.selection) == MENU_REOPEN);
    activate(); /* Recovery from diagnostics offers Retry client in the basic menu. */
    assert(s_lab.client && menu_action(0) == MENU_SCAN);
    run_action(MENU_CONNECTED);
    clock_ms += REQUEST_TIMEOUT_MS + 1;
    plugin.on_loop(plugin.context, 1);
    assert(!s_lab.client && strstr(s_lab.message, "Timed out"));
    before = opens;
    plugin.on_loop(plugin.context, 1);
    assert(opens == before); /* No auto retry or reconnect. */

    s_lab.page = PAGE_HEX;
    s_lab.tx_length = 1;
    s_lab.tx[0] = 0;
    s_lab.cursor = 0;
    s_lab.selection = 1;
    editor_activate();
    assert(s_lab.cursor == 1);
    s_lab.selection = 2;
    editor_activate();
    assert(s_lab.tx[0] == 1);
    s_lab.selection = 4;
    for (unsigned i = 0; i < 100; i++) {
        editor_activate();
    }
    assert(s_lab.tx_length == 64);
    gm_plugin_event_t button = {0};
    button.struct_size = sizeof(button);
    button.type = GM_PLUGIN_EVENT_BUTTON;
    button.data.button.button = GM_PLUGIN_BUTTON_PRIMARY;
    button.data.button.action = GM_PLUGIN_BUTTON_ACTION_RELEASE;
    assert(!plugin.on_event(plugin.context, &button));
    button.data.button.action = GM_PLUGIN_BUTTON_ACTION_LONG;
    assert(plugin.on_event(plugin.context, &button) && s_lab.page == PAGE_MENU);

    labels_created = 0;
    plugin.on_stop(plugin.context);
    supported = false;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK);
    assert(!s_lab.client && strstr(s_lab.message, "unsupported"));
    plugin.on_stop(plugin.context);
    supported = true;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    labels_created = 0;
    fail_label = 3;
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_ENOMEM);
    assert(!s_lab.started && !s_lab.client && !s_call_ui.root && !t_callback);
    fail_label = 0;
    labels_created = 0;
    open_result = GM_PLUGIN_EBUSY;
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK && !s_lab.client);
    open_result = GM_PLUGIN_OK;
    run_action(MENU_REOPEN);
    assert(s_lab.client);
    connect_peer();
    discover_catalog(false);
    run_action(MENU_READ);
    event = event_for(GM_BLE_EVENT_RESULT);
    event.operation = GM_BLE_OP_READ;
    event.data = NULL;
    event.data_length = 1;
    emit(&event);
    assert(!s_lab.client && strstr(s_lab.message, "Invalid value"));
    run_action(MENU_REOPEN);
    connect_peer();
    run_action(MENU_DISCOVER_ALL);
    for (unsigned i = 0; i < CATALOG_LIMIT + 1U; i++) {
        metadata(GM_BLE_EVENT_CHARACTERISTIC, (uint16_t)(i + 1U), (uint16_t)(i + 2U));
    }
    result(GM_PLUGIN_OK, 0);
    assert(!s_lab.catalog_valid && !s_lab.catalog_count && !s_lab.catalog);
    assert(s_lab.connected && strstr(s_lab.message, "exceeds supported capacity"));
    key(GM_PLUGIN_BUTTON_HOME, GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(exits == 1);
    plugin.on_stop(plugin.context);
    plugin.on_unload(plugin.context);
    assert(!ble_callback && !t_callback && !s_lab.client);
    live_scan_connect_tests(&plugin);
    scan_connect_watchdog_tests(&plugin);
    scan_connect_mode_tests(&plugin);
    scan_page_lifecycle_tests(&plugin);
    named_scan_order_tests(&plugin);
    named_scan_pressure_tests(&plugin);
    scan_cursor_tests(&plugin);
    refresh_catalog_tests(&plugin);
    catalog_allocation_tests(&plugin);
    automation_tests(&plugin);
    streaming_scan_tests(&plugin);
    coexist_lifecycle_tests(&plugin);
    simple_menu_tests(&plugin);
    characteristic_page_tests(&plugin);
    common_service_tests(&plugin);
    page_status_tests(&plugin);
    device_reconnect_tests(&plugin);
    retained_peer_selection_tests(&plugin);
    unpair_history_tests(&plugin);
    online_unpair_tests(&plugin);
    saved_devices_tests(&plugin);
    extended_bond_list_tests(&plugin);
    connect_bond_limit_hint_tests(&plugin);
    notification_source_preview_tests(&plugin);
    connect_rejection_hints();
    puts("BLE GATT Lab: PASS (actions/confirmations/async results/loss/lifecycle/call UI)");
    return 0;
}
