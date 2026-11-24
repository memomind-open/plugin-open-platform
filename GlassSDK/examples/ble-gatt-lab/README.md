# BLE GATT Lab

A **glasses-only** native test application for the complete optional BLE GATT
extension (ID `6`). Import **BLE GATT Lab**. Package ID:
`com.memomind.demo.ble-gatt-lab`, version `1`.
No PhoneSDK companion, new phone command, firmware-private header, or vendor
health-data decoder is used. The official App is still used to install/start
the GMP. Clear/disable the phone plugin component in Studio.

Use glasses firmware implementing the full current ID `6` table, including
HOGP key-snapshot coexistence protection and opt-in raw attribute-page discovery.

The numbered notes below describe internal development builds, not installable
package versions. The current package is V1.

Version 5 suppresses duplicate scan-event logs: only new peers, name updates,
and a scan end/selection summary are logged. Full MAC/address type identify peers;
catalog discovery logs one summary instead of one line per metadata record.
Version 6 keeps named scan peers before unnamed peers. Newly named peers join
the end of the named group; existing named peers keep their relative order.
Name-less updates retain an already known name. Deduplication still uses the
full address plus address type. Version 7 keeps the browsing cursor on the same
row during scan updates, so new names cannot keep pushing it down. A previously
saved action target still follows its device when rows move; selecting a live
row acts on the device currently shown there.
Version 8 uses firmware's same-session full metadata cache. **Full GATT (reuse
session cache)** replays a successful snapshot; **Force refresh full GATT** in
Advanced requests real discovery through optional ID `7`. Both wait for the
same async `DISCOVER_ALL` result. Unsupported refresh leaves the current catalog
intact. An attempted refresh failure discards its staged result, not a fake old
success. Disconnect/client close invalidates the firmware snapshot; values
remain remote reads. No phone companion is required.
Version 9 explains generic GATT versus shared system HOGP in connection/session
results. Radio-busy results keep the client open and do not imply an App-first
requirement. This demo never enables HOGP itself, even if it discovers 0x1812.
Version 10 reduces the basic menu to seven entries, merges scan/stop and shows
read/subscription/error/notification counters under Results. The complete manual
toolbox is retained under Advanced; hold Back from its menu to return to basic.
Version 11 replaces the basic Advanced entry with **GATT services**: service
headers and characteristic rows lead directly to Read, Write HEX, Subscribe,
Unsubscribe and Latest data. Actions follow the characteristic properties and
existing Host service policy. **More tools / diagnostics** retains the complete
manual toolbox, raw descriptors and force refresh. No firmware, BCore, SDK ABI
or phone App change is required for this UI update.
Version 12 explains an empty Unpair list: **No plugin bonds to unpair. System
HOGP: unpair in App.** An empty successful query is not a completed unpair,
and selecting the empty page retains the explanation without sending a request.
Version 13 refreshes or clears status on every page entry/back transition.
Late results still update data, but cannot leave the previous visit's message
on another page or unexpectedly open a value view. Automatic progress belongs
to the main menu; characteristic results remain on their own characteristic.
More tools is reduced from 28 entries to 14 extra diagnostics/mode controls;
ordinary scan/stop, disconnect/unpair, read/Notify/write-request and value
actions stay in the basic or characteristic pages instead of appearing twice.
Version 14 separates the bounded owned-bond list from device browsing. Empty,
failed or successful bond queries no longer replace scan/connected-peer rows.
The existing last-successful peer snapshot is also offered when absent from
the current list, allowing an explicit reconnect without another scan in the
same client lifetime. Accepted connections focus the read-only Results entry
(session state in manual Advanced mode), never Disconnect. Finishing automatic
checks does not move the user's cursor.
Version 15 adds `[R/W/N/I]` capability letters to each characteristic row and
exposes explicit Notify/Indicate operations for known services, including HID
and BAS. Updated firmware permits these subscriptions on independent GATT links
but still rejects system-service/reserved CCCD changes on a shared HOGP link.
The Host checks live ownership on every request, so a later App takeover cannot
be bypassed by an old catalog. System-service value writes remain blocked and
the automatic checks still skip system-service subscriptions.
Version 16 bounds scanning to the Scan & Connect/device-list page. Leaving it
requests one stop and retains the discovered rows. Unpair stops any active scan
and loads the owned-bond list after the matching `SCAN_ENDED`; leaving Unpair
before that completion cancels the deferred list query. Neither action closes
the client, disconnects GATT, or deletes a bond automatically. At that revision
Devices was labeled as recent peers, not a live connected-device inventory.
Version 17 makes basic **Devices** query the glasses' saved plugin-owned GATT
bonds on every entry, sharing Unpair's existing bounded list storage. It works
after plugin stop/replacement or reload, without keeping scan history in plugin
RAM or adding plugin-side persistence. Rows become selectable only after the
terminal query succeeds; selecting a row connects with automatic checks, never
unpairs. An active scan is stopped before the query, and leaving the page cancels
the deferred query. Advanced still exposes recent scan peers/connected snapshots.
Version 18 lets **Devices** and **Unpair** use optional ID `8` to display
paired accessories, including the current **App-bound HOGP** row. The row
can reconnect GATT in Devices; selecting it in Unpair directs the user to
**App > Third-party Devices**, without a confirmation or an unbond request.
Older firmware without ID `8` keeps the plugin-owned list.
Version 19 distinguishes an existing plugin GATT session, insufficient LE
slots, and temporary radio busy without disturbing the active connection.
Version 20 trims More tools to eight entries. Connect without auto checks
uses one scan-and-select flow; targeted discovery is submitted from its UUID
editor. Notify/Indicate and both write modes are selected on the actual
characteristic. Retry client appears only when the client is unavailable.
Version 21 reuses the completed full catalog for automatic checks. It chooses
one service instance by handle range and keeps the full catalog available
under GATT services. Explicit UUID discovery and force refresh remain available.
Version 22 keeps a single scan page with an optional Connect only mode and
moves explicit remote refresh to GATT services. More tools keeps only UUID
discovery, raw attributes, and session state. The public SDK and firmware
are unchanged.
Version 23 replaces the manual UUID editor with **Find common services**.
Selecting a preset starts targeted discovery directly; detailed return and
ATT codes remain in the log. SDK UUID support is unchanged.
Version 24 uses lowercase scan log labels: `scan new`, `scan name`,
`scan select`, and `scan end`. The scan behavior and logging frequency are unchanged.
Version 25 clears the forgotten peer's cached scan/saved rows, last-peer
shortcut, and session data. This applies to successful plugin Unpair and to
APP HOGP unpair notifications from matching firmware. Other peers and fresh
advertisements remain available.
Version 26 allocates its 160-record GATT catalog only when discovery results
arrive and keeps the connection usable if full discovery exceeds capacity.
It also supports one confirmation to disconnect and unpair a verified online
plugin bond with matching firmware.
Version 27 keeps the client, connection and subscriptions when matching
firmware confirms notification-only queue loss. It displays a cumulative
notification loss count and logs a rate-limited summary instead of logging
each notification. Unknown or mixed loss still closes the client.
Version 28 processes and counts every delivered notification, while the
characteristic counter and latest HEX preview refresh from notifications at most
once per 200 ms. Button actions, operation results and returning from an
overlay still refresh immediately. No firmware or Host ABI change is needed.
Version 30 shows **Pairing records full. Unpair an accessory first.** when
matching firmware rejects a connection to a peer without a recognized bond.
The firmware checks the quota before starting the link, even for peers that
do not require pairing. The demo does not retry, remove a bond, or disconnect
an existing session. Previously bonded devices can still be selected in **Devices**.
Version 31 labels the latest notification with the actual characteristic value
handle and shows the service and characteristic UUIDs when the session catalog
has them. `Offset 000:` is the byte position in that value, not a report identifier.
Version 32 places the session's cumulative notification loss next to its
received count in **Results** (for example, `RX:123 Loss:5`), not in the footer.
A build or manifest does not prove
that a released firmware/Studio implements it. Unsupported hosts show an
explicit message and allow exit without making radio calls.

## Build and select

From the repository root:

```sh
python3 -m pip install -r GlassSDK/build-host/tools/requirements-review.txt
python3 GlassSDK/build.py build --example ble-gatt-lab
```

On Windows PowerShell, replace `python3` with `py`. The build checks the RV32
ABI and stack/memory limits and creates this matching set:

```text
GlassSDK/build-host/.build/ble-gatt-lab/ble-gatt-lab.gmp
GlassSDK/build-host/.build/ble-gatt-lab/ble-gatt-lab.review.json
GlassSDK/build-host/.build/ble-gatt-lab/ble-gatt-lab.review-source.enc
```

In Desktop Studio, import the repository workspace, refresh, select **BLE GATT
Lab** for the glasses, and **disable the phone component**. Export/install the
glasses-only selection through the App's Developer Workbench. Reimport the
newly built V1 package to test an update; reopening an existing entry may
still run the old binary. If the plugin is not
listed, import the exact GMP directly with its matching source manifest
available. The expected source is `GlassSDK/examples/ble-gatt-lab/manifest.json`.
Workspace discovery and installation must still be checked on your Studio/App
machine; no automatic paired-component matching is required for this app.

Current Desktop Studio ID `6` support is not claimed. Use updated physical
glasses for BLE validation; a simulator's unsupported message is not proof
that the glasses firmware lacks the extension.

## Controls

The default **basic menu** has seven entries:

1. **Scan & Connect** opens the list and starts scanning; leaving that page
   automatically requests **Stop scan**. On entry, the first device (or the
   waiting row) has focus. One primary single click moves to the mode row;
   double-click it to toggle **Mode: Auto checks** and **Mode: Connect only**.
   Each new scan starts in **Auto checks**. Repeated Back/Stop does not
   duplicate an accepted stop. The local scanning flag clears only on `SCAN_ENDED`, not
   on request acceptance; the menu may briefly show **Stop scan** while draining.
   A rejected stop remains visible and can be retried through Stop scan/Unpair;
   the demo never disconnects an established GATT session just to stop a scan.
   If the local client
   is unavailable, this same row becomes **Retry client** (one explicit retry).
2. **Devices** queries up to four plugin-owned GATT bonds plus the current
   App-bound HOGP accessory when ID `8` is available. It shows rows only
   after a successful terminal result.
   Select a saved stable identity to connect directly and run automatic checks,
   without scanning or an unpair confirmation. Stopping/replacing/reimporting the plugin clears its
   RAM, not the glasses' saved bonds; the next visit queries them again.
   This is not a live online inventory or a per-package history. Phone and
   unknown legacy bonds are excluded. An empty successful list says to use
   Scan & Connect; a failed/partial query does not become an
   empty success. Scanning is stopped first; Back cancels any deferred load.
   No automatic reconnect or new plugin/firmware storage is added.
3. **Disconnect** detaches the current plugin session; shared HOGP stays up.
4. **Unpair** shows the same list as Devices. Confirm once to remove an offline
   plugin bond or to disconnect and then remove a verified online plugin bond.
   For an online bond the Host must verify the selected stable identity against
   the current encrypted session, and the demo waits for that session's
   disconnect event before sending unbond. A rotating advertising address is
   not used as proof. If the identity cannot be verified (including on older
   firmware), disconnect manually and retry. Unpairing an offline peer leaves
   any other live connection alone. An App-bound HOGP row still directs the user
   to the App.
   After successful unpair, the peer is no longer offered as a retained
   `[last]` connection shortcut. If the peer advertises again, it can appear
   as a new discovery.
   If a scan is active or stopping, it requests/waits for stop and then loads
   the list automatically. Leaving Unpair before scan completion cancels that
   deferred load, so no late event reopens the page or starts an unexpected query.
   An empty list explains that system HOGP must be unpaired in the official App.
5. **Results** shows the latest HEX value and counters for completed reads (R),
   subscriptions (S), errors (E), received notifications (RX) and notification
   drops (Loss). For a notification,
   the title gives its value handle and the status gives the service and
   characteristic UUIDs, or `unknown` if the current catalog lacks them.
   Identical characteristic UUIDs remain distinguishable by value handle.
   This page does not infer HID Report IDs or retain a history of payloads.
   Zero errors alone does not prove every step ran; the automatic flow's terminal
   log and counters remain the acceptance evidence.
6. **GATT services** shows services with their characteristics. Select a
   characteristic to operate it in place. Capability letters mean Read (`R`),
   Write Request/Command (`W`), Notify (`N`) and Indicate (`I`); they describe
   the peripheral, not a grant of Host permissions. Opening the page uses the
   current full catalog or the Host's session cache when available.
   **Refresh from device** explicitly rediscovers the remote catalog through
   optional ID `7`; without it, the demo reports that refresh is unsupported
   and preserves the existing catalog. **More tools / diagnostics** opens the
   remaining diagnostics.
7. **Exit** ends the plugin cycle.

Double-click a discovered device to **connect immediately**, even while scanning.
There is no connection confirmation page and no need to wait for scan completion
or stop scanning manually. The Host stops the scan when it proceeds with the
connection. The Demo hides late scan rows but waits for `SCAN_ENDED` before
allowing another scan. If connection submission is rejected, the existing scan
remains usable; if a submitted connection is rejected asynchronously before
scanning stops, the Demo requests a stop without hiding the rejection reason.
The connected device runs the automatic checks below in **Auto checks** mode.
In **Connect only** mode it establishes the session without starting automatic
pairing, discovery, reads, or subscriptions; manual GATT actions are still
available. Selecting a **Devices** row always uses automatic checks. A row in
**Unpair** opens a confirmation, defaulting to **CANCEL**.
The main-menu cursor starts on **Results** after accepted connection submission;
an immediate repeated select therefore opens a read-only page. **Disconnect**
remains available only by deliberately selecting its own row. The automatic
workflow no longer repositions the cursor when it finishes.

**More tools / diagnostics** has four entries:

- Find common services.
- Raw GATT attributes.
- Get session state.
- Back to main menu.

Both subscription modes and both write modes appear on the characteristic
page when the peripheral supports them, including when selecting a
characteristic from Raw GATT attributes. No public API is removed.
Hold Back from the Advanced menu, or select **Back to main menu**, to return to
basic controls. Back from a detail page first returns to the relevant parent.
All code still runs only on the glasses; there is no
phone companion. The basic header omits client/session/attribute numbers; detailed
diagnostics remain in Advanced and logs. Rejected requests stay visible.

| Input | Action |
| --- | --- |
| Glasses primary **single** click | Move to next row; scroll the last-value view |
| Glasses primary **double** click | Select the current row |
| Glasses primary **long** press | Back to the main menu; cancel confirmation |
| UP/DOWN, SCROLL_UP/DOWN, PAGE_UP/DOWN navigation triggers | Previous/next row or value offset |
| RIGHT navigation trigger | Select |
| LEFT/BACK navigation trigger | Back/cancel |
| HOME navigation trigger, or the menu's Exit | Exit |

Release events and unsupported button/action combinations do nothing. The
primary-button controls allow testing without an already-connected ring or
mouse. The menu scrolls; display geometry is queried from the Host rather than
assuming a fixed resolution. Logs are prefixed `[ble_lab]` and include event,
operation, request/session, return code, ATT error, handle and payload length.

Opening the application only opens the local client and reads capabilities.
Scanning and choosing a device are user actions. Selecting a device in
**Auto checks** mode or from **Devices** starts the following workflow once:

```text
Connect/attach -> Pair/restore encryption -> Confirm actual session encryption
  -> Full GATT catalog -> Select one eligible service instance from the catalog
  -> Read its readable characteristics -> Subscribe its allowed Notify/Indicate values
  -> Unsubscribe/re-enable the first confirmed subscription once
  -> Session-state query -> Owned-bond list -> Monitor incoming values
```

The target service comes from the successfully discovered catalog, not a guessed
health UUID: prefer a non-system service with Notify/Indicate, then a readable
service. Only that service instance's characteristics are read/subscribed.
Services with the same UUID but another handle range are not included. There is
no second service discovery. Each remote step waits for its real async completion;
accepted submission is not completion.
Pair success alone is not treated as encrypted: query at most ten snapshots,
at least 200 ms apart, before proceeding. This uses `on_loop`, not an OS timer.

The Host can restore a bond or initiate SMP through the existing `pair` API.
No PIN/passkey UI is added. To use a peripheral without requesting pairing
from the demo, choose **Mode: Connect only** in **Scan & Connect**. The
automatic path stops if security cannot be confirmed. Existing HOGP attachments
use the Host's shared-link/security policy.

Properties and the Host's subscription limit are respected. HID (`0x1812`),
Generic Attribute (`0x1801`) and BAS (`0x180F`) are never automatically subscribed.
Unknown owning services are skipped conservatively. A service with no allowed
Notify/Indicate does not fabricate a subscription result. Live bytes appear under
**Last value / notifications**. Summary counters and `[ble_lab] AUTO` logs show
the outcome; a completed flow with nonzero errors is **not** an all-pass result.

Manual Notify/Indicate is available from a known characteristic's page even for
these system services. On updated firmware an independent GATT link may subscribe;
on a shared HOGP link the Host returns `EPERM` without changing the protected
CCCD. An older Host may still deny independent system-service subscriptions.
The result remains on that characteristic page; accepted submission alone never
marks it subscribed. No cached shared flag or extra state-query loop is used.

**No characteristic value is written automatically**, including writable vendor
controls. Select the desired write mode on a writable characteristic to enter
known test bytes and confirm the write. Only an automatic connection starts
the documented pairing and CCCD checks without another confirmation.
There is no automatic unpair, reconnect, write replay or repeated test cycle.
Disconnect can interrupt an in-flight automatic operation; stale completions
cannot restart it. Suspend/call UI pauses new automatic requests while queued
events are still handled, without rendering over the system interface.
Suspend retains the client, connection, catalog and subscriptions; only actual
stop/close performs cleanup. A phone page that explicitly sends STOP still stops
the plugin. Background operation does not require changing the App's lifecycle.

## HOGP coexistence checks (scheme 2)

There is no required connection order. Test both directions on updated glasses:

1. Connect HOGP A from the App, then scan/connect A in this demo: one shared link.
2. Connect HOGP A, then connect HID-capable B in this demo: A remains the only
   system input device; B is generic GATT, not a second HOGP binding.
3. Connect A in this demo first, keep the plugin alive/backgrounded, then connect
   A from the App: explicit system takeover reuses the live physical link. Use
   **Get session state** to verify `shared HOGP=1` after setup.
4. Connect plugin B first, then HOGP A: two independent accessory links, plus at
   most one phone/ANCS link. Replacing A requires explicit App unbond; merely
   disconnecting does not release the persistent binding.
5. With HOGP A out of range, plugin scan/connect must either proceed after a real
   background-initiator cancellation or report busy within its bounded wait.
   Returning A must still restore its system HOGP connection; B must not auto-reconnect
   after physical loss without a new plugin `connect` request.

Do not identify ownership by the presence of 0x1812 or successful SMP alone. Pairing
keys can persist for B without adding B to system HOGP boot/whitelist reconnect.
Test notification/input coexistence with iOS ANCS and an Android phone; native
tests do not certify radio coexistence or a third-party peripheral's behavior.

## First physical-device walkthrough

1. Run **Scan & Connect** (30 seconds). As soon as the target appears, double-click
   its row to connect; do not wait for `SCAN_ENDED`. HOGP can preempt scanning,
   and its result code is displayed.
   Firmware streams new peers throughout the window: its small rotating cache
   is not a device-count quota and never stops scanning merely because the
   environment contains more than 12 devices. Stop scan, timeout, close/error,
   or HOGP preemption ends the scan. Extreme delivery backpressure can coalesce
   reports; `SCAN_ENDED/ENOMEM` then warns about partial delivery, not an early
   capacity stop. The client and coherent retained rows remain usable.
   The demo's list grows dynamically from 16 entries, with a **32 KiB list
   storage budget**, not a 12-device cap. Growth can temporarily hold old/new
   allocations together (at most 64 KiB combined). If memory/budget is exhausted,
   a rolling window admits later peers and displays a partial-list warning.
   Existing names survive unnamed repeat advertisements. Plugin stop/reopen/unload
   releases the list. Finite radio/delivery/memory resources still do not
   guarantee a complete inventory of all surrounding devices.
   With **Auto checks**, selecting the target once starts the rest of the flow.
   Select the mode row just below the first device to switch to **Connect only**.
   Select **Disconnect / detach** to cancel or disconnect. A supported Host
   also reports the current HOGP peer in the scan list even if it is no longer
   advertising.
2. Watch the automatic stage/summary and notification counter. Do not manually
   start other remote operations during this flow. It never automatically
   disconnects/reconnects to retry. A failure remains visible for diagnosis.
   The steps below describe manual inspection; they are also available after
   connecting in **Connect only** mode.
3. Open **GATT services**. An already loaded full catalog (including one kept
   after automatic checks) is reused locally without another API call. If the
   catalog was replaced by manual targeted discovery, opening this page requests
   the Host's complete same-session catalog. Wait for the final successful
   `RESULT`; partial/failed or targeted
   lists are not presented as the complete service list. Select a **Char** row
   under its **Service** header. Its page shows UUID, value handle and
   `R/W/N/I` properties. Descriptors and raw attributes remain available through
   **More tools / diagnostics -> Raw GATT attributes**. To discover just one
   known service, use **Find common services** in that toolbox.
   Use **Refresh from device** in
   **GATT services** only when a fresh remote catalog is needed.
4. Select **Read** on the characteristic page. The page first shows waiting,
   then the async result or `rc`/`ATT` failure. Read success shows byte count
   and a short HEX preview; **Latest data / HEX** opens the full latest buffer.
   It copies callback bytes;
   it does not retain a borrowed pointer or assume every value is a battery.
   For example, a BAS `0x2A19` value `2C` means 44% when that service exists;
   GAP `0x2A00` name bytes remain raw UTF-8/HEX, not a firmware-specific codec.
5. For a characteristic advertising Notify (`0x10`) or Indicate (`0x20`), use
   its **Subscribe Notify** or **Subscribe Indicate** item. Both appear when
   supported. Only the async result's
   `subscribed` field updates the confirmed flag, including failure replies.
   **Sub:ON** means confirmed configured, not that data has arrived: the separate
   **Notifications** counter records actual received events. View bytes under
   **Latest data / HEX**. Use **Unsubscribe** to explicitly disable it. HID (`0x1812`),
   Generic Attribute (`0x1801`) and BAS (`0x180F`) value writes are protected.
   Their CCCDs are also protected on a shared HOGP connection; independent
   GATT links can subscribe when the firmware and peripheral support it.
6. To test writes on an appropriate test characteristic, choose
   **Write HEX (confirm)** or **Write HEX (no response)**,
   edit bytes, select **Done**, review the target/session/length/HEX, move from
   default **CANCEL** to **CONFIRM**, then select again. The two modes are
   shown separately when both are supported.
   The selected value and bytes are snapshotted for confirmation. Write Command
   success is explicitly labelled **LOCAL SUBMISSION ONLY**, not remote ACK.
   Write Request success means an ATT response, not a device-specific business
   action confirmation. No value is written merely by entering or leaving the editor.
   A property mismatch or length over capability/MTU-3 is rejected locally;
   firmware permission and ATT errors remain visible.
7. A successful automatic pair result is not proof of confirmed encryption
   or management ownership: choose **Get session state**
   to inspect actual encryption, SDK bond, managed status, shared HOGP, busy,
   subscription count and MTU. No PIN/passkey UI or proprietary authentication
   bypass is added by this application.
8. **Disconnect / detach** waits beyond the local RESULT until the actual
   disconnected/detached `CONNECTION` event. A shared HOGP session is detached;
   the Host leaves system HID connected. The last session remains queryable
   until this client is closed, but old GATT handles are invalidated immediately.
9. **Unpair** removes only explicitly recorded plugin-manager bonds, not phone,
   App-bound HOGP or unknown legacy records. Wait for the successful list
   result and confirm the stable address/type. A verified online plugin
   session detaches before unbond; an offline peer needs no detach. A failed
   verification or detach never submits unbond. Refresh after a successful
   unbond; no global pair-list clear is exposed.
   Pairing keys live in the glasses' SDK SMP bond storage, not the GMP. The Host
   separately persists managed device identities/names (not keys) in its own
   storage. Successful bonding and persistence normally survive plugin
   stop/unload and glasses restart; merely exiting is not an unpair. Future
   plugin connections remain explicit, not automatic reconnects. Ownership is
   manager-wide, not per plugin package. Restoring an old bond or attaching to
   system HOGP does not grant ownership; pairing or ownership-storage failure
   can leave a device absent from this list.

Characteristic operation results and notification counts belong to the currently
loaded catalog. Reloading metadata resets those local diagnostics; **Sub:?**
means unknown, not remotely disabled. Unsubscribe remains available to confirm
the actual state. The demo retains one latest-value buffer, not a history per
characteristic; a different handle's bytes are never displayed as the selected
characteristic's data. Disconnect/reopen discards catalog handles and context.
Disconnect/detach does retain browsing rows and the last successful peer tuple
for an explicit reconnect. Client close/reopen, stop and unload clear these
temporary snapshots; they are not a persistent device history or bond authority.
Hold Back returns from data/editor/confirmation to the characteristic, then to
the service list, then to the basic menu.

## Find common services

On a connected session, open **More tools / diagnostics -> Find common services**
and select a preset:

- **Battery (0x180F)**
- **Device Information (0x180A)**
- **HID (0x1812)**

Selecting a preset starts targeted discovery without fetching the entire
catalog first. On success, select a characteristic from the discovered
service to use its supported actions. A failure does not prove the service
is absent; check the `[ble_lab]` log for request and ATT details.
The presets apply only to this demo. Plugins may still call
`discover_service()` with an arbitrary 16- or 128-bit service UUID through
the public SDK.

## Write HEX editor

Editing changes **only local configuration**. In a characteristic's Write HEX
flow, **Done** opens explicit confirmation; only **CONFIRM** sends the write.
Hold Back to leave the editor without sending a request.
Editor rows let you move the nibble cursor forward/back, increment/decrement
the digit, or change the length. The status shows the selected nibble offset
and the containing 16-byte HEX window.

- Write payload: 1..64 bytes; newly appended bytes are zeroed. MTU-3 can be a
  smaller limit. No long-write/Prepare/Execute transaction is implemented.
- The confirmation displays the byte count and first 16 bytes (explicitly marked
  for longer values); inspect all editor windows before confirming a long value.

## Failures, lifecycle and coexistence

The application serializes remote requests to make correlation observable.
Automatic security/discovery or transport/session failures stop the workflow;
explicit characteristic ATT/permission failures are counted without blind retry.
Manual discovery and write editing are unavailable while it runs.
Existing confirmed subscriptions remain until explicit
disconnect/cleanup; finishing the checks leaves the link monitoring values.
Synchronous rejection is not an async success. Partial/overflowed/error catalogs
and bond lists cannot be used. The demo allocates its bounded 160-record catalog
(7,040 bytes on RV32, excluding allocator overhead) on the first metadata
callback, not at plugin start. It frees the buffer on invalidation, failure,
detachment or close. A catalog capacity failure is reported separately when the
Host distinguishes it, leaves the connection usable, and suggests **Find common
services**. A data-loss report, 25-second request watchdog,
or **35-second scan-only watchdog**
closes **this plugin client**, invalidates staged/old handles, and requires an
explicit **Retry client**. It never auto-reconnects or replays a
write. An overlay's suspended time does not alone expire the local watchdog;
firmware remote-operation timeouts still apply.
An unpair timeout instead cancels the local sequence and asks the user to
refresh the paired list; a late disconnect cannot start an unbond.

Stop/unload revokes BLE callbacks before releasing display state. Failed start
cleans up its partial UI/subscription. Suspend cancels pending confirmations;
resume does not replay them. Phone call UI hides the LVGL root while BLE
callbacks/state processing remain cooperative; restoration redraws later.
On old hosts without call UI notifications, that coordination is unavailable.

The app does not alter HOGP, ANCS, connection budgets or snapshot ownership.
Those are Host responsibilities. Shared HID input and normal phone notifications
must be checked alongside plugin GATT on actual hardware. Explicit plugin
close/timeout can disconnect its **standalone** peripheral; closing a shared
attachment must leave system HOGP intact. CCCD cleanup is best-effort and can
remain configured remotely until disconnect. Ownership is manager-wide, not a
per-package security sandbox. Only run on trusted test peripherals.

## Verification and where to change it

```sh
cd GlassSDK
python3 -m unittest discover -s tests -p test_ble_gatt_lab.py -v
python3 -m unittest discover -s tests -p test_system_native.py -v
```

`test_ble_gatt_lab.py` executes the **real plugin** against a deterministic Host:
API actions, primary/navigation input, confirmation/cancel, accepted versus
async completion, stale/new-session replies, borrowed payload copying, completed
catalogs, scans past 300 devices, late-target selection, memory-pressure rolling
lists, independent 30/35-second scan timing, live scan selection without a
connection confirmation, rejected submissions retaining the scan, ignored
scan-tail events, subscribe results, scan connection modes, automatic pairing,
service/characteristic navigation, property/protected-service
action filtering, read/write results in place, wrong-handle/session/request
replies, failed-unsubscribe state, no-response write wording, metadata replay,
zero-length reads, target-specific latest data, MTU checks and cancelled writes, actual
encryption gating, reuse of the full catalog for one service instance, automatic read and
Notify/Indicate/unsubscribe, cancellation and failure, disconnect, loss/timeout,
long overlay, unsupported
hosts, failed UI allocation and stop/restart are covered. The lab tests
also cover Unpair/Devices and all editor/catalog/menu entry/back transitions,
late bond/state/scan/read completions, per-characteristic result retention and
the trimmed diagnostic-menu mapping. They
also cover read-only connection focus, repeated select without disconnect,
empty/nonempty/failed bond-query isolation, explicit same-peer reconnect without
scan, and last-peer selection remaining stable as new scan rows arrive. System-call UI tests
also discover and execute this new example, without skipping it. Set `CFLAGS`
to add native sanitizers. These are not real BLE/device or Studio GUI results.

`ble-gatt-lab.c` keeps application state in `s_lab`, remote actions in
`run_action`, SDK event handling in `on_ble`, and UI/navigation in `render` and
`plugin_event`. `auto_pump`/`auto_completed` implement the one-shot automatic
workflow. No firmware or shared SDK ABI changes are needed to edit the lab.

Physical acceptance is still pending: scan/connect both an independent peripheral
and an already-connected HOGP peer; verify full/targeted discovery, read/write,
notification/indication, failure codes, plugin-first/HOGP-first takeover, phone
ANCS alongside HID, owned offline unbond, stop/reopen, and power-loss recovery.
