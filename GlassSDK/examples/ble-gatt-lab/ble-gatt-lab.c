#include "call_ui.h"
#include "gm_plugin_ble.h"
#include "gm_plugin_libc.h"

#define DEVICE_INITIAL_CAPACITY 16U
#define DEVICE_STORAGE_BYTES (32U * 1024U)
#define SCAN_DURATION_MS 30000U
#define SCAN_WATCHDOG_MS 35000U
#define CATALOG_LIMIT GM_BLE_GATT_MAX_ATTRIBUTES
#define REQUEST_TIMEOUT_MS 25000U
#define LOCAL_CONNECTED_LIST 100U
#define LOCAL_DISCONNECT_WAIT 101U
#define SECURITY_POLLS 10U
#define SECURITY_POLL_MS 200U
#define NOTIFICATION_LOSS_LOG_MS 1000U
#define NOTIFICATION_REDRAW_MS 200U
#define SELECT_LAST_PEER (-2)
#define CHARACTERISTIC_ACTION_MAX 7U
#define SCAN_MODE_ROW 1U

enum {
    PAGE_MENU, PAGE_DEVICES, PAGE_CATALOG, PAGE_COMMON_SERVICES, PAGE_HEX, PAGE_VALUE, PAGE_CONFIRM,
    PAGE_GATT, PAGE_CHARACTERISTIC, PAGE_BONDS, PAGE_SAVED_DEVICES
};
enum { LIST_SCAN = 1, LIST_CONNECTED };
enum { CATALOG_OK, CATALOG_NO_MEMORY, CATALOG_TOO_LARGE };
enum { UNPAIR_NONE, UNPAIR_VERIFY, UNPAIR_WAIT_DOWN, UNPAIR_REMOVE };
enum {
    MENU_SCAN,
    MENU_STOP_SCAN,
    MENU_DEVICES,
    MENU_CONNECTED,
    MENU_BONDS,
    MENU_CONNECT,
    MENU_DISCOVER_ALL,
    MENU_COMMON_SERVICES,
    MENU_CATALOG,
    MENU_READ,
    MENU_NOTIFY,
    MENU_INDICATE,
    MENU_UNSUBSCRIBE,
    MENU_HEX,
    MENU_WRITE,
    MENU_WRITE_COMMAND,
    MENU_PAIR,
    MENU_STATE,
    MENU_DISCONNECT,
    MENU_UNBOND,
    MENU_VALUE,
    MENU_REOPEN,
    MENU_EXIT,
    MENU_AUTO_CONNECT,
    MENU_ADVANCED,
    MENU_REFRESH_ALL,
    MENU_GATT,
    MENU_SAVED_DEVICES,
    MENU_COUNT
};
enum {
    AUTO_NONE,
    AUTO_PAIR,
    AUTO_SECURITY,
    AUTO_DISCOVER,
    AUTO_READ,
    AUTO_SUBSCRIBE,
    AUTO_UNSUBSCRIBE,
    AUTO_STATE,
    AUTO_BONDS
};
typedef struct {
    uint16_t type, handle, value, end;
    gm_ble_uuid_t uuid;
    uint8_t properties, subscribed;
    uint8_t subscription_known, result_state, last_operation, att_error;
    uint16_t read_length;
    gm_plugin_result_t last_result;
    uint32_t notifications;
} attribute_t;
typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    const gm_plugin_ble_gatt_extension_api_t *ble;
    const gm_plugin_ble_gatt_refresh_extension_api_t *refresh;
    const gm_plugin_ble_gatt_bond_list_extension_api_t *bond_list;
    gm_plugin_lvgl_obj_t *title, *body, *status, *hint;
    gm_ble_capabilities_t caps;
    gm_ble_device_t *devices;
    gm_ble_device_t bonds[GM_BLE_BONDED_ACCESSORY_MAX];
    uint8_t bond_flags[GM_BLE_BONDED_ACCESSORY_MAX];
    gm_ble_device_t peer, confirm_peer;
    attribute_t *catalog;
    uint32_t client, session, sequence, pending_id, pending_since, scan_id, scan_since;
    uint32_t notifications, confirm_session, scan_reports;
    uint32_t notification_drops, notification_drops_logged, notification_log_at;
    uint32_t notification_rendered_at;
    uint32_t value_session;
    uint32_t view_id, pending_view, scan_view;
    uint16_t pending_handle;
    uint16_t mtu, value_length, value_offset, value_handle, confirm_handle, tx_length,
        confirm_length;
    uint16_t selection, cursor, device_count, device_capacity, device_replace;
    uint16_t auto_service_start, auto_service_end;
    int16_t selected_device, selected_attribute, selected_bond;
    uint8_t bond_count;
    uint8_t page, list_kind, catalog_count, pending_op, confirm_action, rows, value_type;
    uint8_t catalog_error, unpair_stage;
    uint8_t tx[GM_BLE_WRITE_MAX], confirm_tx[GM_BLE_WRITE_MAX], value[GM_BLE_VALUE_MAX];
    bool started, suspended, dirty, connected, scanning, list_valid, catalog_valid;
    bool notification_redraw_pending;
    bool overflow, confirm_enabled, advanced, auto_unsubscribe_tested;
    bool catalog_all, gatt_view, edit_write, pending_gatt;
    bool bonds_valid, bonds_overflow, peer_known;
    bool scan_stopping, bonds_after_scan, scan_connect_only;
    uint8_t auto_stage, auto_index, auto_reads, auto_subscriptions, auto_errors, security_polls;
    uint32_t security_polled_at;
    char message[176], screen[512];
} lab_t;

static lab_t s_lab;
static example_call_ui_t s_call_ui;
static const char *const menu_names[] = {"Scan all BLE (30 seconds)",
                                         "Stop scan",
                                         "Devices / Connect",
                                         "Connected peers",
                                         "Owned bonds / Unpair",
                                         "Manual connect (no auto checks)",
                                         "Full GATT (reuse session cache)",
                                         "Find common services",
                                         "Raw GATT attributes",
                                         "Read selected value",
                                         "Subscribe Notify",
                                         "Subscribe Indicate",
                                         "Unsubscribe",
                                         "Edit write HEX bytes",
                                         "Write Request (confirm)",
                                         "Write Command (confirm)",
                                         "Retry pairing (confirm)",
                                         "Get session state",
                                         "Disconnect / detach",
                                         "Unbond owned peer (confirm)",
                                         "Last value / notifications",
                                         "Retry client",
                                         "Exit",
                                         "Connect + Auto checks",
                                         "Back to main menu",
                                         "Refresh from device",
                                         "GATT services",
                                         "Devices"};
static const uint8_t basic_menu[] = {
    MENU_SCAN,
    MENU_SAVED_DEVICES,
    MENU_DISCONNECT,
    MENU_BONDS,
    MENU_VALUE,
    MENU_GATT,
    MENU_EXIT,
};
/* Keep only diagnostics not available on the ordinary pages. */
static const uint8_t advanced_menu[] = {
    MENU_COMMON_SERVICES,
    MENU_CATALOG,
    MENU_STATE,
    MENU_ADVANCED,
};
static const struct {
    const char *label;
    gm_ble_uuid_t uuid;
} common_services[] = {
    {"Battery (0x180F)", {2, {0x0f, 0x18}}},
    {"Device Information (0x180A)", {2, {0x0a, 0x18}}},
    {"HID (0x1812)", {2, {0x12, 0x18}}},
};
static const char *const auto_names[] = {"Idle",
                                         "Pair / restore encryption",
                                         "Confirm encryption",
                                         "Discover full GATT",
                                         "Read values",
                                         "Subscribe",
                                         "Unsubscribe test",
                                         "Query session",
                                         "Read paired devices"};
static const char digits[] = "0123456789ABCDEF";
static const char no_plugin_bonds[] =
    "No plugin bonds to unpair.\nSystem HOGP: unpair in App.";
static const char no_saved_devices[] =
    "No saved GATT devices. Use Scan & Connect.\nSystem HOGP is managed in App.";
static const char unpair_in_app[] = "App-bound HOGP: unpair in App > Third-party Devices.";
static const char unpair_unverified[] =
    "Cannot verify this live peer. Disconnect it first, then retry Unpair.";
static const char plugin_limit[] = "A GATT device is already connected. Disconnect it first.";
static void render(void);
static void run_action(uint8_t action);
static void on_ble(void *context, const gm_ble_event_t *event);
static void handle_ble(void *context, const gm_ble_event_t *event);
static void auto_pump(void);
static attribute_t *selected_characteristic(void);
static attribute_t *characteristic_by_handle(uint16_t handle);
static void format_operation(const attribute_t *record, char *out, unsigned size);
static void reset_bonds(void);
static void reset_results(void);
static bool device_list_ready(void);
static unsigned page_count(void);

static const char *auto_name(void)
{
    return s_lab.auto_stage < sizeof(auto_names) / sizeof(auto_names[0])
               ? auto_names[s_lab.auto_stage]
               : "Unknown";
}

static unsigned menu_action(unsigned index)
{
    unsigned action = s_lab.advanced ? advanced_menu[index] : basic_menu[index];
    if (action == MENU_SCAN) {
        if (!s_lab.client) {
            return MENU_REOPEN;
        }
        if (s_lab.scanning) {
            return MENU_STOP_SCAN;
        }
    }
    return action;
}
static const char *menu_label(unsigned action)
{
    if (!s_lab.advanced) {
        switch (action) {
        case MENU_SCAN:
            return "Scan & Connect";
        case MENU_SAVED_DEVICES:
            return "Devices";
        case MENU_DISCONNECT:
            return "Disconnect";
        case MENU_BONDS:
            return "Unpair";
        case MENU_VALUE:
            return "Results";
        case MENU_GATT:
            return "GATT services";
        default:
            break;
        }
    }
    return menu_names[action];
}
static void menu_select(unsigned action)
{
    s_lab.selection = 0;
    const uint8_t *menu = s_lab.advanced ? advanced_menu : basic_menu;
    unsigned count = s_lab.advanced ? sizeof(advanced_menu) : sizeof(basic_menu);
    for (unsigned i = 0; i < count; i++) {
        if (menu[i] == action) {
            s_lab.selection = (uint16_t)i;
            break;
        }
    }
}

static void say(const char *text)
{
    s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message), "%s", text);
    s_lab.dirty = true;
}
static void page_hint(void)
{
    switch (s_lab.page) {
    case PAGE_MENU:
        if (s_lab.auto_stage != AUTO_NONE) {
            s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                                 "Auto: %s\nReads=%u subscriptions=%u errors=%u", auto_name(),
                                 (unsigned)s_lab.auto_reads, (unsigned)s_lab.auto_subscriptions,
                                 (unsigned)s_lab.auto_errors);
            s_lab.dirty = true;
        } else {
            say(s_lab.pending_op ? "Operation in progress. Disconnect cancels."
                : s_lab.advanced ? "GATT diagnostics."
                : s_lab.scanning ? "Scanning. Stop scan or open Devices."
                                 : "Scan & Connect, or select a page.");
        }
        break;
    case PAGE_DEVICES:
        say(s_lab.scanning ? "Scanning. Double-click a device to connect now."
            : s_lab.list_kind == LIST_CONNECTED ? "Connected-peer snapshot. Select a device to attach."
            : "Recent scan results + last peer (not live status). Select a device to connect.");
        break;
    case PAGE_SAVED_DEVICES:
        say(s_lab.scanning ? "Stopping scan; saved devices will load next."
            : s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES
                ? (s_lab.bond_list ? "Loading paired devices..." : "Loading saved GATT devices...")
            : !s_lab.bonds_valid ? "List unavailable. Reopen Devices to retry."
            : !s_lab.bond_count ? no_saved_devices
            : "Paired devices (not live status). Double-click to connect.");
        break;
    case PAGE_BONDS:
        say(s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES ? "Loading paired devices..."
            : s_lab.bonds_valid && !s_lab.bond_count
                ? no_plugin_bonds : "Select a plugin bond. A verified live connection is detached first.");
        break;
    case PAGE_GATT:
        say(s_lab.pending_op == GM_BLE_OP_DISCOVER_ALL ? "Loading full GATT catalog..."
            : !s_lab.connected ? "Connect a device first. More tools are available below."
            : s_lab.pending_op || s_lab.auto_stage != AUTO_NONE ? "Wait for the active operation."
            : "Select a characteristic. R=Read W=Write N=Notify I=Indicate.");
        break;
    case PAGE_CHARACTERISTIC: {
        const attribute_t *record = selected_characteristic();
        if (record) {
            format_operation(record, s_lab.message, sizeof(s_lab.message));
            s_lab.dirty = true;
        } else {
            say("No valid characteristic. Return to GATT services.");
        }
        break;
    }
    case PAGE_CATALOG:
        if (s_lab.pending_op == GM_BLE_OP_DISCOVER) {
            say("Finding service. Waiting for the device...");
        } else if (s_lab.catalog_valid) {
            say(s_lab.catalog_all ? "Raw attributes: select a characteristic value."
                                  : "Select a characteristic in this service.");
        } else {
            say("No complete catalog. Discover or refresh first.");
        }
        break;
    case PAGE_COMMON_SERVICES:
        say("Select a service to find on the connected device.");
        break;
    default:
        say(""); /* Editors, confirmations and values render their own current context. */
        break;
    }
}
static gm_plugin_result_t submit_scan_stop(void)
{
    gm_plugin_result_t result = s_lab.ble->scan_stop(s_lab.client);
    s_lab.scan_stopping = result == GM_PLUGIN_OK;
    return result;
}
static void stop_scan(void)
{
    if (!s_lab.client || !s_lab.scanning) {
        return;
    }
    if (s_lab.scan_stopping) {
        say("Stopping scan; waiting for SCAN_ENDED.");
        return;
    }
    gm_plugin_result_t result = submit_scan_stop();
    s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                         result == GM_PLUGIN_OK ? "Stopping scan; waiting for SCAN_ENDED."
                                                : "Stop scan rejected rc=%ld; use Stop scan or Unpair to retry.",
                         (long)result);
    s_lab.dirty = true;
}
static bool scan_list_visible(void)
{
    return s_lab.page == PAGE_DEVICES && s_lab.list_kind == LIST_SCAN;
}

static void enter_page(uint8_t page)
{
    s_lab.page = page;
    if (page != PAGE_BONDS && page != PAGE_SAVED_DEVICES) {
        s_lab.bonds_after_scan = false;
    }
    s_lab.view_id++;
    s_lab.selection = s_lab.cursor = 0;
    page_hint();
    if (page != PAGE_DEVICES && s_lab.pending_op != GM_BLE_OP_CONNECT) {
        stop_scan();
    }
}
static void invalidate_catalog(void)
{
    if (s_lab.catalog != NULL) {
        s_lab.host->free(s_lab.catalog);
        s_lab.catalog = NULL;
    }
    s_lab.catalog_valid = false;
    s_lab.catalog_count = 0;
    s_lab.catalog_error = CATALOG_OK;
    s_lab.selected_attribute = -1;
    s_lab.catalog_all = false;
    s_lab.auto_service_start = s_lab.auto_service_end = 0;
    s_lab.gatt_view = s_lab.edit_write = false;
    s_lab.value_session = 0;
}
static void close_client(void)
{
    if (s_lab.client != 0) {
        s_lab.ble->close(s_lab.client);
    }
    s_lab.client = s_lab.session = 0;
    s_lab.connected = s_lab.scanning = false;
    s_lab.scan_stopping = s_lab.bonds_after_scan = false;
    s_lab.pending_op = 0;
    s_lab.auto_stage = AUTO_NONE;
    s_lab.unpair_stage = UNPAIR_NONE;
    if (s_lab.devices != NULL) {
        s_lab.host->free(s_lab.devices);
        s_lab.devices = NULL;
    }
    s_lab.device_capacity = 0;
    s_lab.device_count = 0;
    s_lab.device_replace = 0;
    s_lab.list_valid = false;
    s_lab.selected_device = -1;
    s_lab.peer_known = false;
    reset_bonds();
    s_lab.confirm_enabled = false;
    invalidate_catalog();
}
static void quarantine(const char *reason)
{
    close_client();
    s_lab.advanced = false; /* Retry client is on the main page. */
    enter_page(PAGE_MENU);
    menu_select(MENU_REOPEN);
    say(reason);
}
static void open_client(void)
{
    if (s_lab.ble == NULL) {
        say("BLE GATT ID 6 unsupported. Install matching firmware; no radio calls made.");
        return;
    }
    gm_plugin_result_t result = s_lab.ble->get_capabilities(&s_lab.caps);
    if (result == GM_PLUGIN_OK && s_lab.caps.struct_size != sizeof(s_lab.caps)) {
        result = GM_PLUGIN_EVERSION;
    }
    if (result == GM_PLUGIN_OK) {
        result = s_lab.ble->open(on_ble, &s_lab, &s_lab.client);
    }
    if (result == GM_PLUGIN_OK) {
        say("Ready. Scan, then double-click a device to connect and run checks.");
    } else {
        s_lab.client = 0;
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             "Open rejected rc=%ld; select Retry client.", (long)result);
        s_lab.dirty = true;
    }
}
static uint32_t begin(uint8_t operation)
{
    s_lab.sequence++;
    if (s_lab.sequence == 0) {
        s_lab.sequence++;
    }
    s_lab.pending_op = operation;
    s_lab.pending_id =
        operation == LOCAL_CONNECTED_LIST || operation == GM_BLE_OP_DISCONNECT ? 0 : s_lab.sequence;
    s_lab.pending_since = s_lab.host->monotonic_ms();
    s_lab.pending_view = s_lab.view_id;
    s_lab.pending_handle = 0;
    s_lab.pending_gatt = s_lab.gatt_view;
    if (operation == GM_BLE_OP_READ || operation == GM_BLE_OP_WRITE ||
        operation == GM_BLE_OP_WRITE_NO_RESPONSE || operation == GM_BLE_OP_SUBSCRIBE ||
        operation == GM_BLE_OP_UNSUBSCRIBE) {
        attribute_t *record = selected_characteristic();
        if (record) {
            s_lab.pending_handle = record->value;
        }
    }
    return s_lab.pending_id;
}
static void auto_stop(const char *reason)
{
    s_lab.host->log("[ble_lab] AUTO stopped stage=%u reads=%u subscriptions=%u errors=%u: %s",
                    (unsigned)s_lab.auto_stage, (unsigned)s_lab.auto_reads,
                    (unsigned)s_lab.auto_subscriptions, (unsigned)s_lab.auto_errors, reason);
    s_lab.auto_stage = AUTO_NONE;
    say(reason);
}
static void submitted(gm_plugin_result_t result)
{
    attribute_t *record = characteristic_by_handle(s_lab.pending_handle);
    if (record) {
        record->last_operation = s_lab.pending_op;
        record->last_result = result;
        record->att_error = 0;
        record->result_state = result == GM_PLUGIN_OK ? 1 : 2;
    }
    if (s_lab.advanced) {
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             result == GM_PLUGIN_OK ? "Accepted op=%u id=%lu; WAITING for async result."
                                                    : "Submission REJECTED op=%u id=%lu rc=%ld.",
                             (unsigned)s_lab.pending_op, (unsigned long)s_lab.pending_id, (long)result);
    } else if (result == GM_PLUGIN_OK) {
        say("Working; waiting for device response.");
    } else {
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             "Request rejected (%ld). See log for details.", (long)result);
    }
    if (result != GM_PLUGIN_OK) {
        if (s_lab.auto_stage != AUTO_NONE) {
            s_lab.host->log("[ble_lab] AUTO rejected stage=%s op=%u id=%lu rc=%ld", auto_name(),
                            (unsigned)s_lab.pending_op, (unsigned long)s_lab.pending_id,
                            (long)result);
            s_lab.auto_errors++;
            auto_stop("Auto stopped: submission rejected. See log; no automatic retry.");
        }
        s_lab.pending_op = 0;
    } else if (s_lab.auto_stage != AUTO_NONE) {
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             "Auto: %s\nWaiting for result; Disconnect cancels.", auto_name());
    } else if (record) {
        format_operation(record, s_lab.message, sizeof(s_lab.message));
    }
    s_lab.dirty = true;
}

static void cancel_unpair(const char *reason)
{
    s_lab.unpair_stage = UNPAIR_NONE;
    s_lab.pending_op = 0;
    say(reason);
}

static void submit_unbond(void)
{
    uint32_t id = begin(GM_BLE_OP_UNBOND);
    gm_plugin_result_t result = s_lab.ble->unbond(s_lab.client, &s_lab.confirm_peer, id);
    submitted(result);
    if (result != GM_PLUGIN_OK) {
        s_lab.unpair_stage = UNPAIR_NONE;
    }
}

static void reset_list(uint8_t kind)
{
    s_lab.list_kind = kind;
    s_lab.scan_connect_only = false;
    s_lab.device_count = 0;
    s_lab.device_replace = 0;
    s_lab.selected_device = -1;
    s_lab.list_valid = s_lab.overflow = false;
}
static void reset_bonds(void)
{
    s_lab.bond_count = 0;
    s_lab.selected_bond = -1;
    s_lab.bonds_valid = s_lab.bonds_overflow = false;
}
static gm_plugin_result_t get_bond_list(uint32_t request)
{
    return s_lab.bond_list ? s_lab.bond_list->get_bonded_accessories(s_lab.client, request)
                           : s_lab.ble->get_bonded_devices(s_lab.client, request);
}
static void collect_bond(const gm_ble_event_t *event)
{
    const gm_ble_device_t *device = &event->device;
    unsigned index;
    for (index = 0; index < s_lab.bond_count; index++) {
        if (s_lab.bonds[index].address_type == device->address_type &&
            !s_lab.libc->memcmp(s_lab.bonds[index].address, device->address, 6)) {
            break;
        }
    }
    unsigned limit = s_lab.bond_list ? GM_BLE_BONDED_ACCESSORY_MAX : GM_BLE_BOND_MAX;
    if (index == limit) {
        s_lab.bonds_overflow = true;
        return;
    }
    if (index == s_lab.bond_count) {
        s_lab.bond_flags[index] = 0;
        s_lab.bond_count++;
    }
    if (s_lab.bond_list) {
        s_lab.bond_flags[index] |= event->properties & GM_BLE_BOND_SYSTEM_HOGP;
    }
    s_lab.libc->memcpy(&s_lab.bonds[index], device, sizeof(*device));
    s_lab.bonds[index].name[GM_BLE_NAME_MAX] = 0;
    s_lab.dirty = true;
}
static unsigned moved_device_index(unsigned index, unsigned from, unsigned to)
{
    if (index == from) {
        return to;
    }
    if (from < to && index > from && index <= to) {
        return index - 1U;
    }
    if (from > to && index >= to && index < from) {
        return index + 1U;
    }
    return index;
}
static void move_scan_device(unsigned from, unsigned to)
{
    if (from == to) {
        return;
    }
    gm_ble_device_t device;
    s_lab.libc->memcpy(&device, &s_lab.devices[from], sizeof(device));
    if (from < to) {
        s_lab.libc->memmove(&s_lab.devices[from], &s_lab.devices[from + 1U],
                            (to - from) * sizeof(device));
    } else {
        s_lab.libc->memmove(&s_lab.devices[to + 1U], &s_lab.devices[to],
                            (from - to) * sizeof(device));
    }
    s_lab.libc->memcpy(&s_lab.devices[to], &device, sizeof(device));
    /* Saved actions follow the peer; the browsing cursor stays on its row so
     * incoming named peers cannot keep pushing it down the live scan list. */
    if (s_lab.selected_device >= 0) {
        s_lab.selected_device = (int16_t)moved_device_index(
            (unsigned)s_lab.selected_device, from, to);
    }
    s_lab.device_replace = (uint16_t)moved_device_index(s_lab.device_replace, from, to);
}
static void collect_device(const gm_ble_device_t *device)
{
    unsigned index;
    for (index = 0; index < s_lab.device_count; index++) {
        if (s_lab.devices[index].address_type == device->address_type &&
            s_lab.libc->memcmp(s_lab.devices[index].address, device->address, 6) == 0) {
            break;
        }
    }
    bool new_device = index == s_lab.device_count;
    if (index == s_lab.device_count && index == s_lab.device_capacity) {
        unsigned capacity =
            s_lab.device_capacity ? s_lab.device_capacity * 2U : DEVICE_INITIAL_CAPACITY;
        unsigned maximum = DEVICE_STORAGE_BYTES / sizeof(*s_lab.devices);
        if (capacity > maximum) {
            capacity = maximum;
        }
        gm_ble_device_t *larger = NULL;
        if (!s_lab.overflow && capacity > s_lab.device_capacity) {
            larger = s_lab.host->alloc(capacity * sizeof(*larger));
        }
        if (larger != NULL) {
            if (s_lab.device_count) {
                s_lab.libc->memcpy(larger, s_lab.devices, s_lab.device_count * sizeof(*larger));
            }
            if (s_lab.devices != NULL) {
                s_lab.host->free(s_lab.devices);
            }
            s_lab.devices = larger;
            s_lab.device_capacity = (uint16_t)capacity;
        } else {
            s_lab.overflow = true;
            if (s_lab.list_kind != LIST_SCAN || !s_lab.device_count) {
                return; /* Never adopt a partial management enumeration. */
            }
            index = s_lab.device_replace;
            s_lab.device_replace = (uint16_t)((index + 1U) % s_lab.device_count);
            if (s_lab.selected_device == (int16_t)index) {
                s_lab.selected_device = -1;
            }
            s_lab.libc->memset(&s_lab.devices[index], 0, sizeof(*device));
        }
    }
    if (index == s_lab.device_count) {
        s_lab.libc->memset(&s_lab.devices[index], 0, sizeof(*device));
        s_lab.device_count++;
    }
    bool was_named = s_lab.devices[index].name[0] != '\0';
    bool name_changed = false;
    if (device->name[0]) {
        for (unsigned i = 0; i <= GM_BLE_NAME_MAX; i++) {
            if (device->name[i] != s_lab.devices[index].name[i]) {
                name_changed = true;
                break;
            }
            if (device->name[i] == '\0') {
                break;
            }
        }
    }
    /* A re-admitted advertisement can lack its earlier SCAN_RSP name. */
    char saved_name[GM_BLE_NAME_MAX + 1U];
    bool keep_name = index < s_lab.device_count && device->name[0] == '\0';
    if (keep_name) {
        s_lab.libc->memcpy(saved_name, s_lab.devices[index].name, sizeof(saved_name));
    }
    s_lab.libc->memcpy(&s_lab.devices[index], device, sizeof(*device));
    if (keep_name) {
        s_lab.libc->memcpy(s_lab.devices[index].name, saved_name, sizeof(saved_name));
    }
    s_lab.devices[index].name[GM_BLE_NAME_MAX] = 0;
    if (s_lab.list_kind == LIST_SCAN && (new_device || (!was_named && device->name[0]))) {
        unsigned target = s_lab.device_count - 1U;
        if (s_lab.devices[index].name[0]) {
            target = 0;
            for (unsigned i = 0; i < s_lab.device_count; i++) {
                target += i != index && s_lab.devices[i].name[0] != '\0';
            }
        }
        move_scan_device(index, target);
        index = target;
    }
    if (s_lab.list_kind == LIST_SCAN && (new_device || name_changed)) {
        const gm_ble_device_t *peer = &s_lab.devices[index];
        s_lab.host->log("[ble_lab] scan %s %02X:%02X:%02X:%02X:%02X:%02X t=%u rssi=%d name=%s",
                        new_device ? "new" : "name", (unsigned)peer->address[5],
                        (unsigned)peer->address[4], (unsigned)peer->address[3],
                        (unsigned)peer->address[2], (unsigned)peer->address[1],
                        (unsigned)peer->address[0], (unsigned)peer->address_type,
                        (int)peer->rssi, peer->name[0] ? peer->name : "(unnamed)");
    }
    s_lab.dirty = true;
}
static bool show_last_peer(void)
{
    if (!s_lab.peer_known) {
        return false;
    }
    if (device_list_ready()) {
        for (unsigned i = 0; i < s_lab.device_count; i++) {
            if (s_lab.devices[i].address_type == s_lab.peer.address_type &&
                !s_lab.libc->memcmp(s_lab.devices[i].address, s_lab.peer.address, 6)) {
                return false;
            }
        }
    }
    return true;
}
static unsigned device_rows(void)
{
    return s_lab.device_count + (show_last_peer() ? 1U : 0U);
}
static const gm_ble_device_t *device_row(unsigned row)
{
    if (row < s_lab.device_count) {
        return &s_lab.devices[row];
    }
    return row == s_lab.device_count && show_last_peer() ? &s_lab.peer : NULL;
}
static bool same_peer(const gm_ble_device_t *device, const gm_ble_device_t *target)
{
    /* Type 0/2 and type 1/3 are base/identity aliases for the same bytes. */
    return device->address_type <= 3 && target->address_type <= 3 &&
        (device->address_type & 1U) == (target->address_type & 1U) &&
        !s_lab.libc->memcmp(device->address, target->address, sizeof(device->address));
}
static void forget_peer_history(const gm_ble_device_t *target)
{
    static const uint8_t zero[6] = {0};
    if (target->address_type > 3 || !s_lab.libc->memcmp(target->address, zero, sizeof(zero))) {
        return;
    }
    bool changed = false;
    for (unsigned i = 0; i < s_lab.device_count;) {
        if (!same_peer(&s_lab.devices[i], target)) {
            i++;
            continue;
        }
        unsigned row = i + (scan_list_visible() && i >= SCAN_MODE_ROW);
        if (s_lab.page == PAGE_DEVICES && s_lab.selection > row &&
            !(scan_list_visible() && s_lab.selection == SCAN_MODE_ROW)) {
            if (scan_list_visible() && s_lab.selection == SCAN_MODE_ROW + 1U) {
                s_lab.selection--; /* Skip the mode row when the next peer shifts to row 0. */
            }
            s_lab.selection--;
        }
        if (s_lab.selected_device == (int16_t)i) {
            s_lab.selected_device = -1;
        } else if (s_lab.selected_device > (int16_t)i) {
            s_lab.selected_device--;
        }
        if (s_lab.device_replace > i) {
            s_lab.device_replace--;
        }
        s_lab.device_count--;
        if (i < s_lab.device_count) {
            s_lab.libc->memmove(&s_lab.devices[i], &s_lab.devices[i + 1U],
                                (s_lab.device_count - i) * sizeof(s_lab.devices[0]));
        }
        s_lab.libc->memset(&s_lab.devices[s_lab.device_count], 0, sizeof(s_lab.devices[0]));
        if (s_lab.device_replace >= s_lab.device_count) {
            s_lab.device_replace = 0;
        }
        changed = true;
    }
    for (unsigned i = 0; i < s_lab.bond_count;) {
        if (!same_peer(&s_lab.bonds[i], target)) {
            i++;
            continue;
        }
        if ((s_lab.page == PAGE_BONDS || s_lab.page == PAGE_SAVED_DEVICES) && s_lab.selection > i) {
            s_lab.selection--;
        }
        if (s_lab.selected_bond == (int16_t)i) {
            s_lab.selected_bond = -1;
        } else if (s_lab.selected_bond > (int16_t)i) {
            s_lab.selected_bond--;
        }
        s_lab.bond_count--;
        if (i < s_lab.bond_count) {
            s_lab.libc->memmove(&s_lab.bonds[i], &s_lab.bonds[i + 1U],
                                (s_lab.bond_count - i) * sizeof(s_lab.bonds[0]));
            s_lab.libc->memmove(&s_lab.bond_flags[i], &s_lab.bond_flags[i + 1U],
                                (s_lab.bond_count - i) * sizeof(s_lab.bond_flags[0]));
        }
        s_lab.libc->memset(&s_lab.bonds[s_lab.bond_count], 0, sizeof(s_lab.bonds[0]));
        s_lab.bond_flags[s_lab.bond_count] = 0;
        changed = true;
    }
    if (s_lab.pending_op != GM_BLE_OP_UNBOND && s_lab.confirm_action == MENU_UNBOND &&
        same_peer(&s_lab.confirm_peer, target)) {
        if (s_lab.unpair_stage == UNPAIR_VERIFY) {
            s_lab.pending_op = 0;
        }
        s_lab.unpair_stage = UNPAIR_NONE;
        if (s_lab.page == PAGE_CONFIRM && s_lab.confirm_enabled) {
            s_lab.confirm_enabled = false;
            enter_page(PAGE_MENU);
        }
        s_lab.libc->memset(&s_lab.confirm_peer, 0, sizeof(s_lab.confirm_peer));
        changed = true;
    }
    if (s_lab.peer_known && same_peer(&s_lab.peer, target)) {
        s_lab.peer_known = false;
        s_lab.libc->memset(&s_lab.peer, 0, sizeof(s_lab.peer));
        if (s_lab.selected_device == SELECT_LAST_PEER) {
            s_lab.selected_device = -1;
        }
        s_lab.connected = false;
        s_lab.session = 0;
        s_lab.unpair_stage = UNPAIR_NONE;
        s_lab.mtu = 0;
        if (s_lab.pending_op != GM_BLE_OP_CONNECT) {
            s_lab.auto_stage = AUTO_NONE;
            if (s_lab.pending_op != GM_BLE_OP_UNBOND &&
                s_lab.pending_op != GM_BLE_OP_GET_BONDED_DEVICES &&
                s_lab.pending_op != LOCAL_CONNECTED_LIST) {
                s_lab.pending_op = 0;
            }
        }
        s_lab.confirm_session = 0;
        if (s_lab.confirm_action != MENU_UNBOND) {
            s_lab.confirm_enabled = false;
        }
        invalidate_catalog();
        reset_results();
        /* Keep a blank editable byte; never reuse the forgotten peer's draft. */
        s_lab.tx_length = 1;
        s_lab.cursor = s_lab.confirm_length = 0;
        s_lab.libc->memset(s_lab.value, 0, sizeof(s_lab.value));
        s_lab.libc->memset(s_lab.tx, 0, sizeof(s_lab.tx));
        s_lab.libc->memset(s_lab.confirm_tx, 0, sizeof(s_lab.confirm_tx));
        changed = true;
    }
    if (changed) {
        unsigned rows = page_count();
        if (s_lab.selection >= rows) {
            s_lab.selection = rows ? (uint16_t)(rows - 1U) : 0;
        }
        page_hint();
    }
}
static bool page_shows_notifications(void)
{
    return s_lab.page == PAGE_VALUE || s_lab.page == PAGE_CHARACTERISTIC;
}

static bool copy_value(const gm_ble_event_t *event)
{
    if (event->data_length > GM_BLE_VALUE_MAX || (event->data_length && event->data == NULL)) {
        quarantine("Invalid value payload; client closed. Select Retry client.");
        return false;
    }
    if (event->data_length != 0) {
        s_lab.libc->memcpy(s_lab.value, event->data, event->data_length);
    }
    s_lab.value_length = event->data_length;
    s_lab.value_handle = event->handle;
    s_lab.value_type = (uint8_t)event->type;
    s_lab.value_session = event->session_id;
    s_lab.value_offset = 0;
    /* Cache every received value, but let the loop pace notification-only
     * redraws. Reads and other UI updates keep their immediate dirty flag. */
    if (event->type == GM_BLE_EVENT_NOTIFICATION) {
        if (page_shows_notifications()) {
            s_lab.notification_redraw_pending = true;
        }
    } else {
        s_lab.dirty = true;
    }
    return true;
}
static bool protected_uuid(const gm_ble_uuid_t *uuid)
{
    static const uint8_t base[] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
    unsigned offset = 0;
    if (uuid->length == 16 && !s_lab.libc->memcmp(uuid->bytes, base, sizeof(base)) &&
        uuid->bytes[14] == 0 && uuid->bytes[15] == 0) {
        offset = 12;
    } else if (uuid->length != 2) {
        return false;
    }
    unsigned value = uuid->bytes[offset] | ((unsigned)uuid->bytes[offset + 1U] << 8);
    return value == 0x1801 || value == 0x180f || value == 0x1812;
}
static const attribute_t *owning_service(const attribute_t *record)
{
    for (unsigned i = 0; i < s_lab.catalog_count; i++) {
        const attribute_t *service = &s_lab.catalog[i];
        if (service->type == GM_BLE_EVENT_SERVICE && record->handle >= service->handle &&
            record->handle <= service->end) {
            return service;
        }
    }
    return NULL;
}
static bool protected_characteristic(const attribute_t *record)
{
    const attribute_t *service = owning_service(record);
    return !service || protected_uuid(&service->uuid);
}
static bool characteristic_in_service(const attribute_t *record, uint16_t start, uint16_t end)
{
    return start != 0 && record->type == GM_BLE_EVENT_CHARACTERISTIC &&
           record->handle > start && record->value > record->handle && record->value <= end;
}
static bool choose_auto_service(void)
{
    unsigned best = 0;
    for (unsigned i = 0; i < s_lab.catalog_count; i++) {
        const attribute_t *service = &s_lab.catalog[i];
        if (service->type != GM_BLE_EVENT_SERVICE) {
            continue;
        }
        for (unsigned j = 0; j < s_lab.catalog_count; j++) {
            const attribute_t *record = &s_lab.catalog[j];
            if (!characteristic_in_service(record, service->handle, service->end)) {
                continue;
            }
            unsigned score = (record->properties & 2U) ? 1U : 0U;
            if (!protected_uuid(&service->uuid)) {
                score = (record->properties & 0x30U) ? 3U : score * 2U;
            }
            if (score > best) {
                best = score;
                /* Scope checks to this instance, even when another has the same UUID. */
                s_lab.auto_service_start = service->handle;
                s_lab.auto_service_end = service->end;
            }
        }
    }
    return best != 0;
}
static bool auto_completed(const gm_ble_event_t *event)
{
    if (s_lab.auto_stage == AUTO_NONE) {
        return false;
    }
    if (event->result != GM_PLUGIN_OK) {
        s_lab.auto_errors++;
        if ((s_lab.auto_stage != AUTO_READ && s_lab.auto_stage != AUTO_SUBSCRIBE &&
             s_lab.auto_stage != AUTO_UNSUBSCRIBE) ||
            (event->result == GM_PLUGIN_EIO && !event->att_error) ||
            event->result == GM_PLUGIN_ESTATE) {
            auto_stop("Auto stopped: operation failed. See log; Disconnect or use Advanced.");
            return true;
        }
    }
    switch (s_lab.auto_stage) {
    case AUTO_PAIR:
        s_lab.auto_stage = AUTO_SECURITY;
        s_lab.security_polls = 0;
        break;
    case AUTO_DISCOVER:
        if (!s_lab.catalog_all || !choose_auto_service()) {
            auto_stop("Auto stopped: no complete catalog/eligible service. See log.");
            return true;
        }
        s_lab.auto_index = 0;
        s_lab.auto_stage = AUTO_READ;
        break;
    case AUTO_READ:
        s_lab.auto_reads += event->result == GM_PLUGIN_OK;
        s_lab.auto_index++;
        break;
    case AUTO_SUBSCRIBE:
        if (event->result == GM_PLUGIN_OK && event->subscribed) {
            s_lab.auto_subscriptions++;
            if (!s_lab.auto_unsubscribe_tested) {
                s_lab.auto_stage = AUTO_UNSUBSCRIBE;
                break;
            }
        } else if (event->result == GM_PLUGIN_OK) {
            s_lab.auto_errors++;
        }
        s_lab.auto_index++;
        break;
    case AUTO_UNSUBSCRIBE:
        s_lab.auto_unsubscribe_tested = true;
        s_lab.auto_stage = AUTO_SUBSCRIBE;
        if (event->result == GM_PLUGIN_OK && !event->subscribed) {
            s_lab.auto_subscriptions--;
        } else {
            s_lab.auto_index++;
            s_lab.auto_errors += event->result == GM_PLUGIN_OK;
        }
        break;
    case AUTO_BONDS:
        s_lab.auto_errors += !s_lab.bonds_valid;
        s_lab.auto_stage = AUTO_NONE;
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             "Auto done: reads=%u subscriptions=%u errors=%u\n"
                             "Monitoring values; writes stay manual.",
                             (unsigned)s_lab.auto_reads, (unsigned)s_lab.auto_subscriptions,
                             (unsigned)s_lab.auto_errors);
        s_lab.host->log("[ble_lab] %s", s_lab.message);
        s_lab.dirty = true;
        return true;
    default:
        auto_stop("Auto stopped: unexpected completion.");
        return true;
    }
    s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                         "Auto: %s\nReads=%u subscriptions=%u errors=%u", auto_name(),
                         (unsigned)s_lab.auto_reads, (unsigned)s_lab.auto_subscriptions,
                         (unsigned)s_lab.auto_errors);
    s_lab.dirty = true;
    return true;
}
static void auto_step(void)
{
    if (!s_lab.auto_stage || !s_lab.connected || s_lab.pending_op || s_lab.scanning ||
        s_lab.suspended || s_call_ui.active) {
        return;
    }
    uint32_t id;
    gm_plugin_result_t result;
    switch (s_lab.auto_stage) {
    case AUTO_PAIR:
        id = begin(GM_BLE_OP_PAIR);
        result = s_lab.ble->pair(s_lab.client, s_lab.session, id);
        break;
    case AUTO_SECURITY:
    case AUTO_STATE:
        if (s_lab.auto_stage == AUTO_SECURITY) {
            uint32_t now = s_lab.host->monotonic_ms();
            if (s_lab.security_polls &&
                (uint32_t)(now - s_lab.security_polled_at) < SECURITY_POLL_MS) {
                return;
            }
            s_lab.security_polled_at = now;
            s_lab.security_polls++;
        }
        id = begin(GM_BLE_OP_GET_SESSION_STATE);
        result = s_lab.ble->get_session_state(s_lab.client, s_lab.session, id);
        break;
    case AUTO_DISCOVER:
        invalidate_catalog();
        id = begin(GM_BLE_OP_DISCOVER_ALL);
        result = s_lab.ble->discover_all(s_lab.client, s_lab.session, id);
        break;
    case AUTO_READ:
    case AUTO_SUBSCRIBE:
        while (s_lab.auto_index < s_lab.catalog_count) {
            const attribute_t *record = &s_lab.catalog[s_lab.auto_index];
            bool eligible =
                characteristic_in_service(record, s_lab.auto_service_start, s_lab.auto_service_end);
            if (s_lab.auto_stage == AUTO_READ) {
                eligible = eligible && (record->properties & 2U);
            } else {
                eligible =
                    eligible && (record->properties & 0x30U) && !protected_characteristic(record);
            }
            if (eligible) {
                break;
            }
            s_lab.auto_index++;
        }
        if (s_lab.auto_index == s_lab.catalog_count ||
            (s_lab.auto_stage == AUTO_SUBSCRIBE &&
             s_lab.auto_subscriptions >= s_lab.caps.max_subscriptions)) {
            s_lab.auto_index = 0;
            s_lab.auto_stage = s_lab.auto_stage == AUTO_READ ? AUTO_SUBSCRIBE : AUTO_STATE;
            return;
        }
        s_lab.selected_attribute = s_lab.auto_index;
        if (s_lab.auto_stage == AUTO_READ) {
            id = begin(GM_BLE_OP_READ);
            result = s_lab.ble->read(s_lab.client, s_lab.session,
                                     s_lab.catalog[s_lab.auto_index].value, id);
        } else {
            id = begin(GM_BLE_OP_SUBSCRIBE);
            const attribute_t *record = &s_lab.catalog[s_lab.auto_index];
            result = s_lab.ble->subscribe(s_lab.client, s_lab.session, record->value, true,
                                          !(record->properties & 0x10U), id);
        }
        break;
    case AUTO_UNSUBSCRIBE:
        id = begin(GM_BLE_OP_UNSUBSCRIBE);
        /* Browsing another characteristic must not change this request's target. */
        s_lab.pending_handle = s_lab.catalog[s_lab.auto_index].value;
        result = s_lab.ble->subscribe(s_lab.client, s_lab.session,
                                      s_lab.pending_handle, false, false, id);
        break;
    case AUTO_BONDS:
        reset_bonds();
        id = begin(GM_BLE_OP_GET_BONDED_DEVICES);
        result = get_bond_list(id);
        break;
    default:
        return;
    }
    s_lab.host->log("[ble_lab] AUTO submit stage=%s op=%u id=%lu rc=%ld", auto_name(),
                    (unsigned)s_lab.pending_op, (unsigned long)s_lab.pending_id, (long)result);
    submitted(result);
}
static void auto_pump(void)
{
    bool was_active = s_lab.auto_stage != AUTO_NONE;
    auto_step();
    if (was_active && s_lab.page != PAGE_MENU) {
        page_hint();
    }
}
static void completed(const gm_ble_event_t *event)
{
    uint8_t operation = s_lab.pending_op;
    attribute_t *target = characteristic_by_handle(s_lab.pending_handle);
    if (target) {
        target->last_operation = operation;
        target->last_result = event->result;
        target->att_error = event->att_error;
        target->result_state = 2;
        if (operation == GM_BLE_OP_SUBSCRIBE || operation == GM_BLE_OP_UNSUBSCRIBE) {
            target->subscribed = event->subscribed;
            target->subscription_known = 1;
        }
    }
    s_lab.pending_op = 0;
    if (operation == GM_BLE_OP_GET_SESSION_STATE && s_lab.unpair_stage == UNPAIR_VERIFY) {
        /* A RESULT is not the verified SESSION_STATE needed for this chain. */
        cancel_unpair(unpair_unverified);
        return;
    }
    if (operation == GM_BLE_OP_DISCONNECT && s_lab.unpair_stage == UNPAIR_WAIT_DOWN) {
        if (event->result != GM_PLUGIN_OK) {
            cancel_unpair("Disconnect failed. No bond removed.");
        } else {
            s_lab.pending_op = LOCAL_DISCONNECT_WAIT;
            say("Waiting for the selected connection to detach before unpairing.");
        }
        return;
    }
    if (operation == GM_BLE_OP_CONNECT && event->result != GM_PLUGIN_OK &&
        s_lab.scanning && !s_lab.scan_stopping) {
        /* Admission failure can arrive before the Host reaches its scan stop. */
        submit_scan_stop();
    }
    if (operation == GM_BLE_OP_CONNECT && event->result == GM_PLUGIN_EBUSY) {
        /* A rejected connect creates no new link; keep any existing session. */
        if (!s_lab.connected) {
            /* Older firmware may assign a session before rejecting a request. */
            s_lab.session = event->session_id;
            invalidate_catalog();
        }
        s_lab.auto_stage = AUTO_NONE;
        if (event->properties == GM_BLE_CONNECT_PLUGIN_LIMIT) {
            say(plugin_limit);
        } else if (event->properties == GM_BLE_CONNECT_SLOT_UNAVAILABLE) {
            say("No free BLE accessory slot. Disconnect an accessory and retry.");
        } else if (event->properties == GM_BLE_CONNECT_BOND_LIMIT) {
            say("Pairing records full. Unpair an accessory first.");
        } else {
            say("Radio busy; retry after the current setup/cancel. App-first is not required.");
        }
        return;
    }
    if (operation == GM_BLE_OP_CONNECT) {
        /* Failed pre-link requests can still have a newly assigned Host session. */
        s_lab.session = event->session_id;
        s_lab.connected = false;
        invalidate_catalog();
    } else if (operation == GM_BLE_OP_DISCOVER || operation == GM_BLE_OP_DISCOVER_ALL) {
        bool full = operation == GM_BLE_OP_DISCOVER_ALL;
        bool host_enomem = full && event->result == GM_PLUGIN_ENOMEM;
        bool too_large = s_lab.catalog_error == CATALOG_TOO_LARGE
            || (host_enomem && event->properties == GM_BLE_DISCOVERY_CATALOG_LIMIT);
        bool local_oom = s_lab.catalog_error == CATALOG_NO_MEMORY;
        s_lab.catalog_valid = event->result == GM_PLUGIN_OK && s_lab.catalog_count != 0
            && s_lab.catalog_error == CATALOG_OK;
        s_lab.catalog_all = s_lab.catalog_valid && full;
        s_lab.host->log("[ble_lab] CATALOG op=%u rc=%ld records=%u valid=%u",
                        (unsigned)operation, (long)event->result,
                        (unsigned)s_lab.catalog_count, (unsigned)s_lab.catalog_valid);
        if (!s_lab.catalog_valid) {
            invalidate_catalog();
        }
        if (too_large || local_oom || host_enomem) {
            const char *message = too_large
                ? "GATT catalog exceeds supported capacity. Connection kept. Use Find common services."
                : local_oom
                    ? "Not enough memory for GATT catalog. Connection kept. Retry or use Find common services."
                    : "GATT catalog unavailable: memory or firmware limit. Connection kept. Use Find common services.";
            if (s_lab.auto_stage == AUTO_DISCOVER) {
                s_lab.auto_errors++;
                auto_stop(message);
            } else {
                say(message);
            }
            return;
        }
    } else if (operation == GM_BLE_OP_GET_BONDED_DEVICES) {
        s_lab.bonds_valid = event->result == GM_PLUGIN_OK && !s_lab.bonds_overflow;
        if (!s_lab.bonds_valid) {
            s_lab.bond_count = 0;
        }
    } else if (event->result == GM_PLUGIN_OK) {
        if (operation == GM_BLE_OP_READ) {
            if (!copy_value(event)) {
                return;
            }
            if (target && !event->att_error) {
                target->read_length = event->data_length;
            }
            if (s_lab.auto_stage == AUTO_NONE && !s_lab.pending_gatt &&
                s_lab.pending_view == s_lab.view_id) {
                enter_page(PAGE_VALUE);
            }
        } else if (operation == GM_BLE_OP_DISCONNECT) {
            s_lab.pending_op = LOCAL_DISCONNECT_WAIT;
            say("Local detach accepted. WAIT for disconnected CONNECTION event.");
            return;
        } else if (operation == GM_BLE_OP_UNBOND) {
            /* Clear the active on-air alias only after the exact stable identity
             * was verified in this same session and successfully unbonded. */
            bool forget_alias = s_lab.unpair_stage == UNPAIR_REMOVE && s_lab.peer_known;
            gm_ble_device_t identity, alias;
            s_lab.libc->memcpy(&identity, &s_lab.confirm_peer, sizeof(identity));
            if (forget_alias) {
                s_lab.libc->memcpy(&alias, &s_lab.peer, sizeof(alias));
            }
            s_lab.unpair_stage = UNPAIR_NONE;
            forget_peer_history(&identity);
            if (forget_alias) {
                forget_peer_history(&alias);
            }
            s_lab.bonds_valid = false;
            s_lab.selected_bond = -1;
        }
    }
    if (operation == GM_BLE_OP_UNBOND) {
        s_lab.unpair_stage = UNPAIR_NONE;
    }
    if (operation == GM_BLE_OP_UNBOND && event->result == GM_PLUGIN_EPERM) {
        s_lab.bonds_valid = false;
        s_lab.selected_bond = -1;
        say("Cannot unpair here. For App-bound HOGP, unpair in App > Third-party Devices.");
        return;
    }
    if (auto_completed(event)) {
        return;
    }
    if (operation == GM_BLE_OP_DISCOVER) {
        say(s_lab.catalog_valid ? "Service found. Select a characteristic."
            : "Service discovery failed. It may not be available on this device. See log.");
        return;
    }
    if (operation == GM_BLE_OP_GET_BONDED_DEVICES && s_lab.bonds_valid &&
        s_lab.page == PAGE_SAVED_DEVICES && s_lab.pending_view == s_lab.view_id) {
        page_hint();
        return;
    }
    if (operation == GM_BLE_OP_GET_BONDED_DEVICES && s_lab.bonds_valid && !s_lab.bond_count) {
        say(no_plugin_bonds);
        return;
    }
    if (target) {
        /* Navigating while waiting cannot put A's completion under B's label. */
        if (!s_lab.gatt_view || selected_characteristic() == target) {
            format_operation(target, s_lab.message, sizeof(s_lab.message));
        }
        s_lab.dirty = true;
        return;
    }
    s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                         "RESULT op=%u id=%lu h=%04X rc=%ld ATT=%02X subscribed=%u%s",
                         (unsigned)operation, (unsigned long)event->request_id,
                         (unsigned)event->handle, (long)event->result, (unsigned)event->att_error,
                         (unsigned)event->subscribed,
                         (operation == GM_BLE_OP_GET_BONDED_DEVICES ? s_lab.bonds_overflow : s_lab.overflow)
                             ? " LOCAL LIST OVERFLOW; NOT VALID"
                         : operation == GM_BLE_OP_WRITE_NO_RESPONSE ? " LOCAL SUBMISSION ONLY"
                                                                    : "");
    s_lab.dirty = true;
}
static void on_ble(void *context, const gm_ble_event_t *event)
{
    uint32_t view = s_lab.view_id;
    bool relevant = true;
    if (event && event->struct_size >= sizeof(*event)) {
        if (event->type == GM_BLE_EVENT_SCAN_ENDED && s_lab.scanning) {
            /* A Connect transition suppresses the scan status, not its lifecycle. */
            relevant = !s_lab.scan_view || s_lab.scan_view == view ||
                (s_lab.scan_id && s_lab.scan_id != event->scan_id);
        } else if (s_lab.pending_op && event->request_id == s_lab.pending_id &&
                   (event->type == GM_BLE_EVENT_RESULT ||
                    event->type == GM_BLE_EVENT_SESSION_STATE ||
                    event->type == GM_BLE_EVENT_CONNECTED_DEVICES_DONE ||
                    (event->type == GM_BLE_EVENT_CONNECTION &&
                     s_lab.pending_op == GM_BLE_OP_CONNECT))) {
            relevant = s_lab.auto_stage != AUTO_NONE ? s_lab.page == PAGE_MENU
                                                    : s_lab.pending_view == view;
        }
    }
    handle_ble(context, event);
    /* Apply every valid result to data, but do not leave another view's status
     * on the current page. A real disconnect/quarantine enters its own page. */
    if (!relevant && s_lab.view_id == view) {
        page_hint();
    }
}
static void handle_ble(void *context, const gm_ble_event_t *event)
{
    (void)context;
    if (!s_lab.started || s_lab.client == 0 || event == NULL ||
        event->struct_size < sizeof(*event)) {
        return;
    }
    if (event->type == GM_BLE_EVENT_DATA_LOSS &&
        event->properties == GM_BLE_DATA_LOSS_NOTIFICATION_ONLY) {
        if (s_lab.connected && event->session_id == s_lab.session) {
            uint32_t remaining = UINT32_MAX - s_lab.notification_drops;
            s_lab.notification_drops += event->request_id < remaining
                ? event->request_id : remaining;
        }
        return;
    }
    if (event->type != GM_BLE_EVENT_SCAN_RESULT && event->type != GM_BLE_EVENT_SCAN_ENDED
        && event->type != GM_BLE_EVENT_SERVICE && event->type != GM_BLE_EVENT_CHARACTERISTIC
        && event->type != GM_BLE_EVENT_DESCRIPTOR && event->type != GM_BLE_EVENT_ATTRIBUTE
        && event->type != GM_BLE_EVENT_NOTIFICATION) {
        s_lab.host->log("[ble_lab] event=%u op=%u req=%lu session=%lu rc=%ld ATT=%u h=%04X len=%u",
                        (unsigned)event->type, (unsigned)event->operation,
                        (unsigned long)event->request_id, (unsigned long)event->session_id,
                        (long)event->result, (unsigned)event->att_error, (unsigned)event->handle,
                        (unsigned)event->data_length);
    }
    if (event->type == GM_BLE_EVENT_DATA_LOSS) {
        quarantine("DATA LOSS: client closed, partial lists discarded. Select Retry client.");
        return;
    }
    if (event->type == GM_BLE_EVENT_RESULT && event->operation == GM_BLE_OP_UNBOND &&
        event->request_id == 0U) {
        if (event->result == GM_PLUGIN_OK) {
            forget_peer_history(&event->device);
        }
        return;
    }
    if (event->type == GM_BLE_EVENT_SCAN_RESULT && s_lab.scanning) {
        if (s_lab.scan_id == 0) {
            s_lab.scan_id = event->scan_id;
        }
        if (s_lab.scan_id == event->scan_id && s_lab.scan_view) {
            s_lab.scan_reports++;
            collect_device(&event->device);
        }
        return;
    }
    if (event->type == GM_BLE_EVENT_SCAN_ENDED && s_lab.scanning &&
        (s_lab.scan_id == 0 || s_lab.scan_id == event->scan_id)) {
        bool load_bonds = s_lab.bonds_after_scan &&
            (s_lab.page == PAGE_BONDS || s_lab.page == PAGE_SAVED_DEVICES);
        s_lab.scanning = false;
        s_lab.scan_stopping = s_lab.bonds_after_scan = false;
        s_lab.list_valid = true; /* A coherent best-effort scan is not a GATT catalog. */
        s_lab.host->log("[ble_lab] scan end rc=%ld reports=%lu peers=%u partial=%u",
                        (long)event->result, (unsigned long)s_lab.scan_reports,
                        (unsigned)s_lab.device_count,
                        (unsigned)(event->result == GM_PLUGIN_ENOMEM || s_lab.overflow));
        s_lab.dirty = true;
        if (!s_lab.scan_view) {
            return; /* Keep the current Connect status while retiring the scan. */
        }
        if (event->result == GM_PLUGIN_ENOMEM || s_lab.overflow) {
            s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                                 "Scan ended; peers=%u (partial delivery).\n"
                                 "Client ready; later peers were not blocked.",
                                 (unsigned)s_lab.device_count);
        } else if (event->result == GM_PLUGIN_EBUSY) {
            say("Scan busy/preempted by HOGP. Client stays open; retry when radio is free.");
        } else {
            s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                                 "Scan ended rc=%ld; peers=%u%s", (long)event->result,
                                 (unsigned)s_lab.device_count, s_lab.overflow ? " OVERFLOW" : "");
        }
        if (load_bonds) {
            run_action(s_lab.page == PAGE_SAVED_DEVICES ? MENU_SAVED_DEVICES : MENU_BONDS);
        }
        return;
    }
    if (s_lab.pending_op == LOCAL_CONNECTED_LIST && event->type != GM_BLE_EVENT_CONNECTION &&
        event->type != GM_BLE_EVENT_NOTIFICATION) {
        if (event->type == GM_BLE_EVENT_CONNECTED_DEVICE) {
            collect_device(&event->device);
        } else if (event->type == GM_BLE_EVENT_CONNECTED_DEVICES_DONE) {
            s_lab.list_valid = !s_lab.overflow;
            s_lab.pending_op = 0;
            say(s_lab.overflow ? "Connected list overflow; invalid."
                               : "Connected peer list complete.");
        }
        return;
    }
    if (event->type == GM_BLE_EVENT_CONNECTION) {
        if (s_lab.pending_op == GM_BLE_OP_CONNECT && event->request_id == s_lab.pending_id) {
            s_lab.pending_op = 0;
            s_lab.session = event->session_id;
            if (event->result == GM_PLUGIN_OK && event->device.connected && event->session_id) {
                s_lab.session = event->session_id;
                s_lab.connected = true;
                s_lab.libc->memcpy(&s_lab.peer, &event->device, sizeof(s_lab.peer));
                s_lab.peer_known = true;
                s_lab.mtu = event->mtu;
                invalidate_catalog();
                if (s_lab.auto_stage != AUTO_NONE) {
                    /* Pair now, not after the user traverses the menu. */
                    auto_pump();
                } else {
                    say(event->device.hid_ready
                            ? "Attached to system HOGP. Open GATT services to browse."
                            : "GATT connected. No auto checks. Open GATT services to browse.");
                }
            } else {
                s_lab.auto_stage = AUTO_NONE;
                s_lab.connected = false;
                s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                                     "Connect FAILED rc=%ld reason=%02X.", (long)event->result,
                                     (unsigned)event->att_error);
                s_lab.dirty = true;
            }
        } else if (event->session_id == s_lab.session && !event->device.connected) {
            if (s_lab.unpair_stage != UNPAIR_NONE && (event->request_id || event->operation)) {
                return; /* A CONNECT completion is not a physical link-down event. */
            }
            bool remove_bond = s_lab.unpair_stage == UNPAIR_WAIT_DOWN &&
                event->session_id == s_lab.confirm_session &&
                (event->result == GM_PLUGIN_OK || event->result == GM_PLUGIN_EIO);
            s_lab.connected = false;
            s_lab.auto_stage = AUTO_NONE;
            s_lab.confirm_enabled = false;
            invalidate_catalog();
            if (s_lab.pending_op == GM_BLE_OP_UNBOND) {
                return; /* A later/duplicate disconnect must not mask the unbond result. */
            }
            s_lab.pending_op = 0;
            s_lab.unpair_stage = UNPAIR_NONE;
            enter_page(PAGE_MENU);
            if (remove_bond) {
                s_lab.unpair_stage = UNPAIR_REMOVE;
                submit_unbond(); /* Link-down, not the disconnect ACK, authorizes deletion. */
            } else {
                say("DISCONNECTED / detached. Session retained only for state query.");
            }
        }
        return;
    }
    if (event->type == GM_BLE_EVENT_NOTIFICATION && event->session_id == s_lab.session &&
        s_lab.connected) {
        s_lab.notifications++;
        if (copy_value(event)) {
            attribute_t *record = characteristic_by_handle(event->handle);
            if (record) {
                record->notifications++;
            }
        }
        return;
    }
    if (event->request_id != s_lab.pending_id || !s_lab.pending_op) {
        return;
    }
    if (s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES &&
        event->type == GM_BLE_EVENT_BONDED_DEVICE) {
        collect_bond(event);
        return;
    }
    if ((s_lab.pending_op == GM_BLE_OP_DISCOVER || s_lab.pending_op == GM_BLE_OP_DISCOVER_ALL) &&
        event->session_id == s_lab.session &&
        (event->type == GM_BLE_EVENT_SERVICE || event->type == GM_BLE_EVENT_CHARACTERISTIC ||
         event->type == GM_BLE_EVENT_DESCRIPTOR || event->type == GM_BLE_EVENT_ATTRIBUTE)) {
        /* Targeted-discovery metadata may have operation=0; correlate by its
         * request/session. Terminal RESULT always supplies the operation. */
        if (s_lab.catalog_error != CATALOG_OK) {
            return;
        }
        if (s_lab.catalog_count == CATALOG_LIMIT) {
            invalidate_catalog();
            s_lab.catalog_error = CATALOG_TOO_LARGE;
            return;
        }
        if (s_lab.catalog == NULL) {
            /* One bounded allocation per discovery, only once metadata arrives.
             * A rejected request or an empty result needs no catalog storage. */
            s_lab.catalog = s_lab.host->alloc(CATALOG_LIMIT * sizeof(*s_lab.catalog));
            if (s_lab.catalog == NULL) {
                s_lab.catalog_error = CATALOG_NO_MEMORY;
                return;
            }
        }
        attribute_t *record = &s_lab.catalog[s_lab.catalog_count++];
        s_lab.libc->memset(record, 0, sizeof(*record));
        record->type = event->type;
        record->handle = event->handle;
        record->value = event->value_handle;
        record->end = event->end_handle;
        record->properties = event->properties;
        s_lab.libc->memcpy(&record->uuid, &event->uuid, sizeof(record->uuid));
        s_lab.dirty = true;
        return;
    }
    if (s_lab.pending_op == GM_BLE_OP_GET_SESSION_STATE &&
        event->type == GM_BLE_EVENT_SESSION_STATE && event->session_id == s_lab.session) {
        if (s_lab.unpair_stage == UNPAIR_VERIFY && event->operation != GM_BLE_OP_GET_SESSION_STATE) {
            return;
        }
        gm_ble_session_state_t snapshot;
        if (event->data == NULL || event->data_length != sizeof(snapshot)) {
            if (s_lab.unpair_stage == UNPAIR_VERIFY) {
                cancel_unpair(unpair_unverified);
            } else {
                quarantine("Invalid session snapshot; client closed.");
            }
            return;
        }
        s_lab.libc->memcpy(&snapshot, event->data, sizeof(snapshot));
        if (snapshot.struct_size != sizeof(snapshot)) {
            if (s_lab.unpair_stage == UNPAIR_VERIFY) {
                cancel_unpair(unpair_unverified);
            } else {
                quarantine("Session snapshot ABI mismatch; client closed.");
            }
            return;
        }
        s_lab.pending_op = 0;
        if (s_lab.unpair_stage == UNPAIR_VERIFY) {
            if (event->result != GM_PLUGIN_OK || event->session_id != s_lab.confirm_session ||
                !s_lab.connected || snapshot.state != GM_BLE_SESSION_CONNECTED ||
                !snapshot.encrypted || !snapshot.bonded || !snapshot.managed ||
                snapshot.shared_hogp || snapshot.busy ||
                event->properties != GM_BLE_SESSION_STABLE_IDENTITY ||
                event->device.address_type != s_lab.confirm_peer.address_type ||
                !same_peer(&event->device, &s_lab.confirm_peer)) {
                cancel_unpair(unpair_unverified);
                return;
            }
            begin(GM_BLE_OP_DISCONNECT);
            if (s_lab.ble->disconnect(s_lab.client, s_lab.confirm_session) == GM_PLUGIN_OK) {
                s_lab.unpair_stage = UNPAIR_WAIT_DOWN;
                say("Disconnecting the verified peer before unpairing.");
            } else {
                cancel_unpair("Disconnect submission failed. No bond removed.");
            }
            return;
        }
        if (snapshot.state == GM_BLE_SESSION_DISCONNECTED) {
            s_lab.connected = false;
            invalidate_catalog();
        }
        if (s_lab.auto_stage != AUTO_NONE) {
            if (snapshot.state != GM_BLE_SESSION_CONNECTED) {
                auto_stop("Auto stopped: session is no longer connected.");
            } else if (s_lab.auto_stage == AUTO_SECURITY) {
                if (snapshot.encrypted) {
                    s_lab.auto_stage = AUTO_DISCOVER;
                    say("Encryption confirmed. Automatically discovering GATT.");
                } else if (s_lab.security_polls >= SECURITY_POLLS) {
                    s_lab.auto_errors++;
                    auto_stop("Auto stopped: encryption not confirmed. No GATT actions sent.");
                } else {
                    say("Pair result received; waiting for real encryption confirmation.");
                }
            } else if (s_lab.auto_stage == AUTO_STATE) {
                s_lab.auto_stage = AUTO_BONDS;
                s_lab.auto_errors += !snapshot.encrypted;
                s_lab.host->log(
                    "[ble_lab] AUTO state encrypted=%u bonded=%u managed=%u shared=%u subs=%u",
                    (unsigned)snapshot.encrypted, (unsigned)snapshot.bonded,
                    (unsigned)snapshot.managed, (unsigned)snapshot.shared_hogp,
                    (unsigned)snapshot.subscriptions);
            } else {
                auto_stop("Auto stopped: unexpected state snapshot.");
            }
            return;
        }
        s_lab.libc->snprintf(s_lab.message, sizeof(s_lab.message),
                             "STATE=%u encrypted=%u bonded=%u managed=%u\n%s HOGP=%u busy=%u "
                             "subscriptions=%u MTU=%u",
                             (unsigned)snapshot.state, (unsigned)snapshot.encrypted,
                             (unsigned)snapshot.bonded, (unsigned)snapshot.managed,
                             snapshot.shared_hogp ? "shared" : "generic GATT; shared",
                             (unsigned)snapshot.shared_hogp, (unsigned)snapshot.busy,
                             (unsigned)snapshot.subscriptions, (unsigned)event->mtu);
        s_lab.dirty = true;
        return;
    }
    if (event->type == GM_BLE_EVENT_RESULT && event->operation == s_lab.pending_op &&
        (event->session_id == s_lab.session || s_lab.pending_op == GM_BLE_OP_GET_BONDED_DEVICES ||
         s_lab.pending_op == GM_BLE_OP_UNBOND || s_lab.pending_op == GM_BLE_OP_CONNECT)) {
        if (!s_lab.pending_handle || event->handle == s_lab.pending_handle) {
            completed(event);
        }
    }
}
static attribute_t *selected_characteristic(void)
{
    if (!s_lab.catalog_valid || s_lab.selected_attribute < 0 ||
        (unsigned)s_lab.selected_attribute >= s_lab.catalog_count) {
        return NULL;
    }
    attribute_t *record = &s_lab.catalog[s_lab.selected_attribute];
    return record->type == GM_BLE_EVENT_CHARACTERISTIC ? record : NULL;
}
static attribute_t *characteristic_by_handle(uint16_t handle)
{
    if (s_lab.catalog_valid && handle) {
        for (unsigned i = 0; i < s_lab.catalog_count; i++) {
            attribute_t *record = &s_lab.catalog[i];
            if (record->type == GM_BLE_EVENT_CHARACTERISTIC && record->value == handle) {
                return record;
            }
        }
    }
    return NULL;
}
static bool device_list_ready(void)
{
    /* Scan rows are independently usable; management lists remain transactional. */
    return s_lab.list_kind == LIST_SCAN || s_lab.list_valid;
}
static void reset_results(void)
{
    s_lab.auto_reads = s_lab.auto_subscriptions = s_lab.auto_errors = 0;
    s_lab.notifications = 0;
    s_lab.notification_drops = s_lab.notification_drops_logged = 0;
    s_lab.notification_log_at = 0;
    s_lab.notification_redraw_pending = false;
    s_lab.value_length = s_lab.value_offset = s_lab.value_handle = 0;
    s_lab.value_type = 0;
    s_lab.value_session = 0;
    s_lab.auto_unsubscribe_tested = false;
}
static void connect_selected(bool automatic)
{
    if (s_lab.pending_op || s_lab.auto_stage != AUTO_NONE) {
        say("Busy: wait for the current operation or Disconnect.");
        return;
    }
    const gm_ble_device_t *selected = NULL;
    if (s_lab.page == PAGE_SAVED_DEVICES) {
        if (s_lab.bonds_valid && s_lab.selected_bond >= 0 &&
            (unsigned)s_lab.selected_bond < s_lab.bond_count) {
            selected = &s_lab.bonds[s_lab.selected_bond];
        }
    } else if (s_lab.selected_device == SELECT_LAST_PEER && s_lab.peer_known) {
        selected = &s_lab.peer;
    } else if (device_list_ready() && s_lab.selected_device >= 0 &&
               (unsigned)s_lab.selected_device < s_lab.device_count) {
        selected = &s_lab.devices[s_lab.selected_device];
    }
    if (!selected) {
        say("Select a discovered peer first.");
        return;
    }
    if (s_lab.connected) {
        say(plugin_limit);
        return;
    }
    gm_ble_device_t peer;
    s_lab.libc->memcpy(&peer, selected, sizeof(peer));
    if (automatic) {
        reset_results();
        s_lab.auto_stage = AUTO_PAIR;
    }
    uint32_t id = begin(GM_BLE_OP_CONNECT);
    gm_plugin_result_t result = s_lab.ble->connect(s_lab.client, &peer, id);
    if (result == GM_PLUGIN_OK) {
        if (!automatic) {
            reset_results();
        }
        /* The Host stops scanning only after accepting the Connect internally.
         * Hide queued scan rows without pretending SCAN_ENDED has arrived. */
        if (s_lab.scanning) {
            s_lab.host->log("[ble_lab] scan select reports=%lu peers=%u partial=%u",
                            (unsigned long)s_lab.scan_reports, (unsigned)s_lab.device_count,
                            (unsigned)s_lab.overflow);
            s_lab.scan_view = 0;
            /* Start the bounded scan drain when Connect is submitted. */
            s_lab.scan_since = s_lab.pending_since;
        }
        s_lab.confirm_enabled = false;
        enter_page(PAGE_MENU);
        s_lab.pending_view = s_lab.view_id;
        menu_select(s_lab.advanced ? MENU_STATE : MENU_VALUE);
    }
    submitted(result);
}
static void confirm_action(uint8_t action, attribute_t *record)
{
    s_lab.confirm_action = action;
    s_lab.confirm_handle = record ? record->value : 0;
    s_lab.confirm_session = s_lab.session;
    if (action == MENU_UNBOND) {
        s_lab.libc->memcpy(&s_lab.confirm_peer, &s_lab.bonds[s_lab.selected_bond],
                           sizeof(s_lab.confirm_peer));
    }
    s_lab.confirm_length = 0;
    if (action == MENU_WRITE || action == MENU_WRITE_COMMAND) {
        s_lab.confirm_length = s_lab.tx_length;
        s_lab.libc->memcpy(s_lab.confirm_tx, s_lab.tx, s_lab.tx_length);
    }
    s_lab.confirm_enabled = true;
    enter_page(PAGE_CONFIRM);
    s_lab.selection = 0; /* Cancel is the default, never Confirm. */
    s_lab.dirty = true;
}
static void execute_confirmed(void)
{
    if (!s_lab.confirm_enabled || !s_lab.client || s_lab.pending_op || s_lab.scanning ||
        s_lab.session != s_lab.confirm_session) {
        s_lab.confirm_enabled = false;
        enter_page(s_lab.gatt_view ? PAGE_CHARACTERISTIC : PAGE_MENU);
        s_lab.selection = 0;
        say("Confirmation expired; select the operation again.");
        return;
    }
    uint8_t action = s_lab.confirm_action;
    s_lab.confirm_enabled = false;
    enter_page(s_lab.gatt_view ? PAGE_CHARACTERISTIC : PAGE_MENU);
    if (s_lab.gatt_view) {
        s_lab.selection = 0;
    } else {
        menu_select(action);
    }
    gm_plugin_result_t result;
    if (action == MENU_UNBOND) {
        bool target_live = s_lab.confirm_peer.connected ||
            (s_lab.connected && s_lab.peer_known && same_peer(&s_lab.peer, &s_lab.confirm_peer));
        if (!target_live) {
            submit_unbond();
        } else if (s_lab.connected && s_lab.session) {
            uint32_t id = begin(GM_BLE_OP_GET_SESSION_STATE);
            result = s_lab.ble->get_session_state(s_lab.client, s_lab.session, id);
            if (result == GM_PLUGIN_OK) {
                s_lab.unpair_stage = UNPAIR_VERIFY;
                say("Verifying the selected peer before disconnecting.");
            } else {
                cancel_unpair(unpair_unverified);
            }
        } else {
            say(unpair_unverified);
        }
        return;
    }
    if (!s_lab.connected) {
        say("No live connection; operation cancelled.");
        return;
    } else if (action == MENU_PAIR) {
        uint32_t id = begin(GM_BLE_OP_PAIR);
        result = s_lab.ble->pair(s_lab.client, s_lab.session, id);
    } else if (action == MENU_WRITE || action == MENU_WRITE_COMMAND) {
        uint8_t op = action == MENU_WRITE ? GM_BLE_OP_WRITE : GM_BLE_OP_WRITE_NO_RESPONSE;
        uint32_t id = begin(op);
        s_lab.pending_handle = s_lab.confirm_handle;
        result = s_lab.ble->write(s_lab.client, s_lab.session, s_lab.confirm_handle,
                                  s_lab.confirm_tx, s_lab.confirm_length, action == MENU_WRITE, id);
    } else {
        say("Invalid confirmation; no operation sent.");
        return;
    }
    submitted(result);
}
static void run_action(uint8_t action)
{
    s_lab.dirty = true;
    if (action == MENU_EXIT) {
        s_lab.host->app_exit();
        return;
    }
    if (action == MENU_ADVANCED) {
        s_lab.gatt_view = s_lab.edit_write = false;
        s_lab.advanced = !s_lab.advanced;
        s_lab.selection = 0;
        enter_page(PAGE_MENU);
        return;
    }
    if (action == MENU_GATT) {
        s_lab.gatt_view = s_lab.edit_write = false;
        s_lab.advanced = false;
        enter_page(PAGE_GATT);
        if (s_lab.connected && !s_lab.pending_op && !s_lab.scanning &&
            s_lab.auto_stage == AUTO_NONE && (!s_lab.catalog_valid || !s_lab.catalog_all)) {
            run_action(MENU_DISCOVER_ALL); /* Same-session Host cache, not a new discovery engine. */
        } else if (!s_lab.connected) {
            say("Connect a device first. More tools are available below.");
        } else if (s_lab.pending_op || s_lab.auto_stage != AUTO_NONE) {
            say("Wait for the current operation; reopen GATT services when done.");
        } else {
            say("Select a characteristic. R=Read W=Write N=Notify I=Indicate.");
        }
        return;
    }
    if (action == MENU_REOPEN) {
        close_client();
        open_client();
        return;
    }
    if (action == MENU_COMMON_SERVICES) {
        if (!s_lab.client || !s_lab.connected) {
            say("Connect / attach a peer first.");
        } else if (s_lab.pending_op || s_lab.scanning || s_lab.auto_stage != AUTO_NONE) {
            say("Wait for the current operation before finding a service.");
        } else {
            enter_page(PAGE_COMMON_SERVICES);
        }
        return;
    }
    if (action == MENU_DEVICES || action == MENU_CATALOG || action == MENU_VALUE ||
        action == MENU_HEX) {
        if (s_lab.auto_stage != AUTO_NONE && action == MENU_HEX) {
            say("Auto check running; editors are locked until it completes or is cancelled.");
            return;
        }
        enter_page(action == MENU_DEVICES ? PAGE_DEVICES
                   : action == MENU_CATALOG ? PAGE_CATALOG
                   : action == MENU_VALUE ? PAGE_VALUE : PAGE_HEX);
        return;
    }
    if (!s_lab.client) {
        say("Client unavailable. Select Retry client; firmware must provide BLE ID 6.");
        return;
    }
    if (action == MENU_STOP_SCAN) {
        if (!s_lab.scanning) {
            say("No scan active; current ATT request unchanged.");
            return;
        }
        if (!s_lab.scan_view) {
            s_lab.scan_view = s_lab.view_id; /* The user explicitly requested stop feedback. */
        }
        stop_scan();
        return;
    }
    if (action == MENU_DISCONNECT && !s_lab.scanning) {
        if (s_lab.unpair_stage != UNPAIR_NONE || s_lab.pending_op == GM_BLE_OP_UNBOND) {
            say("Unpair in progress. Wait for the current result.");
            return;
        }
        s_lab.auto_stage = AUTO_NONE;
        s_lab.confirm_enabled = false;
        if (s_lab.connected) {
            begin(GM_BLE_OP_DISCONNECT);
            submitted(s_lab.ble->disconnect(s_lab.client, s_lab.session));
        } else if (s_lab.pending_op == GM_BLE_OP_CONNECT) {
            close_client();
            say("Connect cancelled. Use Retry client after Host cleanup.");
        } else {
            say("No live connection to disconnect.");
        }
        return;
    }
    if (action == MENU_CONNECT || action == MENU_AUTO_CONNECT) {
        connect_selected(action == MENU_AUTO_CONNECT);
        return;
    }
    if ((action == MENU_BONDS || action == MENU_SAVED_DEVICES) &&
        s_lab.scanning && !s_lab.pending_op &&
        s_lab.auto_stage == AUTO_NONE) {
        reset_bonds();
        enter_page(action == MENU_BONDS ? PAGE_BONDS : PAGE_SAVED_DEVICES);
        s_lab.bonds_after_scan = true; /* Its terminal event may already be queued. */
        s_lab.scan_view = s_lab.view_id;
        return; /* Load the list only after the matching SCAN_ENDED. */
    }
    if (s_lab.pending_op || s_lab.scanning || s_lab.auto_stage != AUTO_NONE) {
        say("Busy: wait for completion, stop the scan, or disconnect from the main menu.");
        return;
    }
    gm_plugin_result_t result;
    uint32_t id;
    if (action == MENU_SCAN) {
        gm_ble_scan_filter_t filter;
        s_lab.libc->memset(&filter, 0, sizeof(filter));
        filter.duration_ms = SCAN_DURATION_MS;
        filter.minimum_rssi = -95;
        reset_list(LIST_SCAN);
        result = s_lab.ble->scan_start(s_lab.client, &filter);
        s_lab.scanning = result == GM_PLUGIN_OK;
        s_lab.scan_stopping = s_lab.bonds_after_scan = false;
        s_lab.scan_id = 0;
        s_lab.scan_reports = 0;
        s_lab.scan_since = s_lab.host->monotonic_ms();
        enter_page(PAGE_DEVICES);
        s_lab.scan_view = s_lab.view_id;
        say(result == GM_PLUGIN_OK ? "Scanning. Double-click a device to connect now."
                                   : "Scan submission rejected.");
        return;
    }
    if (action == MENU_CONNECTED || action == MENU_BONDS || action == MENU_SAVED_DEVICES) {
        if (action != MENU_CONNECTED) {
            reset_bonds();
        } else {
            reset_list(LIST_CONNECTED);
        }
        enter_page(action == MENU_CONNECTED ? PAGE_DEVICES
                   : action == MENU_BONDS ? PAGE_BONDS : PAGE_SAVED_DEVICES);
        id = begin(action == MENU_CONNECTED ? LOCAL_CONNECTED_LIST : GM_BLE_OP_GET_BONDED_DEVICES);
        result = action == MENU_CONNECTED ? s_lab.ble->get_connected_devices(s_lab.client)
                                          : get_bond_list(id);
        submitted(result);
        if (action == MENU_SAVED_DEVICES && result == GM_PLUGIN_OK) {
            page_hint();
        }
        return;
    }
    if (action == MENU_UNBOND) {
        if (!s_lab.bonds_valid || s_lab.selected_bond < 0 ||
            (unsigned)s_lab.selected_bond >= s_lab.bond_count) {
            say("Select a peer from a completed valid device list first.");
            return;
        }
        if (s_lab.bond_flags[s_lab.selected_bond] & GM_BLE_BOND_SYSTEM_HOGP) {
            say(unpair_in_app);
            return;
        }
        const gm_ble_device_t *peer = &s_lab.bonds[s_lab.selected_bond];
        if (peer->address_type != 2 && peer->address_type != 3) {
            say("Unpair requires a stable owned identity. Refresh the paired device list.");
            return;
        }
        if (peer->connected && !s_lab.connected) {
            say(unpair_unverified);
            return;
        }
        confirm_action(action, NULL);
        return;
    }
    if (action == MENU_STATE) {
        if (!s_lab.session) {
            say("No current or previous session in this client.");
            return;
        }
        id = begin(GM_BLE_OP_GET_SESSION_STATE);
        submitted(s_lab.ble->get_session_state(s_lab.client, s_lab.session, id));
        return;
    }
    if (!s_lab.connected) {
        say("Connect / attach a peer first.");
        return;
    }
    if (action == MENU_DISCOVER_ALL || action == MENU_REFRESH_ALL) {
        if (action == MENU_REFRESH_ALL && s_lab.refresh == NULL) {
            say("Force refresh unsupported (ID 7). Current catalog kept.");
            return;
        }
        invalidate_catalog();
        enter_page(s_lab.page == PAGE_GATT ? PAGE_GATT : PAGE_CATALOG);
        id = begin(GM_BLE_OP_DISCOVER_ALL);
        if (action == MENU_REFRESH_ALL) {
            result = s_lab.refresh->refresh_all(s_lab.client, s_lab.session, id);
        } else {
            result = s_lab.ble->discover_all(s_lab.client, s_lab.session, id);
        }
        submitted(result);
        return;
    }
    if (action == MENU_PAIR) {
        confirm_action(action, NULL);
        return;
    }
    attribute_t *record = selected_characteristic();
    if (!record) {
        say("Select a characteristic from a completed valid GATT catalog.");
        return;
    }
    if (action == MENU_WRITE || action == MENU_WRITE_COMMAND) {
        unsigned property = action == MENU_WRITE ? 0x08U : 0x04U;
        if (!(record->properties & property) || s_lab.mtu < 4 ||
            s_lab.tx_length > s_lab.caps.max_write || s_lab.tx_length > s_lab.mtu - 3U) {
            say("Write refused locally: property or length exceeds capability/MTU-3.");
            return;
        }
        confirm_action(action, record);
        return;
    }
    if (action == MENU_READ) {
        id = begin(GM_BLE_OP_READ);
        submitted(s_lab.ble->read(s_lab.client, s_lab.session, record->value, id));
        return;
    }
    bool indicate = action == MENU_INDICATE;
    bool enable = action != MENU_UNSUBSCRIBE;
    if (enable && !(record->properties & (indicate ? 0x20U : 0x10U))) {
        say("Characteristic lacks the selected Notify/Indicate property.");
        return;
    }
    id = begin(enable ? GM_BLE_OP_SUBSCRIBE : GM_BLE_OP_UNSUBSCRIBE);
    submitted(
        s_lab.ble->subscribe(s_lab.client, s_lab.session, record->value, enable, indicate, id));
}
static void format_bytes(char *out, unsigned capacity, const uint8_t *bytes, unsigned length)
{
    unsigned count = length < (capacity - 1U) / 2U ? length : (capacity - 1U) / 2U;
    for (unsigned i = 0; i < count; i++) {
        out[i * 2U] = digits[bytes[i] >> 4];
        out[i * 2U + 1U] = digits[bytes[i] & 15U];
    }
    out[count * 2U] = 0;
}
static void format_operation(const attribute_t *record, char *out, unsigned size)
{
    const char *name;
    switch (record->last_operation) {
    case GM_BLE_OP_READ:
        name = "Read";
        break;
    case GM_BLE_OP_WRITE:
        name = "Write";
        break;
    case GM_BLE_OP_WRITE_NO_RESPONSE:
        name = "Write command";
        break;
    case GM_BLE_OP_SUBSCRIBE:
        name = "Subscribe";
        break;
    case GM_BLE_OP_UNSUBSCRIBE:
        name = "Unsubscribe";
        break;
    default:
        name = "Operation";
        break;
    }
    if (!record->result_state) {
        const char *hint = "Choose an operation for this characteristic.";
        if (!owning_service(record)) {
            hint = "Unknown service: read-only here.";
        } else if (protected_characteristic(record)) {
            hint = "No value writes. Host blocks subscriptions on shared HOGP.";
        }
        s_lab.libc->snprintf(out, size, "%s", hint);
    } else if (record->result_state == 1) {
        s_lab.libc->snprintf(out, size, "%s: waiting for result...", name);
    } else if (record->last_result != GM_PLUGIN_OK || record->att_error) {
        s_lab.libc->snprintf(out, size, "%s FAILED rc=%ld ATT=%02X", name,
                             (long)record->last_result, (unsigned)record->att_error);
    } else if (record->last_operation == GM_BLE_OP_READ) {
        char hex[17];
        hex[0] = 0;
        bool current = s_lab.value_session == s_lab.session && s_lab.value_handle == record->value
            && s_lab.value_type == GM_BLE_EVENT_RESULT;
        if (current) {
            format_bytes(hex, sizeof(hex), s_lab.value, s_lab.value_length);
        }
        s_lab.libc->snprintf(out, size, "Read OK %u B%s%s%s", (unsigned)record->read_length,
                             current ? ": " : "", hex, current && s_lab.value_length > 8 ? "..." : "");
    } else if (record->last_operation == GM_BLE_OP_WRITE_NO_RESPONSE) {
        s_lab.libc->snprintf(out, size, "Write command: LOCAL SUBMISSION ONLY");
    } else if (record->last_operation == GM_BLE_OP_SUBSCRIBE ||
               record->last_operation == GM_BLE_OP_UNSUBSCRIBE) {
        s_lab.libc->snprintf(out, size, "%s: confirmed %s", name, record->subscribed ? "ON" : "OFF");
    } else {
        s_lab.libc->snprintf(out, size, "Write OK (ATT response)");
    }
}
static unsigned gatt_count(void)
{
    unsigned count = 0;
    if (s_lab.catalog_valid && s_lab.catalog_all) {
        for (unsigned i = 0; i < s_lab.catalog_count; i++) {
            count += s_lab.catalog[i].type == GM_BLE_EVENT_SERVICE ||
                     s_lab.catalog[i].type == GM_BLE_EVENT_CHARACTERISTIC;
        }
    }
    return count;
}
static int gatt_index(unsigned row)
{
    if (s_lab.catalog_valid && s_lab.catalog_all) {
        for (unsigned i = 0; i < s_lab.catalog_count; i++) {
            if (s_lab.catalog[i].type == GM_BLE_EVENT_SERVICE ||
                s_lab.catalog[i].type == GM_BLE_EVENT_CHARACTERISTIC) {
                if (row == 0) {
                    return (int)i;
                }
                row--;
            }
        }
    }
    return -1;
}
static unsigned characteristic_actions(uint8_t actions[CHARACTERISTIC_ACTION_MAX])
{
    attribute_t *record = selected_characteristic();
    unsigned count = 0;
    if (!record) {
        return 0;
    }
    if (record->properties & 0x02U) {
        actions[count++] = MENU_READ;
    }
    if (!protected_characteristic(record)) {
        if (record->properties & 0x08U) {
            actions[count++] = MENU_WRITE;
        }
        if (record->properties & 0x04U) {
            actions[count++] = MENU_WRITE_COMMAND;
        }
    }
    /* Capabilities stay visible; the Host checks live HOGP ownership for every
     * request, including takeover after this catalog was loaded. */
    if (owning_service(record) && (record->properties & 0x30U)) {
        if (record->properties & 0x10U) {
            actions[count++] = MENU_NOTIFY;
        }
        if (record->properties & 0x20U) {
            actions[count++] = MENU_INDICATE;
        }
        actions[count++] = MENU_UNSUBSCRIBE;
    }
    actions[count++] = MENU_VALUE;
    return count;
}
static void format_uuid(const gm_ble_uuid_t *uuid, char out[33])
{
    if (uuid->length == 2) {
        s_lab.libc->snprintf(out, 33, "%04X", (unsigned)uuid->bytes[0] | ((unsigned)uuid->bytes[1] << 8));
    } else {
        format_bytes(out, 33, uuid->bytes, uuid->length); /* 128-bit bytes remain in BLE order. */
    }
}
static unsigned page_count(void)
{
    switch (s_lab.page) {
    case PAGE_MENU:
        return s_lab.advanced ? sizeof(advanced_menu) : sizeof(basic_menu);
    case PAGE_DEVICES: {
        unsigned peers = device_rows();
        /* Keep the mode one click after the first peer, even before it arrives. */
        return scan_list_visible() ? 1U + (peers ? peers : 1U) : peers;
    }
    case PAGE_BONDS:
        return s_lab.bond_count;
    case PAGE_SAVED_DEVICES:
        return s_lab.bonds_valid ? s_lab.bond_count : 0;
    case PAGE_CATALOG:
        return s_lab.catalog_count;
    case PAGE_GATT:
        return gatt_count() + 2U; /* Explicit remote refresh and diagnostics. */
    case PAGE_CHARACTERISTIC: {
        uint8_t actions[CHARACTERISTIC_ACTION_MAX];
        return characteristic_actions(actions);
    }
    case PAGE_COMMON_SERVICES:
        return sizeof(common_services) / sizeof(common_services[0]);
    case PAGE_HEX:
        return 7;
    case PAGE_CONFIRM:
        return 2;
    default:
        return 1;
    }
}
static void row_text(unsigned index, char *out, unsigned capacity)
{
    if (s_lab.page == PAGE_MENU) {
        s_lab.libc->snprintf(out, capacity, "%s", menu_label(menu_action(index)));
    } else if (s_lab.page == PAGE_COMMON_SERVICES) {
        s_lab.libc->snprintf(out, capacity, "%s", common_services[index].label);
    } else if (s_lab.page == PAGE_GATT) {
        int slot = gatt_index(index);
        if (slot >= 0) {
            const attribute_t *record = &s_lab.catalog[slot];
            char uuid[33];
            format_uuid(&record->uuid, uuid);
            if (record->type == GM_BLE_EVENT_SERVICE) {
                s_lab.libc->snprintf(out, capacity, "Service %s start=%04X", uuid,
                                     (unsigned)record->handle);
            } else {
                unsigned properties = record->properties;
                s_lab.libc->snprintf(out, capacity, "  Char [%s%s%s%s] %s value=%04X",
                                     properties & 0x02U ? "R" : "", properties & 0x0cU ? "W" : "",
                                     properties & 0x10U ? "N" : "", properties & 0x20U ? "I" : "",
                                     uuid, (unsigned)record->value);
            }
        } else {
            s_lab.libc->snprintf(out, capacity, "%s", index == gatt_count()
                ? menu_names[MENU_REFRESH_ALL] : "More tools / diagnostics");
        }
    } else if (s_lab.page == PAGE_CHARACTERISTIC) {
        uint8_t actions[CHARACTERISTIC_ACTION_MAX];
        unsigned count = characteristic_actions(actions);
        const char *label = "Unavailable";
        if (index < count) {
            unsigned action = actions[index];
            label = action == MENU_READ ? "Read" :
                action == MENU_WRITE ? "Write HEX (confirm)" :
                action == MENU_WRITE_COMMAND ? "Write HEX (no response)" :
                action == MENU_NOTIFY ? "Subscribe Notify" :
                action == MENU_INDICATE ? "Subscribe Indicate" :
                action == MENU_UNSUBSCRIBE ? "Unsubscribe" : "Latest data / HEX";
        }
        s_lab.libc->snprintf(out, capacity, "%s", label);
    } else if (s_lab.page == PAGE_DEVICES || s_lab.page == PAGE_BONDS ||
               s_lab.page == PAGE_SAVED_DEVICES) {
        if (scan_list_visible()) {
            if (index == SCAN_MODE_ROW) {
                s_lab.libc->snprintf(out, capacity, "%s", s_lab.scan_connect_only
                    ? "Mode: Connect only" : "Mode: Auto checks");
                return;
            }
            index -= index > SCAN_MODE_ROW;
            if (!s_lab.device_count && !s_lab.peer_known) {
                s_lab.libc->snprintf(out, capacity, "%s", s_lab.scanning
                    ? "Waiting for devices..." : "No devices found");
                return;
            }
        }
        const gm_ble_device_t *peer = s_lab.page == PAGE_DEVICES ? device_row(index) : &s_lab.bonds[index];
        const char *retained = peer == &s_lab.peer ? (s_lab.connected ? " [session]" : " [last]") : "";
        if (s_lab.page != PAGE_DEVICES && (s_lab.bond_flags[index] & GM_BLE_BOND_SYSTEM_HOGP)) {
            retained = " [App-bound HOGP]";
        }
        if (!s_lab.advanced) {
            s_lab.libc->snprintf(out, capacity, "%u %s [%02X:%02X]%s", index + 1U,
                                 peer->name[0] ? peer->name : "(unnamed)",
                                 (unsigned)peer->address[1], (unsigned)peer->address[0], retained);
        } else {
            s_lab.libc->snprintf(out, capacity, "%u %s [%02X:%02X] t%u %ddBm%s", index + 1U,
                                 peer->name[0] ? peer->name : "(unnamed)", (unsigned)peer->address[1],
                                 (unsigned)peer->address[0], (unsigned)peer->address_type,
                                 (int)peer->rssi, retained);
        }
    } else if (s_lab.page == PAGE_CATALOG) {
        const attribute_t *record = &s_lab.catalog[index];
        char uuid[33];
        if (s_lab.catalog_valid && !s_lab.catalog_all &&
            (record->type == GM_BLE_EVENT_SERVICE || record->type == GM_BLE_EVENT_CHARACTERISTIC)) {
            format_uuid(&record->uuid, uuid);
            s_lab.libc->snprintf(out, capacity, "%s %s",
                                 record->type == GM_BLE_EVENT_SERVICE ? "Service" : "  Characteristic",
                                 uuid);
            return;
        }
        format_bytes(uuid, sizeof(uuid), record->uuid.bytes, record->uuid.length);
        s_lab.libc->snprintf(out, capacity, "%s %04X v%04X p%02X %s",
                             record->type == GM_BLE_EVENT_CHARACTERISTIC ? "CHAR"
                             : record->type == GM_BLE_EVENT_SERVICE      ? "SVC"
                             : record->type == GM_BLE_EVENT_DESCRIPTOR   ? "DESC"
                                                                         : "ATTR",
                             (unsigned)record->handle, (unsigned)record->value,
                             (unsigned)record->properties, uuid);
    } else if (s_lab.page == PAGE_CONFIRM) {
        s_lab.libc->snprintf(out, capacity, "%s", index == 0 ? "CANCEL" : "CONFIRM operation");
    } else {
        static const char *const editors[] = {"Cursor +1 nibble",
                                              "Cursor -1 nibble",
                                              "Digit +1",
                                              "Digit -1",
                                              "Length +1 byte",
                                              "Length -1 byte",
                                              "Done"};
        s_lab.libc->snprintf(out, capacity, "%s", editors[index]);
    }
}
static void render(void)
{
    if (!s_lab.started || s_lab.suspended || s_call_ui.active || !s_lab.dirty) {
        return;
    }
    char line[100];
    char detail[176];
    unsigned count = page_count();
    unsigned first = s_lab.rows ? s_lab.selection / s_lab.rows * s_lab.rows : 0;
    unsigned used = 0;
    bool notification = false;
    s_lab.screen[0] = 0;
    if (s_lab.page == PAGE_VALUE) {
        attribute_t *record = selected_characteristic();
        bool matches = !s_lab.gatt_view || (record && s_lab.value_session == s_lab.session &&
                                            s_lab.value_handle == record->value);
        unsigned length = matches ? s_lab.value_length : 0;
        unsigned hex_rows = s_lab.rows;
        notification = matches && s_lab.value_type == GM_BLE_EVENT_NOTIFICATION &&
            s_lab.value_session == s_lab.session && s_lab.value_handle != 0;
        if (notification && !s_lab.gatt_view && hex_rows > 1U) {
            int n = s_lab.libc->snprintf(
                s_lab.screen, sizeof(s_lab.screen), "R:%u S:%u E:%u RX:%lu Loss:%lu\n",
                (unsigned)s_lab.auto_reads, (unsigned)s_lab.auto_subscriptions,
                (unsigned)s_lab.auto_errors, (unsigned long)s_lab.notifications,
                (unsigned long)s_lab.notification_drops);
            if (n > 0 && (unsigned)n < sizeof(s_lab.screen)) {
                used = (unsigned)n;
                hex_rows--;
            }
        }
        for (unsigned i = s_lab.value_offset;
             i < length && i < s_lab.value_offset + hex_rows * 16U; i += 16U) {
            unsigned bytes = s_lab.value_length - i < 16 ? s_lab.value_length - i : 16;
            char hex[33];
            format_bytes(hex, sizeof(hex), s_lab.value + i, bytes);
            int n = s_lab.libc->snprintf(s_lab.screen + used, sizeof(s_lab.screen) - used,
                                         "Offset %03u: %s\n", i, hex);
            if (n < 0 || (unsigned)n >= sizeof(s_lab.screen) - used) {
                break;
            }
            used += (unsigned)n;
        }
        if (!matches) {
            s_lab.libc->snprintf(s_lab.screen, sizeof(s_lab.screen), "No latest data for this characteristic.");
        }
        if (notification) {
            /* Resolve the actual received Value Handle, not the selected row
             * or the characteristic UUID (which can occur more than once). */
            const attribute_t *source = characteristic_by_handle(s_lab.value_handle);
            const attribute_t *service = source ? owning_service(source) : NULL;
            char service_uuid[33];
            char char_uuid[33];
            const char *service_text = "unknown";
            const char *char_text = "unknown";
            if (service && (service->uuid.length == 2 || service->uuid.length == 16)) {
                format_uuid(&service->uuid, service_uuid);
                service_text = service_uuid;
            }
            if (source && (source->uuid.length == 2 || source->uuid.length == 16)) {
                format_uuid(&source->uuid, char_uuid);
                char_text = char_uuid;
            }
            s_lab.libc->snprintf(detail, sizeof(detail), "Service UUID: %s\nChar UUID: %s",
                                 service_text, char_text);
        } else if (s_lab.gatt_view) {
            s_lab.libc->snprintf(detail, sizeof(detail), "%s h=%04X bytes=%u\nHold: return to operations",
                                 matches ? "READ" : "No data",
                                 record ? (unsigned)record->value : 0U, length);
        } else if (!s_lab.advanced) {
            s_lab.libc->snprintf(detail, sizeof(detail),
                                 "Reads=%u Subs=%u Errors=%u\nRX:%lu Loss:%lu Bytes=%u",
                                 (unsigned)s_lab.auto_reads, (unsigned)s_lab.auto_subscriptions,
                                 (unsigned)s_lab.auto_errors, (unsigned long)s_lab.notifications,
                                 (unsigned long)s_lab.notification_drops,
                                 (unsigned)s_lab.value_length);
        } else {
            s_lab.libc->snprintf(detail, sizeof(detail),
                                 "READ h=%04X bytes=%u\nRX:%lu Loss:%lu",
                                 (unsigned)s_lab.value_handle, (unsigned)s_lab.value_length,
                                 (unsigned long)s_lab.notifications,
                                 (unsigned long)s_lab.notification_drops);
        }
    } else {
        for (unsigned i = first; i < count && i < first + s_lab.rows; i++) {
            row_text(i, line, sizeof(line));
            int n = s_lab.libc->snprintf(s_lab.screen + used, sizeof(s_lab.screen) - used,
                                         "%c %s\n", i == s_lab.selection ? '>' : ' ', line);
            if (n < 0 || (unsigned)n >= sizeof(s_lab.screen) - used) {
                break;
            }
            used += (unsigned)n;
        }
        s_lab.libc->snprintf(detail, sizeof(detail), "%s", s_lab.message);
        if (s_lab.page == PAGE_CHARACTERISTIC) {
            const attribute_t *record = selected_characteristic();
            if (record) {
                s_lab.libc->snprintf(detail, sizeof(detail), "%s\nSub:%s Notifications:%lu",
                                     s_lab.message, record->subscription_known ? (record->subscribed ? "ON" : "OFF") : "?",
                                     (unsigned long)record->notifications);
            }
        } else if (s_lab.page == PAGE_CATALOG && s_lab.catalog_all &&
            s_lab.selection < s_lab.catalog_count) {
            const attribute_t *record = &s_lab.catalog[s_lab.selection];
            char uuid[33];
            format_bytes(uuid, sizeof(uuid), record->uuid.bytes, record->uuid.length);
            s_lab.libc->snprintf(
                detail, sizeof(detail), "h=%04X end=%04X value=%04X p=%02X sub=%u\nUUID BLE: %s",
                (unsigned)record->handle, (unsigned)record->end, (unsigned)record->value,
                (unsigned)record->properties, (unsigned)record->subscribed, uuid);
        } else if (s_lab.page == PAGE_HEX) {
            unsigned length = s_lab.tx_length;
            unsigned start = s_lab.cursor / 32U * 16U;
            char hex[33];
            unsigned visible = length - start < 16 ? length - start : 16;
            format_bytes(hex, sizeof(hex), s_lab.tx + start, visible);
            s_lab.libc->snprintf(detail, sizeof(detail),
                                 "BLE byte-order HEX [%u bytes] nibble=%u\n@%u: %s", length,
                                 (unsigned)s_lab.cursor, start, hex);
        } else if (s_lab.page == PAGE_CONFIRM) {
            char hex[33];
            format_bytes(hex, sizeof(hex), s_lab.confirm_tx, s_lab.confirm_length);
            if (s_lab.confirm_action == MENU_UNBOND) {
                const gm_ble_device_t *peer = &s_lab.confirm_peer;
                s_lab.libc->snprintf(detail, sizeof(detail),
                                     "UNBOND %02X:%02X:%02X:%02X:%02X:%02X type=%u",
                                     (unsigned)peer->address[5], (unsigned)peer->address[4],
                                     (unsigned)peer->address[3], (unsigned)peer->address[2],
                                     (unsigned)peer->address[1], (unsigned)peer->address[0],
                                     (unsigned)peer->address_type);
            } else if (s_lab.confirm_action == MENU_PAIR) {
                s_lab.libc->snprintf(detail, sizeof(detail),
                                     "PAIR session=%lu\nQuery session state after completion.",
                                     (unsigned long)s_lab.confirm_session);
            } else {
                s_lab.libc->snprintf(detail, sizeof(detail), "%s session=%lu h=%04X bytes=%u\n%s%s",
                                     menu_names[s_lab.confirm_action],
                                     (unsigned long)s_lab.confirm_session,
                                     (unsigned)s_lab.confirm_handle, (unsigned)s_lab.confirm_length,
                                     hex, s_lab.confirm_length > 16 ? " (first 16 bytes)" : "");
            }
        }
    }
    s_lab.ui->label_set_text(s_lab.body,
                             count || s_lab.page == PAGE_VALUE ? s_lab.screen : "(empty)");
    s_lab.ui->label_set_text(s_lab.status, detail);
    s_lab.ui->label_set_text(s_lab.hint,
                             "Click: next  Double: select\nHold: back  HOME: exit");
    if (notification) {
        s_lab.libc->snprintf(line, sizeof(line), "Notify value 0x%04X (%u B)",
                             (unsigned)s_lab.value_handle, (unsigned)s_lab.value_length);
    } else if (s_lab.page == PAGE_CHARACTERISTIC) {
        const attribute_t *record = selected_characteristic();
        char uuid[33];
        uuid[0] = 0;
        if (record) {
            format_uuid(&record->uuid, uuid);
        }
        unsigned properties = record ? record->properties : 0;
        s_lab.libc->snprintf(line, sizeof(line), "%s h=%04X [%s%s%s%s]", uuid,
                             record ? (unsigned)record->value : 0U,
                             properties & 0x02U ? "R" : "", properties & 0x0cU ? "W" : "",
                             properties & 0x10U ? "N" : "", properties & 0x20U ? "I" : "");
    } else if (s_lab.page == PAGE_GATT) {
        s_lab.libc->snprintf(line, sizeof(line), "GATT services / characteristics%s",
                             s_lab.pending_op == GM_BLE_OP_DISCOVER_ALL ? " (loading)" : "");
    } else if (s_lab.page == PAGE_COMMON_SERVICES) {
        s_lab.libc->snprintf(line, sizeof(line), "%s", menu_names[MENU_COMMON_SERVICES]);
    } else if (s_lab.page == PAGE_BONDS) {
        s_lab.libc->snprintf(line, sizeof(line), "Unpair / paired devices");
    } else if (s_lab.page == PAGE_SAVED_DEVICES) {
        s_lab.libc->snprintf(line, sizeof(line), "Devices / paired devices");
    } else if (s_lab.page == PAGE_DEVICES) {
        s_lab.libc->snprintf(line, sizeof(line), "%s", s_lab.scanning
            ? s_lab.scan_stopping ? "Scan & Connect / stopping" : "Scan & Connect"
            : s_lab.list_kind == LIST_CONNECTED ? "Devices / connected snapshot" : "Devices / recent peers");
    } else if (!s_lab.advanced) {
        s_lab.libc->snprintf(line, sizeof(line), "BLE GATT | %s",
                             s_lab.pending_op == GM_BLE_OP_CONNECT ? "Connecting" :
                             s_lab.connected ? "Connected" : "Offline");
    } else {
        attribute_t *selected = selected_characteristic();
        s_lab.libc->snprintf(line, sizeof(line), "Advanced c=%lu s=%lu %s h=%04X",
                             (unsigned long)s_lab.client, (unsigned long)s_lab.session,
                             s_lab.connected ? "CONNECTED" : "OFFLINE",
                             selected ? (unsigned)selected->value : 0U);
    }
    s_lab.ui->label_set_text(s_lab.title, line);
    if (page_shows_notifications()) {
        s_lab.notification_rendered_at = s_lab.host->monotonic_ms();
    }
    s_lab.notification_redraw_pending = false;
    s_lab.dirty = false;
    s_call_ui.redraw_pending = false;
}
static void editor_activate(void)
{
    if (s_lab.selection == 6) {
        if (s_lab.edit_write) {
            s_lab.edit_write = false;
            enter_page(PAGE_CHARACTERISTIC);
            attribute_t *record = selected_characteristic();
            if (!record || s_lab.confirm_session != s_lab.session ||
                s_lab.confirm_handle != record->value) {
                say("Write target changed; nothing sent.");
                return;
            }
            run_action(s_lab.confirm_action); /* Existing bounds checks and explicit confirmation. */
            return;
        }
        enter_page(PAGE_MENU);
        return;
    }
    unsigned length = s_lab.tx_length;
    if (s_lab.selection < 2) {
        s_lab.cursor = (uint16_t)((s_lab.cursor + length * 2U + (s_lab.selection == 0 ? 1 : -1)) %
                                  (length * 2U));
    } else if (s_lab.selection < 4) {
        unsigned byte = s_lab.cursor / 2U;
        unsigned shift = s_lab.cursor % 2U ? 0 : 4;
        unsigned nibble = ((s_lab.tx[byte] >> shift) + (s_lab.selection == 2 ? 1 : 15)) & 15U;
        s_lab.tx[byte] = (uint8_t)((s_lab.tx[byte] & ~(15U << shift)) | (nibble << shift));
    } else {
        if (s_lab.selection == 4 && s_lab.tx_length < GM_BLE_WRITE_MAX) {
            s_lab.tx[s_lab.tx_length] = 0;
            s_lab.tx_length++;
        } else if (s_lab.selection == 5 && s_lab.tx_length > 1) {
            s_lab.tx_length--;
        }
        s_lab.cursor %= s_lab.tx_length * 2U;
    }
}
static void find_common_service(unsigned index)
{
    if (index >= sizeof(common_services) / sizeof(common_services[0])) {
        return;
    }
    if (!s_lab.client || !s_lab.connected) {
        say("Connect / attach a peer first.");
        return;
    }
    if (s_lab.pending_op || s_lab.scanning || s_lab.auto_stage != AUTO_NONE) {
        say("Wait for the current operation before finding a service.");
        return;
    }
    invalidate_catalog();
    enter_page(PAGE_CATALOG);
    uint32_t id = begin(GM_BLE_OP_DISCOVER);
    gm_plugin_result_t result =
        s_lab.ble->discover_service(s_lab.client, s_lab.session, &common_services[index].uuid, id);
    s_lab.host->log("[ble_lab] FIND %s id=%lu rc=%ld", common_services[index].label,
                    (unsigned long)id, (long)result);
    if (result == GM_PLUGIN_OK) {
        say("Finding service. Waiting for the device...");
    } else {
        s_lab.pending_op = 0;
        say("Service discovery failed to start. See log for details.");
    }
}
static void activate(void)
{
    if (s_lab.page == PAGE_MENU) {
        run_action((uint8_t)menu_action(s_lab.selection));
    } else if (s_lab.page == PAGE_COMMON_SERVICES) {
        find_common_service(s_lab.selection);
    } else if (s_lab.page == PAGE_GATT) {
        int slot = gatt_index(s_lab.selection);
        if (slot >= 0 && s_lab.catalog[slot].type == GM_BLE_EVENT_CHARACTERISTIC) {
            s_lab.selected_attribute = (int16_t)slot;
            s_lab.gatt_view = true;
            s_lab.advanced = false;
            enter_page(PAGE_CHARACTERISTIC);
        } else if (slot >= 0) {
            say("Service header; select a characteristic below it.");
        } else if (s_lab.selection == gatt_count()) {
            run_action(MENU_REFRESH_ALL);
        } else {
            run_action(MENU_ADVANCED);
        }
    } else if (s_lab.page == PAGE_CHARACTERISTIC) {
        uint8_t actions[CHARACTERISTIC_ACTION_MAX];
        unsigned count = characteristic_actions(actions);
        if (s_lab.selection >= count) {
            return;
        }
        unsigned action = actions[s_lab.selection];
        if (action == MENU_WRITE || action == MENU_WRITE_COMMAND) {
            if (s_lab.pending_op || s_lab.scanning || s_lab.auto_stage != AUTO_NONE) {
                say("Busy: wait before editing a write.");
                return;
            }
            const attribute_t *record = selected_characteristic();
            s_lab.confirm_session = s_lab.session;
            s_lab.confirm_handle = record->value;
            s_lab.confirm_action = (uint8_t)action;
            s_lab.edit_write = true;
            enter_page(PAGE_HEX);
            say("Edit HEX, select Done, then confirm. No write sent yet.");
        } else {
            run_action((uint8_t)action);
        }
    } else if (s_lab.page == PAGE_DEVICES || s_lab.page == PAGE_BONDS ||
               s_lab.page == PAGE_SAVED_DEVICES) {
        bool bond_page = s_lab.page == PAGE_BONDS;
        bool saved_page = s_lab.page == PAGE_SAVED_DEVICES;
        bool scan_list = scan_list_visible();
        if (scan_list && s_lab.selection == SCAN_MODE_ROW) {
            s_lab.scan_connect_only = !s_lab.scan_connect_only;
            /* Continue on the same scan; select the first peer to connect. */
            s_lab.selection = 0;
            say(s_lab.scan_connect_only ? "Connect only: no automatic pairing or discovery."
                                       : "Auto checks: select a device to connect and run checks.");
            return;
        }
        if ((bond_page || saved_page) && s_lab.scanning) {
            say(s_lab.scan_stopping ? "Stopping scan; plugin bonds will load next."
                                   : "Scan still active. Reopen this page to retry stopping it.");
            return;
        }
        const gm_ble_device_t *peer = NULL;
        if (bond_page || saved_page) {
            if (s_lab.bonds_valid && s_lab.selection < s_lab.bond_count) {
                s_lab.selected_bond = (int16_t)s_lab.selection;
                peer = &s_lab.bonds[s_lab.selection];
            }
        } else {
            unsigned index = s_lab.selection;
            index -= scan_list && index > SCAN_MODE_ROW;
            const gm_ble_device_t *row = device_row(index);
            if (row && (device_list_ready() || row == &s_lab.peer)) {
                peer = row;
                s_lab.selected_device = row == &s_lab.peer ? SELECT_LAST_PEER : (int16_t)index;
            }
        }
        if (peer) {
            bool connect_only = !saved_page && !bond_page &&
                (scan_list ? s_lab.scan_connect_only : s_lab.advanced);
            run_action(bond_page ? MENU_UNBOND : connect_only ? MENU_CONNECT : MENU_AUTO_CONNECT);
        } else if (bond_page && s_lab.bonds_valid && !s_lab.bond_count) {
            say(no_plugin_bonds);
        } else if (saved_page) {
            page_hint();
        } else if (!s_lab.scanning && !device_rows()) {
            say("No device list. Use Scan & Connect to refresh.");
        } else {
            say(s_lab.list_kind == LIST_SCAN
                    ? "No device at this row. Keep scanning or scan again."
                    : "List incomplete/invalid. Wait for completion or refresh.");
        }
    } else if (s_lab.page == PAGE_CATALOG) {
        if (s_lab.catalog_valid && s_lab.selection < s_lab.catalog_count &&
            s_lab.catalog[s_lab.selection].type == GM_BLE_EVENT_CHARACTERISTIC) {
            s_lab.selected_attribute = (int16_t)s_lab.selection;
            s_lab.gatt_view = true;
            s_lab.advanced = false;
            enter_page(PAGE_CHARACTERISTIC);
        } else {
            s_lab.selected_attribute = -1;
            say("Select a CHARACTERISTIC from a completed valid catalog, not a "
                "service/descriptor.");
        }
    } else if (s_lab.page == PAGE_CONFIRM) {
        if (s_lab.selection == 1) {
            execute_confirmed();
        } else {
            s_lab.confirm_enabled = false;
            enter_page(s_lab.gatt_view ? PAGE_CHARACTERISTIC : PAGE_MENU);
            s_lab.selection = 0;
            say("Cancelled; nothing sent.");
        }
    } else if (s_lab.page == PAGE_HEX) {
        editor_activate();
    }
    s_lab.dirty = true;
}
static bool plugin_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (!s_lab.started || s_lab.suspended || s_call_ui.active || event == NULL ||
        event->struct_size < GM_PLUGIN_EVENT_MIN_SIZE || event->type != GM_PLUGIN_EVENT_BUTTON) {
        return false;
    }
    gm_plugin_button_t button = event->data.button.button;
    gm_plugin_button_action_t action = event->data.button.action;
    bool move = button == GM_PLUGIN_BUTTON_PRIMARY && action == GM_PLUGIN_BUTTON_ACTION_SINGLE;
    bool select = button == GM_PLUGIN_BUTTON_PRIMARY && action == GM_PLUGIN_BUTTON_ACTION_DOUBLE;
    bool back = button == GM_PLUGIN_BUTTON_PRIMARY && action == GM_PLUGIN_BUTTON_ACTION_LONG;
    if (action == GM_PLUGIN_BUTTON_ACTION_TRIGGER) {
        if (button == GM_PLUGIN_BUTTON_HOME) {
            s_lab.host->app_exit();
            return true;
        }
        move = button == GM_PLUGIN_BUTTON_DOWN || button == GM_PLUGIN_BUTTON_SCROLL_DOWN ||
               button == GM_PLUGIN_BUTTON_PAGE_DOWN;
        select = button == GM_PLUGIN_BUTTON_RIGHT;
        back = button == GM_PLUGIN_BUTTON_LEFT || button == GM_PLUGIN_BUTTON_BACK;
        if (button == GM_PLUGIN_BUTTON_UP || button == GM_PLUGIN_BUTTON_SCROLL_UP ||
            button == GM_PLUGIN_BUTTON_PAGE_UP) {
            if (s_lab.page == PAGE_VALUE) {
                s_lab.value_offset = s_lab.value_offset >= 16 ? s_lab.value_offset - 16 : 0;
            } else {
                unsigned count = page_count();
                if (count) {
                    s_lab.selection = (uint16_t)((s_lab.selection + count - 1U) % count);
                }
            }
            s_lab.dirty = true;
            return true;
        }
    }
    if (move) {
        if (s_lab.page == PAGE_VALUE) {
            if (s_lab.value_offset + 16U < s_lab.value_length) {
                s_lab.value_offset += 16;
            }
        } else {
            unsigned count = page_count();
            if (count) {
                s_lab.selection = (uint16_t)((s_lab.selection + 1U) % count);
            }
        }
    } else if (select) {
        activate();
    } else if (back) {
        s_lab.confirm_enabled = false;
        if (s_lab.gatt_view && (s_lab.page == PAGE_HEX || s_lab.page == PAGE_VALUE ||
                                s_lab.page == PAGE_CONFIRM)) {
            s_lab.edit_write = false;
            enter_page(PAGE_CHARACTERISTIC);
        } else if (s_lab.page == PAGE_CHARACTERISTIC) {
            s_lab.gatt_view = false;
            enter_page(s_lab.catalog_all ? PAGE_GATT : PAGE_CATALOG);
        } else {
            s_lab.gatt_view = s_lab.edit_write = false;
            if (s_lab.page == PAGE_MENU || s_lab.page == PAGE_GATT) {
                s_lab.advanced = false;
            }
            enter_page(PAGE_MENU);
        }
        s_lab.selection = 0;
    } else {
        return false;
    }
    s_lab.dirty = true;
    return true;
}
static void plugin_stop(void *context)
{
    (void)context;
    close_client();
    s_lab.started = false;
    if (s_call_ui.root != NULL) {
        s_lab.ui->obj_clean(s_call_ui.root);
    }
    example_call_ui_stop(&s_call_ui);
    s_lab.title = s_lab.body = s_lab.status = s_lab.hint = NULL;
}
static gm_plugin_result_t plugin_start(void *context)
{
    (void)context;
    gm_plugin_display_info_t display;
    gm_plugin_result_t result = s_lab.host->display_get_info(&display);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    if (display.width < 240 || display.height < 180 || display.width > 32767 ||
        display.height > 32767) {
        return GM_PLUGIN_ENOTSUP;
    }
    result = example_call_ui_start(&s_call_ui);
    if (result != GM_PLUGIN_OK || s_call_ui.root == NULL) {
        example_call_ui_stop(&s_call_ui);
        return result != GM_PLUGIN_OK ? result : GM_PLUGIN_ESTATE;
    }
    s_lab.ui->obj_clean(s_call_ui.root);
    s_lab.title = s_lab.ui->label_create(s_call_ui.root);
    s_lab.body = s_lab.ui->label_create(s_call_ui.root);
    s_lab.status = s_lab.ui->label_create(s_call_ui.root);
    s_lab.hint = s_lab.ui->label_create(s_call_ui.root);
    if (!s_lab.title || !s_lab.body || !s_lab.status || !s_lab.hint) {
        plugin_stop(NULL);
        return GM_PLUGIN_ENOMEM;
    }
    unsigned line = 22;
    if (s_lab.ui->font_default && s_lab.ui->font_get_line_height) {
        unsigned measured = (unsigned)s_lab.ui->font_get_line_height(s_lab.ui->font_default);
        if (measured >= 12 && measured <= 40) {
            line = measured + 2;
        }
    }
    unsigned reserved = line * 5U;
    if (reserved + line > display.height) {
        plugin_stop(NULL);
        return GM_PLUGIN_ENOTSUP;
    }
    s_lab.rows = (uint8_t)((display.height - reserved) / line);
    if (s_lab.rows > 6) {
        s_lab.rows = 6;
    }
    gm_plugin_lvgl_obj_t *labels[] = {s_lab.title, s_lab.body, s_lab.status, s_lab.hint};
    unsigned heights[] = {line, s_lab.rows * line, line * 2U, line * 2U};
    unsigned y = 0;
    for (unsigned i = 0; i < 4; i++) {
        s_lab.ui->obj_set_pos(labels[i], 6, (int16_t)y);
        s_lab.ui->obj_set_size(labels[i], (int16_t)(display.width - 12U), (int16_t)heights[i]);
        s_lab.ui->label_set_long_mode(labels[i], GM_PLUGIN_LVGL_LABEL_CLIP);
        s_lab.ui->style_set(labels[i], GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                            gm_plugin_lvgl_style_color(0xff), GM_PLUGIN_LVGL_SELECTOR_MAIN);
        y += heights[i];
    }
    s_lab.ui->style_set(s_call_ui.root, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        gm_plugin_lvgl_style_color(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    s_lab.ui->style_set(s_call_ui.root, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                        gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    enter_page(PAGE_MENU);
    s_lab.selection = s_lab.cursor = 0;
    s_lab.selected_device = s_lab.selected_attribute = -1;
    s_lab.suspended = false;
    s_lab.advanced = false;
    s_lab.started = true;
    if (s_lab.tx_length == 0) {
        s_lab.tx_length = 1;
    }
    open_client();
    s_lab.dirty = true;
    render();
    return GM_PLUGIN_OK;
}
static void plugin_loop(void *context, uint32_t elapsed_ms)
{
    (void)context;
    (void)elapsed_ms;
    uint32_t now = s_lab.host->monotonic_ms();
    if (s_lab.notification_redraw_pending && page_shows_notifications()
        && (uint32_t)(now - s_lab.notification_rendered_at) >= NOTIFICATION_REDRAW_MS) {
        s_lab.dirty = true;
    }
    if (s_lab.connected && s_lab.notification_drops != s_lab.notification_drops_logged
        && (!s_lab.notification_drops_logged ||
            (uint32_t)(now - s_lab.notification_log_at) >= NOTIFICATION_LOSS_LOG_MS)) {
        s_lab.notification_drops_logged = s_lab.notification_drops;
        s_lab.notification_log_at = now;
        s_lab.host->log("[ble_lab] notify loss session=%lu total=%lu; connection retained",
                        (unsigned long)s_lab.session, (unsigned long)s_lab.notification_drops);
        s_lab.dirty = true;
    }
    if (s_call_ui.redraw_pending) {
        if (s_lab.pending_op) {
            s_lab.pending_since = now;
        }
        if (s_lab.scanning) {
            s_lab.scan_since = now;
        }
        s_lab.dirty = true;
    }
    if (!s_lab.suspended && !s_call_ui.active &&
        ((s_lab.pending_op && (uint32_t)(now - s_lab.pending_since) >= REQUEST_TIMEOUT_MS) ||
         (s_lab.scanning && (uint32_t)(now - s_lab.scan_since) >= SCAN_WATCHDOG_MS))) {
        if (s_lab.unpair_stage != UNPAIR_NONE || s_lab.pending_op == GM_BLE_OP_UNBOND) {
            s_lab.bonds_valid = false;
            cancel_unpair("Unpair timed out. Refresh the paired device list before retrying.");
        } else {
            quarantine("Timed out; client closed, stale lists discarded. Select Retry client.");
        }
    }
    auto_pump();
    render();
}
static void plugin_suspend(void *context)
{
    (void)context;
    /* Background/suspend is not STOP: retain client, session, subscriptions and
     * catalog. A later explicit App HOGP attach may share this physical link. */
    s_lab.suspended = true;
    s_lab.confirm_enabled = false;
    if (s_lab.page == PAGE_CONFIRM) {
        enter_page(s_lab.gatt_view ? PAGE_CHARACTERISTIC : PAGE_MENU);
    }
}
static void plugin_resume(void *context)
{
    (void)context;
    s_lab.suspended = false;
    /* The Host may resume before draining queued BLE events. An overlay alone
     * must not cause this local watchdog to discard a completed operation. */
    uint32_t now = s_lab.host->monotonic_ms();
    if (s_lab.pending_op) {
        s_lab.pending_since = now;
    }
    if (s_lab.scanning) {
        s_lab.scan_since = now;
    }
    s_lab.dirty = true;
}
static void plugin_unload(void *context)
{
    (void)context;
    close_client();
}
gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host, gm_plugin_descriptor_t *plugin)
{
    if (!host || !plugin || host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version, GM_PLUGIN_ABI_MIN_VERSION)) {
        return GM_PLUGIN_EVERSION;
    }
    const gm_plugin_lvgl_api_t *ui = host->graphics.lvgl;
    if (!(host->capabilities & GM_PLUGIN_CAP_BUTTON) || !host->display_get_info ||
        !host->app_exit || !host->monotonic_ms || !host->log || !host->alloc || !host->free ||
        !ui || ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(ui->api_version, GM_PLUGIN_LVGL_API_MIN_VERSION) ||
        !ui->root_get || !ui->obj_clean || !ui->label_create || !ui->label_set_text ||
        !ui->obj_set_pos || !ui->obj_set_size || !ui->label_set_long_mode || !ui->style_set ||
        !ui->obj_add_flag || !ui->obj_clear_flag) {
        return GM_PLUGIN_ENOTSUP;
    }
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_result_t result = gm_plugin_libc_get(host, &libc);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    if (!libc->memcpy || !libc->memmove || !libc->memset || !libc->memcmp || !libc->snprintf) {
        return GM_PLUGIN_ENOTSUP;
    }
    s_lab.host = host;
    s_lab.ui = ui;
    s_lab.libc = libc;
    result = gm_plugin_ble_get(host, &s_lab.ble);
    if (result != GM_PLUGIN_OK && result != GM_PLUGIN_ENOTSUP) {
        return result;
    }
    result = gm_plugin_ble_refresh_get(host, &s_lab.refresh);
    if (result != GM_PLUGIN_OK && result != GM_PLUGIN_ENOTSUP) {
        return result;
    }
    result = gm_plugin_ble_bond_list_get(host, &s_lab.bond_list);
    if (result != GM_PLUGIN_OK && result != GM_PLUGIN_ENOTSUP) {
        return result;
    }
    s_call_ui.host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &s_lab;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_suspend = plugin_suspend;
    plugin->on_resume = plugin_resume;
    plugin->on_stop = plugin_stop;
    plugin->on_unload = plugin_unload;
    return GM_PLUGIN_OK;
}
