# Foreground location

Location comes from the **phone**, not a glasses GPS or a Bluetooth message.
Declare `{ "name": "location.foreground", "required": true }` in the current
manifest permission list. Both the App's plugin authorization and the phone's
system location permission are required. Location service must be enabled.

The App source supports Android, iOS and OHOS; availability still depends on the
host build and runtime. Check `gm.runtime.getCapabilities()` rather than assuming
that every simulator or browser provides a native position source.

## Methods

| Method | Parameters | Result |
| --- | --- | --- |
| `gm.location.getCurrentPosition(options?)` | Only `timeoutMs`, integer 1–60000; default 15000 | One `PluginPosition` |
| `gm.location.watchPosition(options?)` | Same options | `{ watchId: string }` after subscription preparation |
| `gm.location.clearWatch(watchId)` | Watch identifier returned by this runtime | `{ released: true }` |
| `gm.location.onPosition(listener)` | Event callback | Unsubscribe function |
| `gm.location.onError(listener)` | Event callback | Unsubscribe function |

Only one location operation is active per runtime: a second one-shot/watch
request returns `BUSY`. Unknown options, explicit null timeout, fractional or
out-of-range timeout are rejected. `clearWatch` is idempotent for an inactive or
nonmatching string ID; it does not cancel another runtime's watch.

## Position and events

| Field | Meaning |
| --- | --- |
| `latitude` | Finite number, -90 through 90 degrees |
| `longitude` | Finite number, -180 through 180 degrees |
| `accuracy` | Nonnegative accuracy in metres |
| `timestamp` | Unix epoch milliseconds |
| `coordinateSystem` | `WGS84` |
| `precision` | `precise`, `reduced`, or `unknown`; do not assume precise permission |

`location.position` contains `{ watchId, ...position }`.
`location.error` contains `{ watchId, error }`, where `error` is the Bridge error
object. A watch ID identifies the subscription; it is not a location timestamp.

```js
let watchId;
const offPosition = gm.location.onPosition(position => {
  // Subscribe first: a native event can arrive before watchPosition resolves.
  console.log(position.latitude, position.longitude, position.precision);
});
const offError = gm.location.onError(({ watchId, error }) => {
  console.error(watchId, error);
});
({ watchId } = await gm.location.watchPosition({ timeoutMs: 15000 }));
// When leaving the feature:
// await gm.location.clearWatch(watchId);
// offPosition(); offError();
```

One-shot calls ignore positions timestamped before the request or in the future.
Watch delivery is limited to at most one position per second. `timeoutMs` is an
initial/no-valid-position deadline, not the report interval: accepted positions
refresh it. A timeout ends the watch rather than generating an endless stream
of repeated errors.

## Foreground and cleanup

The plugin page must be visible and the App resumed. Hiding/suspending the page,
backgrounding the App, closing the runtime or invalidating authorization cancels
location work and releases the subscription. Returning to the foreground does
not silently restart an old watch; explicitly start a new one if still needed.
Do not depend on receiving a final error event after the runtime loses permission
or visibility: events are filtered by the current lifecycle and authorization.

## Errors

| Code | Meaning/action |
| --- | --- |
| `INVALID_REQUEST` | Fix the options or watch ID type |
| `BUSY` | Finish/clear the existing operation first |
| `PERMISSION_DENIED` | Plugin permission missing, revoked, or page not foreground |
| `SYSTEM_PERMISSION_DENIED` | Phone OS denied location; respect the user's choice |
| `LOCATION_SERVICE_DISABLED` | Phone location service is off |
| `CAPABILITY_UNAVAILABLE` | Platform/source unavailable or prior cleanup failed |
| `TIMEOUT` | No usable position within the deadline |
| `POSITION_UNAVAILABLE` | Invalid position, ended source stream or source failure |
| `RUNTIME_CLOSED` | Operation cancelled by runtime closure/replacement |

Session-token and stale-runtime checks also apply. Do not relabel every failure
as a Bluetooth timeout. See [runtime and lifecycle](runtime-and-lifecycle.md).
