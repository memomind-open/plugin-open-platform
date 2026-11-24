#ifndef GM_PLUGIN_BLE_GATT_H
#define GM_PLUGIN_BLE_GATT_H

#include "gm_plugin.h"

/* Local plugin ABI, NOT phone Bluetooth service/command IDs. All buffers passed
 * into a request are copied before it returns. Callback values are borrowed and
 * valid only for that callback; callbacks run on the display task. */
#define GM_BLE_NAME_MAX            31U
#define GM_BLE_WRITE_MAX           64U
#define GM_BLE_VALUE_MAX           512U
#define GM_BLE_SUBSCRIPTION_MAX    8U
#define GM_BLE_GATT_MAX_ATTRIBUTES 160U
#define GM_BLE_BOND_MAX            4U
#define GM_BLE_BONDED_ACCESSORY_MAX (GM_BLE_BOND_MAX + 1U)

typedef struct {
    uint8_t length; /* 2 or 16; bytes use Bluetooth little-endian UUID order. */
    uint8_t bytes[16];
} gm_ble_uuid_t;

typedef struct {
    uint8_t address[6];   /* Bluetooth little-endian address order. */
    uint8_t address_type; /* Preserve the scan result's address type. */
    int8_t rssi;
    uint8_t connected;
    uint8_t hid_ready;
    char name[GM_BLE_NAME_MAX + 1U];
} gm_ble_device_t;

typedef struct {
    uint32_t duration_ms;                   /* 0 = 10 seconds; maximum 30 seconds. */
    gm_ble_uuid_t service;                  /* length=0 means no UUID filter. */
    char name_prefix[GM_BLE_NAME_MAX + 1U]; /* optional; AND with service */
    int8_t minimum_rssi;                    /* 0 = SDK default */
} gm_ble_scan_filter_t;

enum {
    GM_BLE_EVENT_SCAN_RESULT = 1,
    GM_BLE_EVENT_SCAN_ENDED,
    GM_BLE_EVENT_CONNECTED_DEVICE,
    GM_BLE_EVENT_CONNECTION,
    GM_BLE_EVENT_SERVICE,
    GM_BLE_EVENT_CHARACTERISTIC,
    GM_BLE_EVENT_RESULT,
    GM_BLE_EVENT_NOTIFICATION,
    GM_BLE_EVENT_DATA_LOSS,
    GM_BLE_EVENT_CONNECTED_DEVICES_DONE,
    GM_BLE_EVENT_DESCRIPTOR,
    GM_BLE_EVENT_ATTRIBUTE,
    GM_BLE_EVENT_SESSION_STATE,
    GM_BLE_EVENT_BONDED_DEVICE,
};
enum {
    GM_BLE_OP_CONNECT = 1,
    GM_BLE_OP_DISCOVER,
    GM_BLE_OP_READ,
    GM_BLE_OP_WRITE,
    GM_BLE_OP_WRITE_NO_RESPONSE,
    GM_BLE_OP_SUBSCRIBE,
    GM_BLE_OP_UNSUBSCRIBE,
    GM_BLE_OP_PAIR,
    GM_BLE_OP_DISCONNECT,
    GM_BLE_OP_DISCOVER_ALL,
    GM_BLE_OP_UNBOND,
    GM_BLE_OP_GET_SESSION_STATE,
    GM_BLE_OP_GET_BONDED_DEVICES,
};

enum {
    GM_BLE_SESSION_DISCONNECTED,
    GM_BLE_SESSION_CONNECTING,
    GM_BLE_SESSION_CONNECTED,
    GM_BLE_SESSION_DISCONNECTING,
};

/* SESSION_STATE.properties: event.device.address/address_type carry the
 * verified stable identity (type 2/3) of a live managed standalone bond. */
enum {
    GM_BLE_SESSION_STABLE_IDENTITY = 1,
};

/* SESSION_STATE.data is this borrowed struct; copy with memcpy before use.
 * The containing event supplies session_id, request_id, device and current MTU.
 * Without GM_BLE_SESSION_STABLE_IDENTITY, the reported address may be the
 * current or last on-air address and is not proof of a stable bond identity. */
typedef struct {
    uint16_t struct_size;
    uint8_t state;
    uint8_t encrypted;
    uint8_t bonded;
    uint8_t managed;
    uint8_t shared_hogp;
    uint8_t busy;
    uint8_t subscriptions;
} gm_ble_session_state_t;

/* SERVICE.properties for discover_all(); characteristic properties stay ATT bits. */
#define GM_BLE_SERVICE_PRIMARY   1U
#define GM_BLE_SERVICE_SECONDARY 2U

/* BONDED_DEVICE.properties for the optional bonded accessory list (ID 8). */
#define GM_BLE_BOND_SYSTEM_HOGP 1U

/* RESULT(CONNECT).properties on EBUSY; older firmware leaves it zero. */
#define GM_BLE_CONNECT_PLUGIN_LIMIT     1U
#define GM_BLE_CONNECT_SLOT_UNAVAILABLE 2U /* No slot assignable to this request. */
#define GM_BLE_CONNECT_BOND_LIMIT       3U /* Plugin bond records are full. */
/* Compatibility alias for existing plugin source. */
#define GM_BLE_CONNECT_LE_SLOTS GM_BLE_CONNECT_SLOT_UNAVAILABLE

/* DATA_LOSS.properties: only notifications in session_id were dropped because
 * the host queue was full. Zero or an unknown value may include control events.
 * request_id remains the drop count; this is not an in-stream boundary. */
#define GM_BLE_DATA_LOSS_NOTIFICATION_ONLY 1U

/* RESULT(DISCOVER_ALL).properties on GM_PLUGIN_ENOMEM only. */
enum {
    GM_BLE_DISCOVERY_CATALOG_LIMIT = 1,
};

typedef struct {
    uint16_t struct_size;
    uint16_t type;
    uint32_t session_id; /* Changes on every attach/connect; stale IDs reject. */
    uint32_t request_id;
    uint32_t scan_id;
    gm_plugin_result_t result;
    gm_ble_device_t device;
    gm_ble_uuid_t uuid;
    uint16_t handle;
    uint16_t end_handle;
    uint16_t value_handle;
    uint16_t mtu;
    uint16_t data_length;
    uint8_t operation;
    uint8_t properties;
    uint8_t att_error;
    uint8_t subscribed; /* Confirmed remote config, not request acceptance. */
    const uint8_t *data;
} gm_ble_event_t;

typedef void (*gm_ble_callback_t)(void *context, const gm_ble_event_t *event);

typedef struct {
    uint16_t struct_size;
    uint16_t max_write;
    uint16_t max_read;
    uint8_t max_subscriptions;
    uint8_t max_pending;
    uint8_t max_plugin_connections;
    uint8_t shared_hogp;
} gm_ble_capabilities_t;

/* discover_all() emits SERVICE / CHARACTERISTIC / DESCRIPTOR / ATTRIBUTE
 * metadata in handle order, then RESULT(GM_BLE_OP_DISCOVER_ALL). Commit the
 * staged catalog only when that RESULT succeeds. It does not read value data.
 * SERVICE: handle=start, end_handle=last occupied handle in that service,
 * properties=PRIMARY/SECONDARY. CHARACTERISTIC: handle=declaration,
 * value_handle=value, properties=ATT bits. DESCRIPTOR: handle=descriptor,
 * value_handle=owning characteristic's value. ATTRIBUTE covers other attributes
 * (e.g. Include declarations), with their type UUID/handle, not decoded values.
 * UUIDs preserve 16/128-bit form. Limits: 160 raw attributes (including value
 * handles), 16 services and 48 characteristics. Overflow returns
 * GM_PLUGIN_ENOMEM with properties=GM_BLE_DISCOVERY_CATALOG_LIMIT; it does not
 * disconnect the link or return a successful partial catalog. Zero or an
 * unknown properties value does not distinguish allocation failure from a
 * catalog limit on older firmware. Collection timeout is 15s; publishing uses
 * bounded backpressure after releasing shared ATT. Discovery is explicit, not automatic
 * at connect. Successful full metadata is cached for this client/session only.
 * Repeating discover_all replays the full snapshot without remote discovery;
 * targeted discovery does not replace it. No characteristic values are cached.
 * Disconnect, close, database invalidation and power-off release the snapshot.
 * Optional ID 7 (gm_plugin_ble_refresh_get) exposes refresh_all with the same
 * client/session/request arguments and DISCOVER_ALL events. It discards the old
 * snapshot and performs real discovery; failure never reports the old table as
 * success. ID 6's table/slots are unchanged. Without ID 7, explicit refresh is
 * unsupported; do not probe beyond ID 6's table. A generic dynamic database
 * without automatic invalidation requires an explicit refresh by its developer;
 * this API does not install new Service Changed subscriptions.
 * The pre-release extension appends discover_all after pair; all
 * previously defined slots, event numbers and operation numbers are retained.
 */

/* v1: one plugin client and one plugin connection, or an attachment to the
 * existing HOGP connection, in either connection order. Plugin connect is
 * generic GATT only: discovering HID or pairing does not enable system HOGP.
 * Only explicit App/HOGP takeover grants that role; one system HOGP binding
 * remains supported. Generic SMP keys do not grant system auto-reconnect.
 * Background/suspend retains the client; actual stop/close performs cleanup.
 * Radio setup is serialized; a background HOGP initiator may yield through its
 * existing cancel completion (plugin waits at most 5s, then reports busy).
 * Foreground HOGP setup/established links are never forcibly disconnected.
 *
 * No generic background auto-reconnect. Discover a service
 * before accessing its characteristics. Generic Attribute (0x1801), HID (0x1812)
 * and BAS (0x180F) characteristic values are read-only. Their Notify/Indicate
 * CCCDs may be configured through subscribe on an independent GATT link. On a
 * shared HOGP link, these services' CCCDs and other system-reserved CCCDs remain
 * protected. The Host checks live ownership for each request, including after
 * App takeover. Other services can be subscribed by value handle.
 * Physical disconnect/explicit HOGP unbond retires a
 * shared session too. Local close never disconnects a system-owned HOGP link.
 *
 * OK from a request means accepted, not remote completion. A RESULT/CONNECTION
 * reports completion. WRITE_NO_RESPONSE only reports local submission. Busy and
 * queue-full are explicit errors; no automatic retries of writes are performed.
 * RESULT(CONNECT)/EBUSY with GM_BLE_CONNECT_BOND_LIMIT in properties means
 * plugin bond records are full. A peer without a recognized existing bond is
 * rejected before a new link, even if it needs no pairing. Unpair a managed
 * accessory before retrying. Known bonds and shared live HOGP remain eligible.
 * Scan results normally match advertisement data, not the complete remote DB.
 * A successful scan also reports a connected HOGP peer matching the known HID
 * UUID/name filters, even without advertising: connected/hid_ready are set,
 * data_length is zero (no fabricated ADV), and RSSI 0 is unavailable, not a
 * fresh measurement. The RSSI threshold applies to real advertising reports.
 * Later same-address identity/base aliases use the live connection's tuple so
 * consumers still deduplicate by the full address and address_type.
 * get_connected_devices also works when a connected peer no longer advertises.
 * Data loss is explicit (DATA_LOSS, count in request_id). Notification-only loss
 * does not revoke the session or subscriptions. Service catalog bounds
 * for discover_service are 4 instances of the requested UUID and 48 characteristics across this
 * connection; overflow fails discovery with ENOMEM, never publishes success.
 * Max pending=4 includes accepted requests whose completion has not been polled.
 * A shared ATT timeout retires the plugin session and uses the existing HOGP
 * quiet window; it does not disconnect HID. Failed close cleanup may leave the
 * remote CCCD enabled until disconnect. No automatic write replay/reconnect.
 * disconnect's RESULT confirms local revocation, not physical link teardown;
 * in-flight requests/CCCD cleanup drain asynchronously. No further notification
 * is delivered after local revocation. The system can explicitly disconnect or
 * unbond a shared peer; plugins must then reconnect/rediscover a fresh session.
 */
/* Function table is in gm_plugin_extensions.h, covered by the extension ABI gate. */

/* Management APIs append after discover_all (pre-release extension ID 6):
 *
 * get_session_state(client, session, request): SESSION_STATE is the terminal
 * success, carrying gm_ble_session_state_t in data. Errors use RESULT. Only the
 * calling client's current/most-recent session is queryable, including after
 * disconnect; a different client's old session is rejected. MTU is 0 offline.
 * This is a main-task snapshot, not an ATT request and not a security trigger.
 *
 * get_bonded_devices(client, request): BONDED_DEVICE entries then terminal
 * RESULT(GET_BONDED_DEVICES). Stage until terminal OK; backpressure returns
 * EBUSY, not a successful partial list. Only fresh standalone-plugin bonds
 * explicitly recorded by this manager are listed (max GM_BLE_BOND_MAX). No
 * phone/global/legacy/HOGP pairing list or keys are exposed. address_type=2/3
 * denotes the stable public/random identity; pass that exact tuple to unbond.
 * Ownership persists across plugin unload/reboot, not a per-package ACL.
 *
 * Optional ID 8 (gm_plugin_ble_bond_list_get) adds get_bonded_accessories
 * using the same BONDED_DEVICE entries and terminal GET_BONDED_DEVICES result.
 * It also reports the current App-bound HOGP peer, if its usable bond exists,
 * with GM_BLE_BOND_SYSTEM_HOGP in properties. A peer occurs only once, and
 * phone or other system bonds are not exposed. The stable identity can be
 * passed to connect, but App-owned HOGP cannot be unbonded by a plugin.
 * The optional list is bounded by GM_BLE_BONDED_ACCESSORY_MAX. ID 6's
 * get_bonded_devices continues listing plugin-owned bonds only.
 *
 * unbond(client, identity, request): offline-only; disconnect and wait for the
 * actual disconnect event first. Unknown/unmanaged/system/HOGP identities are
 * denied, live links return EBUSY. It removes only that SDK LE bond and this
 * manager's ownership entry; no classic deletion, disconnect, or reconnect.
 * RESULT OK confirms SDK RAM deletion and ownership storage; SDK key flash uses
 * its existing asynchronous persistence FIFO (not a power-loss transaction).
 * If ownership persistence fails after SDK deletion, RESULT is EIO; retry is
 * idempotent. Failed ownership save after pairing leaves managed=0: encryption
 * success alone never grants unbond authority. Preexisting bonds are not adopted.
 */

#endif
