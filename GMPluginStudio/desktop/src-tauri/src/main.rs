use base64::{engine::general_purpose::STANDARD as BASE64, Engine as _};
use qrcode::{types::Color, QrCode};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::{HashMap, HashSet};
use std::ffi::{c_char, c_int, c_uchar, c_void, CStr, CString};
use std::fs::{self, File};
use std::io::{self, BufRead, BufReader, Cursor, Read, Write};
use std::net::{Ipv4Addr, SocketAddrV4, TcpListener, TcpStream, UdpSocket};
use std::path::{Path, PathBuf};
use std::ptr::NonNull;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, RwLock};
use std::thread;
use std::time::Duration;
use tiny_http::{Header, Response, Server, StatusCode};

const DISPLAY_WIDTH: usize = 600;
const DISPLAY_HEIGHT: usize = 350;
const MAX_PLUGIN_PAYLOAD_BYTES: usize = 81_901;
const MAX_MMPKG_BYTES: u64 = 10 * 1024 * 1024;
const MAX_MMPKG_EXTRACTED_BYTES: u64 = 30 * 1024 * 1024;
const MAX_MMPKG_FILES: usize = 500;
const MMPKG_SHARE_PORT: u16 = 18_765;
const MMPKG_REQUEST_LINE: &[u8] = b"MMPKG/1 GET\n";
const MAX_GMP_BYTES: u64 = 200 * 1024;
const MIN_GMP_BYTES: u64 = 28;
const GMP_SHARE_PORT: u16 = 18_766;
const GMP_REQUEST_LINE: &[u8] = b"GMP/1 GET\n";

#[link(name = "gmplugin_previewer_core", kind = "static")]
extern "C" {
    fn gm_preview_create() -> *mut c_void;
    fn gm_preview_destroy(handle: *mut c_void);
    fn gm_preview_load(handle: *mut c_void, path: *const c_char) -> c_int;
    fn gm_preview_start(handle: *mut c_void) -> c_int;
    fn gm_preview_stop(handle: *mut c_void) -> c_int;
    fn gm_preview_tick(handle: *mut c_void, elapsed_ms: u32) -> c_int;
    fn gm_preview_send_bluetooth(
        handle: *mut c_void,
        channel: u16,
        payload: *const c_uchar,
        payload_size: usize,
        handled: *mut c_int,
    ) -> c_int;
    fn gm_preview_send_button(
        handle: *mut c_void,
        action: u16,
        button: u16,
        handled: *mut c_int,
    ) -> c_int;
    fn gm_preview_send_gesture(
        handle: *mut c_void,
        gesture: u16,
        active: c_int,
        handled: *mut c_int,
    ) -> c_int;
    fn gm_preview_simulate_direction_gesture(
        handle: *mut c_void,
        gesture: u16,
        handled: *mut c_int,
    ) -> c_int;
    fn gm_preview_is_loaded(handle: *const c_void) -> c_int;
    fn gm_preview_is_running(handle: *const c_void) -> c_int;
    fn gm_preview_frame_size() -> usize;
    fn gm_preview_copy_frame(
        handle: *mut c_void,
        output: *mut c_uchar,
        output_size: usize,
    ) -> c_int;
    fn gm_preview_outbox_count(handle: *const c_void) -> usize;
    fn gm_preview_outbox_channel(handle: *const c_void, index: usize, channel: *mut u16) -> c_int;
    fn gm_preview_outbox_payload_size(handle: *const c_void, index: usize) -> usize;
    fn gm_preview_copy_outbox_payload(
        handle: *mut c_void,
        index: usize,
        output: *mut c_uchar,
        output_size: usize,
    ) -> c_int;
    fn gm_preview_clear_outbox(handle: *mut c_void) -> c_int;
    fn gm_preview_last_error(handle: *const c_void) -> *const c_char;
}

struct NativePreviewer {
    handle: NonNull<c_void>,
    source: Option<PathBuf>,
}

unsafe impl Send for NativePreviewer {}

impl NativePreviewer {
    fn new() -> Result<Self, String> {
        let handle = NonNull::new(unsafe { gm_preview_create() })
            .ok_or_else(|| "could not allocate native previewer".to_string())?;
        Ok(Self {
            handle,
            source: None,
        })
    }

    fn error(&self) -> String {
        let value = unsafe { gm_preview_last_error(self.handle.as_ptr()) };
        if value.is_null() {
            return "native previewer operation failed".to_string();
        }
        unsafe { CStr::from_ptr(value) }
            .to_string_lossy()
            .into_owned()
    }

    fn require(&self, result: c_int) -> Result<(), String> {
        if result != 0 {
            Ok(())
        } else {
            Err(self.error())
        }
    }
}

impl Drop for NativePreviewer {
    fn drop(&mut self) {
        unsafe { gm_preview_destroy(self.handle.as_ptr()) };
    }
}

type PreviewerState = Mutex<NativePreviewer>;

struct WebPluginServer {
    root: Arc<RwLock<Option<PathBuf>>>,
    base_url: String,
}

struct ActivePackageShare {
    stop: Arc<AtomicBool>,
    thread: Option<thread::JoinHandle<()>>,
}

impl ActivePackageShare {
    fn stop(mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

impl Drop for ActivePackageShare {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

#[derive(Default)]
struct PackageShareState {
    active: Mutex<Option<ActivePackageShare>>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct DiscoveredManifest {
    id: String,
    name: String,
    version: String,
    entry: String,
    #[serde(default, deserialize_with = "deserialize_permissions")]
    permissions: Vec<String>,
    #[serde(default)]
    device_requirements: DeviceRequirements,
}

fn deserialize_permissions<'de, D>(deserializer: D) -> Result<Vec<String>, D::Error>
where
    D: serde::Deserializer<'de>,
{
    let values = Vec::<serde_json::Value>::deserialize(deserializer)?;
    values
        .into_iter()
        .map(|value| match value {
            serde_json::Value::String(name) => Ok(name),
            serde_json::Value::Object(object) => object
                .get("name")
                .and_then(serde_json::Value::as_str)
                .map(str::to_string)
                .ok_or_else(|| serde::de::Error::custom("permission object requires a name")),
            _ => Err(serde::de::Error::custom(
                "permission must be a string or named object",
            )),
        })
        .collect()
}

#[derive(Clone, Default, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
struct DeviceRequirements {
    preferred_plugin_id: Option<String>,
    required_plugin_id: Option<String>,
    min_plugin_version: Option<String>,
    #[serde(default)]
    protocols: Vec<ProtocolRequirement>,
}

#[derive(Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
struct ProtocolRequirement {
    id: String,
    min_version: String,
}

#[derive(Clone, Deserialize, Serialize)]
struct ProvidedProtocol {
    id: String,
    version: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct DiscoveredWebPlugin {
    id: String,
    name: String,
    version: String,
    path: String,
    permissions: Vec<String>,
    device_requirements: DeviceRequirements,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct DiscoveredDevicePlugin {
    id: String,
    name: String,
    version: String,
    path: String,
    provides: Vec<ProvidedProtocol>,
    metadata_available: bool,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PackageShareResult {
    package_path: String,
    qr_payload: String,
    host: String,
    port: u16,
    name: String,
    qr_size: usize,
    qr_modules: Vec<bool>,
}

impl WebPluginServer {
    fn new() -> Result<Self, String> {
        let server = Server::http("127.0.0.1:0")
            .map_err(|error| format!("Could not start Web plugin server: {error}"))?;
        let port = server
            .server_addr()
            .to_ip()
            .ok_or_else(|| "Web plugin server did not bind an IP socket".to_string())?
            .port();
        let token = uuid::Uuid::new_v4().simple().to_string();
        let prefix = format!("/{token}/");
        let root = Arc::new(RwLock::new(None));
        let thread_root = Arc::clone(&root);
        thread::Builder::new()
            .name("gm-web-plugin-server".to_string())
            .spawn(move || {
                for request in server.incoming_requests() {
                    serve_web_request(request, &prefix, &thread_root);
                }
            })
            .map_err(|error| format!("Could not start Web plugin server thread: {error}"))?;
        Ok(Self {
            root,
            base_url: format!("http://127.0.0.1:{port}/{token}"),
        })
    }

    fn url_for(&self, root: &Path, entry: &Path) -> Result<String, String> {
        let canonical_root = root
            .canonicalize()
            .map_err(|error| format!("Could not resolve Web plugin root: {error}"))?;
        let relative = entry
            .strip_prefix(&canonical_root)
            .map_err(|_| "Web plugin entry is outside its package root".to_string())?;
        let encoded = relative
            .components()
            .map(|component| {
                urlencoding::encode(&component.as_os_str().to_string_lossy()).into_owned()
            })
            .collect::<Vec<_>>()
            .join("/");
        *self
            .root
            .write()
            .map_err(|_| "Web plugin server lock is poisoned".to_string())? = Some(canonical_root);
        Ok(format!("{}/{encoded}", self.base_url))
    }
}

fn serve_web_request(
    request: tiny_http::Request,
    prefix: &str,
    root_state: &RwLock<Option<PathBuf>>,
) {
    let path = request.url().split('?').next().unwrap_or_default();
    let Some(encoded_relative) = path.strip_prefix(prefix) else {
        let _ =
            request.respond(Response::from_string("Not found").with_status_code(StatusCode(404)));
        return;
    };
    let Ok(decoded) = urlencoding::decode(encoded_relative) else {
        let _ =
            request.respond(Response::from_string("Bad path").with_status_code(StatusCode(400)));
        return;
    };
    let Ok(root_guard) = root_state.read() else {
        let _ =
            request.respond(Response::from_string("Unavailable").with_status_code(StatusCode(503)));
        return;
    };
    let Some(root) = root_guard.as_ref() else {
        let _ = request.respond(
            Response::from_string("No Web plugin selected").with_status_code(StatusCode(404)),
        );
        return;
    };
    let Ok(candidate) = safe_relative_entry(root, &decoded) else {
        let _ =
            request.respond(Response::from_string("Unsafe path").with_status_code(StatusCode(400)));
        return;
    };
    let Ok(canonical) = candidate.canonicalize() else {
        let _ =
            request.respond(Response::from_string("Not found").with_status_code(StatusCode(404)));
        return;
    };
    if !canonical.starts_with(root) || !canonical.is_file() {
        let _ =
            request.respond(Response::from_string("Not found").with_status_code(StatusCode(404)));
        return;
    }
    let Ok(file) = File::open(&canonical) else {
        let _ =
            request.respond(Response::from_string("Not found").with_status_code(StatusCode(404)));
        return;
    };
    let content_type = content_type_for(&canonical);
    let mut response = Response::from_file(file);
    if let Ok(header) = Header::from_bytes("Content-Type", content_type) {
        response.add_header(header);
    }
    if let Ok(header) = Header::from_bytes("Cache-Control", "no-store") {
        response.add_header(header);
    }
    let _ = request.respond(response);
}

fn content_type_for(path: &Path) -> &'static str {
    match path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or_default()
        .to_ascii_lowercase()
        .as_str()
    {
        "html" => "text/html; charset=utf-8",
        "js" | "mjs" => "text/javascript; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "json" => "application/json; charset=utf-8",
        "svg" => "image/svg+xml",
        "png" => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "gif" => "image/gif",
        "webp" => "image/webp",
        "wasm" => "application/wasm",
        "woff" => "font/woff",
        "woff2" => "font/woff2",
        _ => "application/octet-stream",
    }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PreviewStatus {
    loaded: bool,
    running: bool,
    source: Option<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PluginMessageResult {
    sent: bool,
    handled: bool,
    channel: u16,
    payload_bytes: usize,
}

#[derive(Serialize)]
struct SimulatedEventResult {
    handled: bool,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct FrameResult {
    width: usize,
    height: usize,
    gray4_base64: String,
    running: bool,
    messages: Vec<OutboundMessage>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct OutboundMessage {
    channel: u16,
    data_base64: String,
}

#[derive(Deserialize)]
struct WebManifest {
    entry: String,
}

fn sdk_root() -> Result<PathBuf, String> {
    if let Ok(current) = std::env::current_dir() {
        for candidate in current.ancestors() {
            let web_sdk = candidate.join("GMWebPluginSDK");
            if web_sdk.join("tools/build-mmpkg.mjs").is_file() && web_sdk.join("examples").is_dir()
            {
                return web_sdk
                    .canonicalize()
                    .map_err(|error| format!("Could not resolve SDK root: {error}"));
            }
        }
    }
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../../GMWebPluginSDK")
        .canonicalize()
        .map_err(|error| format!("Could not resolve SDK root: {error}"))
}

fn read_discovered_manifest(directory: &Path) -> Result<DiscoveredManifest, String> {
    let manifest_path = directory.join("manifest.json");
    let manifest: DiscoveredManifest = serde_json::from_reader(
        File::open(&manifest_path)
            .map_err(|error| format!("Could not open {}: {error}", manifest_path.display()))?,
    )
    .map_err(|error| format!("Invalid {}: {error}", manifest_path.display()))?;
    validate_manifest_identity(&manifest)
        .map_err(|error| format!("{error}: {}", manifest_path.display()))?;
    let entry = safe_relative_entry(directory, &manifest.entry)?;
    if !entry.is_file() {
        return Err(format!("Web plugin entry not found: {}", entry.display()));
    }
    Ok(manifest)
}

fn discover_web_plugins_in(root: &Path) -> Result<Vec<DiscoveredWebPlugin>, String> {
    let mut directories = Vec::new();
    for plugins_root in [root.join("plugins"), root.join("examples")] {
        if !plugins_root.is_dir() {
            continue;
        }
        let mut entries = fs::read_dir(&plugins_root)
            .map_err(|error| format!("Could not scan {}: {error}", plugins_root.display()))?
            .filter_map(Result::ok)
            .filter(|entry| entry.file_type().is_ok_and(|kind| kind.is_dir()))
            .map(|entry| entry.path())
            .collect::<Vec<_>>();
        entries.sort();
        directories.extend(entries);
    }

    let mut plugins = Vec::new();
    let mut discovered_ids = HashSet::new();
    for project in directories {
        let candidates = [project.clone(), project.join("dist"), project.join("build")];
        let Some((source, manifest)) = candidates.into_iter().find_map(|candidate| {
            read_discovered_manifest(&candidate)
                .ok()
                .map(|m| (candidate, m))
        }) else {
            continue;
        };
        if !discovered_ids.insert(manifest.id.clone()) {
            continue;
        }
        let source = source
            .canonicalize()
            .map_err(|error| format!("Could not resolve Web plugin directory: {error}"))?;
        plugins.push(DiscoveredWebPlugin {
            id: manifest.id,
            name: manifest.name,
            version: manifest.version,
            path: source.to_string_lossy().into_owned(),
            permissions: manifest.permissions,
            device_requirements: manifest.device_requirements,
        });
    }
    plugins.sort_by(|left, right| left.name.cmp(&right.name).then(left.id.cmp(&right.id)));
    Ok(plugins)
}

#[tauri::command]
fn discover_web_plugins() -> Result<Vec<DiscoveredWebPlugin>, String> {
    discover_web_plugins_in(&sdk_root()?)
}

#[tauri::command]
fn inspect_web_plugin(path: String) -> Result<DiscoveredWebPlugin, String> {
    let requested = PathBuf::from(path)
        .canonicalize()
        .map_err(|error| format!("Could not resolve Web plugin: {error}"))?;
    let (source, manifest) = if requested.is_dir() {
        [
            requested.clone(),
            requested.join("dist"),
            requested.join("build"),
        ]
        .into_iter()
        .find_map(|candidate| {
            read_discovered_manifest(&candidate)
                .ok()
                .map(|manifest| (candidate, manifest))
        })
        .ok_or_else(|| "Web plugin workspace does not contain a runnable manifest".to_string())?
    } else {
        checked_mmpkg(&requested)?;
        let file = File::open(&requested)
            .map_err(|error| format!("Could not open Web plugin package: {error}"))?;
        let mut archive = zip::ZipArchive::new(file)
            .map_err(|error| format!("Invalid Web plugin package: {error}"))?;
        let manifest = serde_json::from_reader(
            archive
                .by_name("manifest.json")
                .map_err(|_| "Web plugin package does not contain manifest.json".to_string())?,
        )
        .map_err(|error| format!("Invalid packaged Web manifest: {error}"))?;
        (requested.clone(), manifest)
    };
    Ok(DiscoveredWebPlugin {
        id: manifest.id,
        name: manifest.name,
        version: manifest.version,
        path: source.to_string_lossy().into_owned(),
        permissions: manifest.permissions,
        device_requirements: manifest.device_requirements,
    })
}

fn device_sdk_root() -> Result<PathBuf, String> {
    if let Ok(current) = std::env::current_dir() {
        for candidate in current.ancestors() {
            let device_sdk = candidate.join("GMPluginSDK");
            if device_sdk.join("gm-build").is_file() && device_sdk.join("examples").is_dir() {
                return device_sdk
                    .canonicalize()
                    .map_err(|error| format!("Could not resolve device SDK root: {error}"));
            }
        }
    }
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../../GMPluginSDK")
        .canonicalize()
        .map_err(|error| format!("Could not resolve device SDK root: {error}"))
}

fn collect_gmp_paths(directory: &Path, files: &mut Vec<PathBuf>) -> Result<(), String> {
    let mut entries = fs::read_dir(directory)
        .map_err(|error| format!("Could not scan {}: {error}", directory.display()))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| format!("Could not inspect {}: {error}", directory.display()))?;
    entries.sort_by_key(|entry| entry.file_name());
    for entry in entries {
        let file_type = entry
            .file_type()
            .map_err(|error| format!("Could not inspect {}: {error}", entry.path().display()))?;
        if file_type.is_dir() {
            collect_gmp_paths(&entry.path(), files)?;
        } else if file_type.is_file()
            && entry
                .path()
                .extension()
                .and_then(|value| value.to_str())
                .is_some_and(|value| value.eq_ignore_ascii_case("gmp"))
        {
            files.push(entry.path());
        }
    }
    Ok(())
}

fn device_plugin_descriptor(
    package: PathBuf,
    sdk: Option<&Path>,
) -> Result<DiscoveredDevicePlugin, String> {
    let package = package
        .canonicalize()
        .map_err(|error| format!("Could not resolve device plugin: {error}"))?;
    let stem = package
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("device-plugin")
        .to_string();
    let mut id = stem.clone();
    let mut name = stem.clone();
    let mut version = String::new();
    let mut provides = Vec::new();
    let mut metadata_available = false;
    let manifest_path = find_device_manifest(&package, sdk);
    if let Some(manifest_path) = manifest_path.as_ref() {
        if let Ok(file) = File::open(manifest_path) {
            if let Ok(manifest) = serde_json::from_reader::<_, serde_json::Value>(file) {
                metadata_available = true;
                id = manifest["id"].as_str().unwrap_or(&id).to_string();
                name = manifest["name"].as_str().unwrap_or(&name).to_string();
                version = manifest["version"]
                    .as_str()
                    .map(str::to_string)
                    .unwrap_or_else(|| manifest["version"].to_string());
                if version == "null" {
                    version.clear();
                }
                provides = manifest["provides"]["protocols"]
                    .as_array()
                    .into_iter()
                    .flatten()
                    .filter_map(|protocol| {
                        serde_json::from_value::<ProvidedProtocol>(protocol.clone()).ok()
                    })
                    .collect();
            }
        }
    }
    Ok(DiscoveredDevicePlugin {
        id,
        name,
        version,
        path: package.to_string_lossy().into_owned(),
        provides,
        metadata_available,
    })
}

fn find_device_manifest(package: &Path, sdk: Option<&Path>) -> Option<PathBuf> {
    if let Some(sdk) = sdk {
        if let Ok(relative) = package.strip_prefix(sdk.join("build-host")) {
            if let Some(parent) = relative.parent() {
                let manifest = sdk.join("examples").join(parent).join("manifest.json");
                if manifest.is_file() {
                    return Some(manifest);
                }
            }
        }
    }
    if let Some(parent) = package.parent() {
        let sidecar = parent.join("manifest.json");
        if sidecar.is_file() {
            return Some(sidecar);
        }
    }
    for build_root in package
        .ancestors()
        .filter(|path| path.file_name().is_some_and(|name| name == "build-host"))
    {
        let relative = package.strip_prefix(build_root).ok()?.parent()?;
        let workspace = build_root.parent()?;
        let manifest = workspace
            .join("examples")
            .join(relative)
            .join("manifest.json");
        if manifest.is_file() {
            return Some(manifest);
        }
    }
    None
}

fn discover_device_plugins_in(root: &Path) -> Result<Vec<DiscoveredDevicePlugin>, String> {
    let sdk = device_sdk_root().ok();
    let mut packages = Vec::new();
    if root.is_file() {
        packages.push(root.to_path_buf());
    } else {
        collect_gmp_paths(root, &mut packages)?;
    }
    let mut plugins = packages
        .into_iter()
        .map(|package| device_plugin_descriptor(package, sdk.as_deref()))
        .collect::<Result<Vec<_>, _>>()?;
    plugins.sort_by(|left, right| left.name.cmp(&right.name).then(left.id.cmp(&right.id)));
    Ok(plugins)
}

#[tauri::command]
fn discover_device_plugins() -> Result<Vec<DiscoveredDevicePlugin>, String> {
    let sdk = device_sdk_root()?;
    let build_root = sdk.join("build-host");
    if !build_root.is_dir() {
        return Ok(Vec::new());
    }
    discover_device_plugins_in(&build_root)
}

#[tauri::command]
fn import_device_workspace(path: String) -> Result<Vec<DiscoveredDevicePlugin>, String> {
    let root = PathBuf::from(path)
        .canonicalize()
        .map_err(|error| format!("Could not resolve device workspace: {error}"))?;
    let plugins = discover_device_plugins_in(&root)?;
    if plugins.is_empty() {
        return Err("The selected workspace does not contain a built .gmp file".to_string());
    }
    Ok(plugins)
}

fn choose_lan_address() -> Result<Ipv4Addr, String> {
    let probe = UdpSocket::bind((Ipv4Addr::UNSPECIFIED, 0))
        .map_err(|error| format!("Could not create LAN probe: {error}"))?;
    probe
        .connect((Ipv4Addr::new(192, 0, 2, 1), 9))
        .map_err(|error| format!("Could not determine LAN route: {error}"))?;
    match probe
        .local_addr()
        .map_err(|error| format!("Could not determine LAN address: {error}"))?
        .ip()
    {
        std::net::IpAddr::V4(address) if !address.is_loopback() => Ok(address),
        _ => Err("Could not determine a reachable IPv4 LAN address".to_string()),
    }
}

fn collect_mmpkg_files(
    directory: &Path,
    root: &Path,
    files: &mut Vec<(String, Vec<u8>)>,
) -> Result<(), String> {
    let mut entries = fs::read_dir(directory)
        .map_err(|error| format!("Could not read {}: {error}", directory.display()))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| format!("Could not inspect {}: {error}", directory.display()))?;
    entries.sort_by_key(|entry| entry.file_name());
    for entry in entries {
        let file_type = entry
            .file_type()
            .map_err(|error| format!("Could not inspect {}: {error}", entry.path().display()))?;
        if file_type.is_symlink() {
            return Err(format!(
                "Web plugin directories must not contain symbolic links: {}",
                entry.path().display()
            ));
        }
        if file_type.is_dir() {
            collect_mmpkg_files(&entry.path(), root, files)?;
            continue;
        }
        if !file_type.is_file() {
            return Err(format!(
                "Web plugin contains an unsupported file type: {}",
                entry.path().display()
            ));
        }
        let relative = entry
            .path()
            .strip_prefix(root)
            .map_err(|_| "Web plugin file escaped its source directory".to_string())?
            .components()
            .map(|component| component.as_os_str().to_string_lossy().into_owned())
            .collect::<Vec<_>>()
            .join("/");
        if relative.is_empty()
            || relative.starts_with('/')
            || relative
                .split('/')
                .any(|component| component.is_empty() || matches!(component, "." | ".."))
        {
            return Err(format!("Web plugin contains an unsafe path: {relative}"));
        }
        let bytes = fs::read(entry.path())
            .map_err(|error| format!("Could not read {relative}: {error}"))?;
        if bytes.len() as u64 > MAX_MMPKG_BYTES {
            return Err(format!("{relative} exceeds the 10 MB file limit"));
        }
        files.push((relative, bytes));
        if files.len() > MAX_MMPKG_FILES {
            return Err(format!(
                "Web plugin contains more than {MAX_MMPKG_FILES} files"
            ));
        }
    }
    Ok(())
}

fn build_mmpkg(source: &Path, output: &Path) -> Result<(), String> {
    let mut files = Vec::new();
    collect_mmpkg_files(source, source, &mut files)?;
    let manifest_index = files
        .iter()
        .position(|(path, _)| path == "manifest.json")
        .ok_or_else(|| "Web plugin directory does not contain manifest.json".to_string())?;
    let (_, manifest_bytes) = files.remove(manifest_index);
    let mut manifest: serde_json::Value = serde_json::from_slice(&manifest_bytes)
        .map_err(|error| format!("Invalid Web manifest: {error}"))?;
    let manifest = manifest
        .as_object_mut()
        .ok_or_else(|| "Web manifest must be a JSON object".to_string())?;
    let entry = manifest
        .get("entry")
        .and_then(serde_json::Value::as_str)
        .ok_or_else(|| "Web manifest entry must be a string".to_string())?
        .to_string();
    safe_relative_entry(source, &entry)?;

    let signature = files
        .iter()
        .position(|(path, _)| path == "signature.sig")
        .map(|index| files.remove(index));
    files.sort_by(|left, right| left.0.cmp(&right.0));
    if !files.iter().any(|(path, _)| path == &entry) {
        return Err(format!("Web plugin entry is missing: {entry}"));
    }
    let extracted_bytes = files
        .iter()
        .map(|(_, bytes)| bytes.len() as u64)
        .sum::<u64>()
        + signature
            .as_ref()
            .map(|(_, bytes)| bytes.len() as u64)
            .unwrap_or(0);
    if extracted_bytes > MAX_MMPKG_EXTRACTED_BYTES {
        return Err("Web plugin expands beyond the 30 MB limit".to_string());
    }

    let hashes = files
        .iter()
        .map(|(path, bytes)| {
            (
                path.clone(),
                serde_json::Value::String(format!("sha256:{:x}", Sha256::digest(bytes))),
            )
        })
        .collect::<serde_json::Map<_, _>>();
    manifest.insert("schemaVersion".to_string(), serde_json::json!(1));
    manifest.insert("files".to_string(), serde_json::Value::Object(hashes));
    let mut final_manifest = serde_json::to_vec_pretty(manifest)
        .map_err(|error| format!("Could not encode Web manifest: {error}"))?;
    final_manifest.push(b'\n');
    if extracted_bytes + final_manifest.len() as u64 > MAX_MMPKG_EXTRACTED_BYTES {
        return Err("Web plugin expands beyond the 30 MB limit".to_string());
    }

    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent)
            .map_err(|error| format!("Could not create package output directory: {error}"))?;
    }
    let temporary = output.with_extension(format!("tmp-{}", uuid::Uuid::new_v4().simple()));
    let result = (|| -> Result<(), String> {
        let file = File::create(&temporary)
            .map_err(|error| format!("Could not create Web plugin package: {error}"))?;
        let mut archive = zip::ZipWriter::new(file);
        let options = zip::write::SimpleFileOptions::default()
            .compression_method(zip::CompressionMethod::Deflated)
            .unix_permissions(0o644);
        archive
            .start_file("manifest.json", options)
            .map_err(|error| format!("Could not write package manifest: {error}"))?;
        archive
            .write_all(&final_manifest)
            .map_err(|error| format!("Could not write package manifest: {error}"))?;
        for (path, bytes) in &files {
            archive
                .start_file(path, options)
                .map_err(|error| format!("Could not add {path} to package: {error}"))?;
            archive
                .write_all(bytes)
                .map_err(|error| format!("Could not add {path} to package: {error}"))?;
        }
        if let Some((path, bytes)) = &signature {
            archive
                .start_file(path, options)
                .map_err(|error| format!("Could not add package signature: {error}"))?;
            archive
                .write_all(bytes)
                .map_err(|error| format!("Could not add package signature: {error}"))?;
        }
        archive
            .finish()
            .map_err(|error| format!("Could not finalize Web plugin package: {error}"))?;
        let package_bytes = fs::read(&temporary)
            .map_err(|error| format!("Could not verify Web plugin package: {error}"))?;
        validate_mmpkg_bytes(&package_bytes)?;
        if output.exists() {
            fs::remove_file(output)
                .map_err(|error| format!("Could not replace Web plugin package: {error}"))?;
        }
        fs::rename(&temporary, output)
            .map_err(|error| format!("Could not finalize Web plugin package: {error}"))
    })();
    if result.is_err() {
        let _ = fs::remove_file(&temporary);
    }
    result
}

fn checked_mmpkg(path: &Path) -> Result<Vec<u8>, String> {
    if path.extension().and_then(|value| value.to_str()) != Some("mmpkg") {
        return Err("Web plugin package must use the .mmpkg extension".to_string());
    }
    let bytes =
        fs::read(path).map_err(|error| format!("Could not read Web plugin package: {error}"))?;
    validate_mmpkg_bytes(&bytes)?;
    Ok(bytes)
}

fn validate_mmpkg_bytes(bytes: &[u8]) -> Result<(), String> {
    if bytes.is_empty() || bytes.len() as u64 > MAX_MMPKG_BYTES {
        return Err(format!(
            "Web plugin package must be 1..={MAX_MMPKG_BYTES} bytes"
        ));
    }
    if !bytes.starts_with(b"PK\x03\x04") {
        return Err("Web plugin package is not a ZIP container".to_string());
    }

    let mut archive = zip::ZipArchive::new(Cursor::new(bytes))
        .map_err(|error| format!("Invalid Web plugin package: {error}"))?;
    if archive.len() > MAX_MMPKG_FILES {
        return Err(format!(
            "Web plugin package contains more than {MAX_MMPKG_FILES} entries"
        ));
    }

    let mut seen = HashSet::new();
    let mut payload_hashes = HashMap::new();
    let mut manifest_bytes = None;
    let mut extracted_bytes = 0u64;
    for index in 0..archive.len() {
        let mut entry = archive
            .by_index(index)
            .map_err(|error| format!("Could not inspect package entry: {error}"))?;
        let name = entry.name().to_string();
        if !seen.insert(name.clone()) {
            return Err(format!(
                "Web plugin package contains duplicate path: {name}"
            ));
        }
        if entry
            .unix_mode()
            .is_some_and(|mode| mode & 0o170000 == 0o120000)
        {
            return Err("Web plugin package must not contain symbolic links".to_string());
        }
        validate_archive_path(&name, entry.is_dir())?;
        if entry.size() > MAX_MMPKG_BYTES {
            return Err(format!("{name} exceeds the 10 MB file limit"));
        }
        extracted_bytes = extracted_bytes
            .checked_add(entry.size())
            .filter(|total| *total <= MAX_MMPKG_EXTRACTED_BYTES)
            .ok_or_else(|| "Web plugin package expands beyond the 30 MB limit".to_string())?;
        if entry.is_dir() {
            continue;
        }
        let mut content = Vec::with_capacity(entry.size() as usize);
        entry
            .read_to_end(&mut content)
            .map_err(|error| format!("Could not read package entry {name}: {error}"))?;
        if content.len() as u64 > MAX_MMPKG_BYTES {
            return Err(format!("{name} exceeds the 10 MB file limit"));
        }
        if name == "manifest.json" {
            manifest_bytes = Some(content);
        } else if name != "signature.sig" {
            payload_hashes.insert(name, format!("sha256:{:x}", Sha256::digest(&content)));
        }
    }

    let manifest_bytes = manifest_bytes
        .ok_or_else(|| "Web plugin package does not contain manifest.json".to_string())?;
    let manifest_value: serde_json::Value = serde_json::from_slice(&manifest_bytes)
        .map_err(|error| format!("Invalid packaged Web manifest: {error}"))?;
    let manifest = manifest_value
        .as_object()
        .ok_or_else(|| "Packaged Web manifest must be a JSON object".to_string())?;
    if manifest
        .get("schemaVersion")
        .and_then(serde_json::Value::as_u64)
        != Some(1)
    {
        return Err("Web plugin package requires schemaVersion 1".to_string());
    }
    if manifest
        .get("bridgeVersion")
        .and_then(serde_json::Value::as_str)
        != Some("1.0")
    {
        return Err("Web plugin package requires bridgeVersion 1.0".to_string());
    }
    let discovered: DiscoveredManifest = serde_json::from_value(manifest_value.clone())
        .map_err(|error| format!("Invalid packaged Web manifest: {error}"))?;
    validate_manifest_identity(&discovered)?;
    let declared_files = manifest
        .get("files")
        .and_then(serde_json::Value::as_object)
        .ok_or_else(|| "Web plugin package manifest requires a files object".to_string())?;
    if declared_files.len() != payload_hashes.len() {
        return Err(
            "Web plugin package files table does not exactly cover its payload".to_string(),
        );
    }
    for (path, actual_hash) in &payload_hashes {
        validate_archive_path(path, false)?;
        let declared_hash = declared_files
            .get(path)
            .and_then(serde_json::Value::as_str)
            .ok_or_else(|| format!("Web plugin package files table is missing {path}"))?;
        if declared_hash != actual_hash {
            return Err(format!("Web plugin package hash mismatch for {path}"));
        }
    }
    if !payload_hashes.contains_key(&discovered.entry) {
        return Err(format!("Web plugin entry is missing: {}", discovered.entry));
    }
    Ok(())
}

fn validate_archive_path(path: &str, is_directory: bool) -> Result<(), String> {
    let path = if is_directory {
        path.strip_suffix('/').unwrap_or(path)
    } else {
        path
    };
    if path.is_empty()
        || path.starts_with('/')
        || path.contains('\\')
        || path
            .split('/')
            .any(|component| component.is_empty() || matches!(component, "." | ".."))
    {
        return Err(format!("Web plugin contains an unsafe path: {path}"));
    }
    Ok(())
}

fn validate_manifest_identity(manifest: &DiscoveredManifest) -> Result<(), String> {
    if manifest.id.trim().is_empty()
        || manifest.name.trim().is_empty()
        || manifest.version.trim().is_empty()
        || manifest.entry.trim().is_empty()
    {
        return Err("Web manifest is missing identity fields".to_string());
    }
    validate_archive_path(&manifest.entry, false)?;
    if !manifest.entry.to_ascii_lowercase().ends_with(".html") {
        return Err("Web manifest entry must be an HTML file".to_string());
    }
    if manifest.permissions.len() > 16 {
        return Err("Web manifest permissions must contain at most 16 items".to_string());
    }
    let supported = ["display", "device.events", "storage", "network"];
    let mut seen = HashSet::new();
    for permission in &manifest.permissions {
        if !supported.contains(&permission.as_str()) {
            return Err(format!("Unsupported Web plugin permission: {permission}"));
        }
        if !seen.insert(permission) {
            return Err(format!("Duplicate Web plugin permission: {permission}"));
        }
    }
    Ok(())
}

fn serve_mmpkg_client(mut stream: TcpStream, package: &Path) -> Result<(), String> {
    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .map_err(|error| format!("Could not set client timeout: {error}"))?;
    let clone = stream
        .try_clone()
        .map_err(|error| format!("Could not read client connection: {error}"))?;
    let mut reader = BufReader::new(clone);
    let mut request = Vec::new();
    reader
        .by_ref()
        .take(64)
        .read_until(b'\n', &mut request)
        .map_err(|error| format!("Could not read package request: {error}"))?;
    if request != MMPKG_REQUEST_LINE {
        stream
            .write_all(b"MMPKG/1 ERROR invalid-request\n")
            .map_err(|error| format!("Could not reject package request: {error}"))?;
        return Ok(());
    }
    let bytes = match checked_mmpkg(package) {
        Ok(bytes) => bytes,
        Err(_) => {
            stream
                .write_all(b"MMPKG/1 ERROR invalid-package\n")
                .map_err(|error| format!("Could not report invalid package: {error}"))?;
            return Ok(());
        }
    };
    let checksum = Sha256::digest(&bytes);
    write!(stream, "MMPKG/1 OK {} {:x}\n", bytes.len(), checksum)
        .map_err(|error| format!("Could not send package metadata: {error}"))?;
    stream
        .write_all(&bytes)
        .map_err(|error| format!("Could not send Web plugin package: {error}"))
}

fn start_mmpkg_server(package: PathBuf) -> Result<(ActivePackageShare, u16), String> {
    checked_mmpkg(&package)?;
    let listener = TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, MMPKG_SHARE_PORT))
        .or_else(|_| TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, 0)))
        .map_err(|error| format!("Could not start Web package server: {error}"))?;
    listener
        .set_nonblocking(true)
        .map_err(|error| format!("Could not configure Web package server: {error}"))?;
    let port = listener
        .local_addr()
        .map_err(|error| format!("Could not inspect Web package server: {error}"))?
        .port();
    let stop = Arc::new(AtomicBool::new(false));
    let thread_stop = Arc::clone(&stop);
    let thread = thread::Builder::new()
        .name("gm-mmpkg-share".to_string())
        .spawn(move || {
            while !thread_stop.load(Ordering::Relaxed) {
                match listener.accept() {
                    Ok((stream, _)) => {
                        let package = package.clone();
                        let _ = thread::Builder::new()
                            .name("gm-mmpkg-client".to_string())
                            .spawn(move || {
                                let _ = serve_mmpkg_client(stream, &package);
                            });
                    }
                    Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                        thread::sleep(Duration::from_millis(50));
                    }
                    Err(_) => break,
                }
            }
        })
        .map_err(|error| format!("Could not start Web package server thread: {error}"))?;
    Ok((
        ActivePackageShare {
            stop,
            thread: Some(thread),
        },
        port,
    ))
}

fn checked_gmp(path: &Path) -> Result<Vec<u8>, String> {
    if !path
        .extension()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value.eq_ignore_ascii_case("gmp"))
    {
        return Err("Device plugin package must use the .gmp extension".to_string());
    }
    let bytes = fs::read(path).map_err(|error| format!("Could not read device plugin: {error}"))?;
    let size = bytes.len() as u64;
    if !(MIN_GMP_BYTES..=MAX_GMP_BYTES).contains(&size) {
        return Err(format!(
            "Device plugin package must be {MIN_GMP_BYTES}..={MAX_GMP_BYTES} bytes"
        ));
    }
    if !bytes.starts_with(b"GMPK") {
        return Err("Device plugin package does not start with GMPK".to_string());
    }
    Ok(bytes)
}

fn serve_gmp_client(mut stream: TcpStream, package: &Path) -> Result<(), String> {
    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .map_err(|error| format!("Could not set client timeout: {error}"))?;
    let clone = stream
        .try_clone()
        .map_err(|error| format!("Could not read client connection: {error}"))?;
    let mut reader = BufReader::new(clone);
    let mut request = Vec::new();
    reader
        .by_ref()
        .take(64)
        .read_until(b'\n', &mut request)
        .map_err(|error| format!("Could not read package request: {error}"))?;
    if request != GMP_REQUEST_LINE {
        stream
            .write_all(b"GMP/1 ERROR invalid-request\n")
            .map_err(|error| format!("Could not reject package request: {error}"))?;
        return Ok(());
    }
    let bytes = match checked_gmp(package) {
        Ok(bytes) => bytes,
        Err(_) => {
            stream
                .write_all(b"GMP/1 ERROR invalid-package\n")
                .map_err(|error| format!("Could not report invalid package: {error}"))?;
            return Ok(());
        }
    };
    write!(
        stream,
        "GMP/1 OK {} {:x}\n",
        bytes.len(),
        Sha256::digest(&bytes)
    )
    .map_err(|error| format!("Could not send package metadata: {error}"))?;
    stream
        .write_all(&bytes)
        .map_err(|error| format!("Could not send device plugin package: {error}"))
}

fn start_gmp_server(package: PathBuf) -> Result<(ActivePackageShare, u16), String> {
    checked_gmp(&package)?;
    let listener = TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, GMP_SHARE_PORT))
        .or_else(|_| TcpListener::bind(SocketAddrV4::new(Ipv4Addr::UNSPECIFIED, 0)))
        .map_err(|error| format!("Could not start device package server: {error}"))?;
    listener
        .set_nonblocking(true)
        .map_err(|error| format!("Could not configure device package server: {error}"))?;
    let port = listener
        .local_addr()
        .map_err(|error| format!("Could not inspect device package server: {error}"))?
        .port();
    let stop = Arc::new(AtomicBool::new(false));
    let thread_stop = Arc::clone(&stop);
    let thread = thread::Builder::new()
        .name("gm-gmp-share".to_string())
        .spawn(move || {
            while !thread_stop.load(Ordering::Relaxed) {
                match listener.accept() {
                    Ok((stream, _)) => {
                        let package = package.clone();
                        let _ = thread::Builder::new()
                            .name("gm-gmp-client".to_string())
                            .spawn(move || {
                                let _ = serve_gmp_client(stream, &package);
                            });
                    }
                    Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                        thread::sleep(Duration::from_millis(50));
                    }
                    Err(_) => break,
                }
            }
        })
        .map_err(|error| format!("Could not start device package server thread: {error}"))?;
    Ok((
        ActivePackageShare {
            stop,
            thread: Some(thread),
        },
        port,
    ))
}

fn safe_output_stem(path: &Path) -> String {
    let name = path.file_name().and_then(|value| value.to_str());
    let package_root = if matches!(name, Some("dist" | "build")) {
        path.parent().unwrap_or(path)
    } else {
        path
    };
    package_root
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or("web-plugin")
        .chars()
        .map(|character| {
            if character.is_ascii_alphanumeric() || matches!(character, '-' | '_' | '.') {
                character
            } else {
                '-'
            }
        })
        .collect()
}

#[tauri::command]
fn build_and_share_web_plugin(
    path: String,
    state: tauri::State<'_, PackageShareState>,
) -> Result<PackageShareResult, String> {
    let source = PathBuf::from(path)
        .canonicalize()
        .map_err(|error| format!("Could not resolve Web plugin: {error}"))?;
    let output = if source.is_dir() {
        let manifest = read_discovered_manifest(&source)?;
        let root = sdk_root()?;
        let output = root.join("dist").join(format!(
            "{}-{}.mmpkg",
            safe_output_stem(&source),
            manifest.version
        ));
        build_mmpkg(&source, &output)?;
        output
    } else {
        checked_mmpkg(&source)?;
        source
    };
    checked_mmpkg(&output)?;
    let host = choose_lan_address()?;

    let mut active = state
        .active
        .lock()
        .map_err(|_| "Web package share lock is poisoned".to_string())?;
    if let Some(previous) = active.take() {
        previous.stop();
    }
    let (server, port) = start_mmpkg_server(output.clone())?;
    *active = Some(server);

    let name = output
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| "Generated package name is not UTF-8".to_string())?
        .to_string();
    let qr_payload = serde_json::json!({
        "v": 1,
        "scheme": "mmpkg+tcp",
        "host": host.to_string(),
        "port": port,
        "name": name,
    })
    .to_string();
    let qr = QrCode::new(qr_payload.as_bytes())
        .map_err(|error| format!("Could not generate QR code: {error}"))?;
    let qr_size = qr.width();
    let qr_modules = qr
        .to_colors()
        .into_iter()
        .map(|color| color == Color::Dark)
        .collect();
    Ok(PackageShareResult {
        package_path: output.to_string_lossy().into_owned(),
        qr_payload,
        host: host.to_string(),
        port,
        name,
        qr_size,
        qr_modules,
    })
}

#[tauri::command]
fn share_device_plugin(
    path: String,
    state: tauri::State<'_, PackageShareState>,
) -> Result<PackageShareResult, String> {
    let package = canonical_gmp(&path)?;
    checked_gmp(&package)?;
    let host = choose_lan_address()?;
    let mut active = state
        .active
        .lock()
        .map_err(|_| "Package share lock is poisoned".to_string())?;
    if let Some(previous) = active.take() {
        previous.stop();
    }
    let (server, port) = start_gmp_server(package.clone())?;
    *active = Some(server);
    let name = package
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| "Device package name is not UTF-8".to_string())?
        .to_string();
    let qr_payload = serde_json::json!({
        "v": 1,
        "scheme": "gmp+tcp",
        "host": host.to_string(),
        "port": port,
        "name": name,
    })
    .to_string();
    let qr = QrCode::new(qr_payload.as_bytes())
        .map_err(|error| format!("Could not generate QR code: {error}"))?;
    let qr_size = qr.width();
    let qr_modules = qr
        .to_colors()
        .into_iter()
        .map(|color| color == Color::Dark)
        .collect();
    Ok(PackageShareResult {
        package_path: package.to_string_lossy().into_owned(),
        qr_payload,
        host: host.to_string(),
        port,
        name,
        qr_size,
        qr_modules,
    })
}

#[tauri::command]
fn stop_web_package_share(state: tauri::State<'_, PackageShareState>) -> Result<(), String> {
    let mut active = state
        .active
        .lock()
        .map_err(|_| "Web package share lock is poisoned".to_string())?;
    if let Some(server) = active.take() {
        server.stop();
    }
    Ok(())
}

#[tauri::command]
fn pick_web_package() -> Option<String> {
    rfd::FileDialog::new()
        .set_title("Choose a Web plugin package")
        .add_filter("GM Web plugin", &["mmpkg"])
        .pick_file()
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
fn pick_web_directory() -> Option<String> {
    rfd::FileDialog::new()
        .set_title("Choose an unpacked Web plugin directory")
        .pick_folder()
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
fn pick_device_plugin() -> Option<String> {
    rfd::FileDialog::new()
        .set_title("Choose a device plugin")
        .add_filter("GM device plugin", &["gmp"])
        .pick_file()
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
fn pick_device_directory() -> Option<String> {
    rfd::FileDialog::new()
        .set_title("Choose a device plugin workspace")
        .pick_folder()
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
fn resolve_web_entry(
    path: String,
    server: tauri::State<'_, WebPluginServer>,
) -> Result<String, String> {
    let requested = PathBuf::from(path);
    let entry = if requested.is_dir() {
        resolve_directory_entry(&requested)?
    } else if requested
        .extension()
        .and_then(|value| value.to_str())
        .is_some_and(|value| value.eq_ignore_ascii_case("mmpkg"))
    {
        extract_web_package(&requested)?
    } else {
        requested
    };
    if entry.extension().and_then(|value| value.to_str()) != Some("html") {
        return Err("Web plugin entry must be an index.html file or directory".to_string());
    }
    if !entry.is_file() {
        return Err(format!("Web plugin entry not found: {}", entry.display()));
    }
    let canonical = entry
        .canonicalize()
        .map_err(|error| format!("Could not resolve Web plugin entry: {error}"))?;
    let directory = canonical
        .ancestors()
        .skip(1)
        .find(|directory| directory.join("manifest.json").is_file())
        .or_else(|| canonical.parent())
        .ok_or_else(|| "Web plugin entry does not have a parent directory".to_string())?;
    server.url_for(directory, &canonical)
}

fn resolve_directory_entry(directory: &Path) -> Result<PathBuf, String> {
    let manifest_path = directory.join("manifest.json");
    if !manifest_path.is_file() {
        return Ok(directory.join("index.html"));
    }
    let manifest: WebManifest = serde_json::from_reader(
        File::open(&manifest_path)
            .map_err(|error| format!("Could not open Web manifest: {error}"))?,
    )
    .map_err(|error| format!("Invalid Web manifest: {error}"))?;
    safe_relative_entry(directory, &manifest.entry)
}

fn extract_web_package(package: &Path) -> Result<PathBuf, String> {
    if !package.is_file() {
        return Err(format!(
            "Web plugin package not found: {}",
            package.display()
        ));
    }
    let canonical = package
        .canonicalize()
        .map_err(|error| format!("Could not resolve Web plugin package: {error}"))?;
    let package_bytes = checked_mmpkg(&canonical)?;
    let package_hash = format!("{:x}", Sha256::digest(&package_bytes));
    let output = std::env::temp_dir()
        .join("gm-plugin-studio")
        .join(std::process::id().to_string())
        .join(&package_hash[..32]);
    if output.is_dir() {
        return resolve_directory_entry(&output);
    }

    let mut archive = zip::ZipArchive::new(Cursor::new(package_bytes))
        .map_err(|error| format!("Invalid Web plugin package: {error}"))?;

    let staging = output.with_extension("partial");
    if staging.exists() {
        fs::remove_dir_all(&staging)
            .map_err(|error| format!("Could not reset Web plugin cache: {error}"))?;
    }
    fs::create_dir_all(&staging)
        .map_err(|error| format!("Could not create Web plugin cache: {error}"))?;
    for index in 0..archive.len() {
        let mut entry = archive
            .by_index(index)
            .map_err(|error| format!("Could not read package entry: {error}"))?;
        let relative = entry
            .enclosed_name()
            .ok_or_else(|| "Web plugin package contains an unsafe path".to_string())?;
        let destination = staging.join(relative);
        if entry.is_dir() {
            fs::create_dir_all(&destination)
                .map_err(|error| format!("Could not create package directory: {error}"))?;
            continue;
        }
        if entry
            .unix_mode()
            .is_some_and(|mode| mode & 0o170000 == 0o120000)
        {
            return Err("Web plugin package must not contain symbolic links".to_string());
        }
        if let Some(parent) = destination.parent() {
            fs::create_dir_all(parent)
                .map_err(|error| format!("Could not create package directory: {error}"))?;
        }
        let mut destination_file = File::create(&destination)
            .map_err(|error| format!("Could not extract package file: {error}"))?;
        io::copy(&mut entry, &mut destination_file)
            .map_err(|error| format!("Could not extract package file: {error}"))?;
    }
    let _ = resolve_directory_entry(&staging)?;
    fs::rename(&staging, &output)
        .map_err(|error| format!("Could not finalize Web plugin cache: {error}"))?;
    resolve_directory_entry(&output)
}

fn safe_relative_entry(directory: &Path, value: &str) -> Result<PathBuf, String> {
    let relative = Path::new(value);
    if relative.as_os_str().is_empty()
        || relative.is_absolute()
        || relative
            .components()
            .any(|part| !matches!(part, std::path::Component::Normal(_)))
    {
        return Err("Web manifest entry must be a safe relative path".to_string());
    }
    Ok(directory.join(relative))
}

#[tauri::command]
fn load_device_plugin(
    path: String,
    state: tauri::State<'_, PreviewerState>,
) -> Result<PreviewStatus, String> {
    let source = canonical_gmp(&path)?;
    let encoded = CString::new(source.to_string_lossy().as_bytes())
        .map_err(|_| "Device plugin path contains a null byte".to_string())?;
    let mut previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    let load = unsafe { gm_preview_load(previewer.handle.as_ptr(), encoded.as_ptr()) };
    previewer.require(load)?;
    let start = unsafe { gm_preview_start(previewer.handle.as_ptr()) };
    previewer.require(start)?;
    previewer.source = Some(source);
    Ok(status(&previewer))
}

#[tauri::command]
fn stop_device_plugin(state: tauri::State<'_, PreviewerState>) -> Result<PreviewStatus, String> {
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    if unsafe { gm_preview_is_running(previewer.handle.as_ptr()) } != 0 {
        let stopped = unsafe { gm_preview_stop(previewer.handle.as_ptr()) };
        previewer.require(stopped)?;
    }
    Ok(status(&previewer))
}

#[tauri::command]
fn preview_status(state: tauri::State<'_, PreviewerState>) -> Result<PreviewStatus, String> {
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    Ok(status(&previewer))
}

#[tauri::command]
fn send_plugin_message(
    channel: u16,
    payload: Vec<u8>,
    state: tauri::State<'_, PreviewerState>,
) -> Result<PluginMessageResult, String> {
    if payload.is_empty() {
        return Err("plugin message payload must not be empty".to_string());
    }
    if payload.len() > MAX_PLUGIN_PAYLOAD_BYTES {
        return Err(format!(
            "plugin message exceeds {MAX_PLUGIN_PAYLOAD_BYTES} bytes"
        ));
    }
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    if unsafe { gm_preview_is_running(previewer.handle.as_ptr()) } == 0 {
        return Err("device plugin is not running".to_string());
    }
    let mut handled = 0;
    let sent = unsafe {
        gm_preview_send_bluetooth(
            previewer.handle.as_ptr(),
            channel,
            payload.as_ptr(),
            payload.len(),
            &mut handled,
        )
    };
    previewer.require(sent)?;
    Ok(PluginMessageResult {
        sent: true,
        handled: handled != 0,
        channel,
        payload_bytes: payload.len(),
    })
}

#[tauri::command]
fn simulate_button(
    action: u16,
    state: tauri::State<'_, PreviewerState>,
) -> Result<SimulatedEventResult, String> {
    if !(1..=5).contains(&action) {
        return Err("button action must be between 1 and 5".to_string());
    }
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    require_running(&previewer)?;
    let mut handled = 0;
    let sent =
        unsafe { gm_preview_send_button(previewer.handle.as_ptr(), action, 1, &mut handled) };
    previewer.require(sent)?;
    Ok(SimulatedEventResult {
        handled: handled != 0,
    })
}

#[tauri::command]
fn simulate_gesture(
    gesture: u16,
    active: bool,
    state: tauri::State<'_, PreviewerState>,
) -> Result<SimulatedEventResult, String> {
    if !(1..=8).contains(&gesture) {
        return Err("gesture must be between 1 and 8".to_string());
    }
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    require_running(&previewer)?;
    let mut handled = 0;
    let sent = unsafe {
        if active && matches!(gesture, 2 | 3 | 7 | 8) {
            gm_preview_simulate_direction_gesture(previewer.handle.as_ptr(), gesture, &mut handled)
        } else {
            gm_preview_send_gesture(
                previewer.handle.as_ptr(),
                gesture,
                if active { 1 } else { 0 },
                &mut handled,
            )
        }
    };
    previewer.require(sent)?;
    Ok(SimulatedEventResult {
        handled: handled != 0,
    })
}

fn require_running(previewer: &NativePreviewer) -> Result<(), String> {
    if unsafe { gm_preview_is_running(previewer.handle.as_ptr()) } == 0 {
        Err("device plugin is not running".to_string())
    } else {
        Ok(())
    }
}

#[tauri::command]
fn tick_frame(
    elapsed_ms: u32,
    state: tauri::State<'_, PreviewerState>,
) -> Result<FrameResult, String> {
    let previewer = state
        .lock()
        .map_err(|_| "previewer lock is poisoned".to_string())?;
    let running = unsafe { gm_preview_is_running(previewer.handle.as_ptr()) } != 0;
    if running {
        let ticked =
            unsafe { gm_preview_tick(previewer.handle.as_ptr(), elapsed_ms.clamp(1, 100)) };
        previewer.require(ticked)?;
    }
    let frame_size = unsafe { gm_preview_frame_size() };
    if frame_size != DISPLAY_WIDTH * DISPLAY_HEIGHT {
        return Err(format!("unexpected native frame size: {frame_size}"));
    }
    let mut pixels = vec![0u8; frame_size];
    let copied = unsafe {
        gm_preview_copy_frame(previewer.handle.as_ptr(), pixels.as_mut_ptr(), pixels.len())
    };
    previewer.require(copied)?;
    let mut gray4 = vec![0u8; frame_size / 2];
    for (index, pair) in pixels.chunks_exact(2).enumerate() {
        gray4[index] = ((pair[0] / 17) << 4) | (pair[1] / 17);
    }
    Ok(FrameResult {
        width: DISPLAY_WIDTH,
        height: DISPLAY_HEIGHT,
        gray4_base64: BASE64.encode(gray4),
        running,
        messages: drain_outbox(&previewer)?,
    })
}

fn drain_outbox(previewer: &NativePreviewer) -> Result<Vec<OutboundMessage>, String> {
    let count = unsafe { gm_preview_outbox_count(previewer.handle.as_ptr()) };
    let mut messages = Vec::with_capacity(count);
    for index in 0..count {
        let mut channel = 0u16;
        let channel_read =
            unsafe { gm_preview_outbox_channel(previewer.handle.as_ptr(), index, &mut channel) };
        previewer.require(channel_read)?;
        let size = unsafe { gm_preview_outbox_payload_size(previewer.handle.as_ptr(), index) };
        let mut payload = vec![0u8; size];
        let copied = unsafe {
            gm_preview_copy_outbox_payload(
                previewer.handle.as_ptr(),
                index,
                payload.as_mut_ptr(),
                payload.len(),
            )
        };
        previewer.require(copied)?;
        messages.push(OutboundMessage {
            channel,
            data_base64: BASE64.encode(payload),
        });
    }
    let cleared = unsafe { gm_preview_clear_outbox(previewer.handle.as_ptr()) };
    previewer.require(cleared)?;
    Ok(messages)
}

fn canonical_gmp(path: &str) -> Result<PathBuf, String> {
    let source = Path::new(path);
    if source.extension().and_then(|value| value.to_str()) != Some("gmp") {
        return Err("Device plugin must use the .gmp extension".to_string());
    }
    if !source.is_file() {
        return Err(format!("Device plugin not found: {}", source.display()));
    }
    source
        .canonicalize()
        .map_err(|error| format!("Could not resolve device plugin: {error}"))
}

fn status(previewer: &NativePreviewer) -> PreviewStatus {
    PreviewStatus {
        loaded: unsafe { gm_preview_is_loaded(previewer.handle.as_ptr()) } != 0,
        running: unsafe { gm_preview_is_running(previewer.handle.as_ptr()) } != 0,
        source: previewer
            .source
            .as_ref()
            .map(|path| path.to_string_lossy().into_owned()),
    }
}

fn main() {
    let previewer = NativePreviewer::new().expect("could not initialize GM Plugin Previewer core");
    let web_server = WebPluginServer::new().expect("could not initialize Web plugin server");
    tauri::Builder::default()
        .manage(Mutex::new(previewer))
        .manage(web_server)
        .manage(PackageShareState::default())
        .invoke_handler(tauri::generate_handler![
            discover_web_plugins,
            inspect_web_plugin,
            discover_device_plugins,
            import_device_workspace,
            build_and_share_web_plugin,
            share_device_plugin,
            stop_web_package_share,
            pick_web_package,
            pick_web_directory,
            pick_device_plugin,
            pick_device_directory,
            resolve_web_entry,
            load_device_plugin,
            stop_device_plugin,
            preview_status,
            send_plugin_message,
            simulate_button,
            simulate_gesture,
            tick_frame,
        ])
        .run(tauri::generate_context!())
        .expect("error while running GM Plugin Studio");
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn write_test_mmpkg(package: &Path) {
        let source = package
            .parent()
            .expect("package must have a parent")
            .join("source");
        fs::create_dir_all(&source).expect("source directory must be created");
        fs::write(
            source.join("manifest.json"),
            r#"{"id":"com.memomind.sample","name":"Sample","version":"1.0.0","entry":"web/start.html","bridgeVersion":"1.0","permissions":[]}"#,
        )
        .expect("manifest must be written");
        fs::create_dir_all(source.join("web")).expect("entry directory must be created");
        fs::write(
            source.join("web/start.html"),
            "<!doctype html><title>sample</title>",
        )
        .expect("entry must be written");
        build_mmpkg(&source, package).expect("test package must build");
    }

    #[test]
    fn extracts_manifest_entry_from_mmpkg() {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock must be after epoch")
            .as_nanos();
        let test_directory = std::env::temp_dir().join(format!(
            "gm-plugin-studio-test-{}-{suffix}",
            std::process::id()
        ));
        fs::create_dir_all(&test_directory).expect("test directory must be created");
        let package = test_directory.join("sample.mmpkg");
        write_test_mmpkg(&package);

        let entry = extract_web_package(&package).expect("package must extract");
        assert_eq!(
            fs::read_to_string(&entry).expect("entry must be readable"),
            "<!doctype html><title>sample</title>"
        );
        let cache = entry
            .parent()
            .and_then(Path::parent)
            .expect("entry must have a cache directory");
        fs::remove_dir_all(cache).expect("package cache must be removable");
        fs::remove_dir_all(test_directory).expect("test directory must be removable");
    }

    #[test]
    fn rejects_unsafe_manifest_entry() {
        assert!(safe_relative_entry(Path::new("/tmp/plugin"), "../escape.html").is_err());
        assert!(safe_relative_entry(Path::new("/tmp/plugin"), "/escape.html").is_err());
    }

    #[test]
    fn discovers_source_and_built_web_plugins() {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock must be after epoch")
            .as_nanos();
        let root = std::env::temp_dir().join(format!(
            "gm-plugin-discovery-test-{}-{suffix}",
            std::process::id()
        ));
        for (directory, id, name) in [
            ("plugins/counter", "com.memomind.counter", "Counter"),
            (
                "examples/counter",
                "com.memomind.counter",
                "Counter Example",
            ),
            (
                "plugins/tictactoe/dist",
                "com.memomind.tictactoe",
                "Tic Tac Toe",
            ),
        ] {
            let directory = root.join(directory);
            fs::create_dir_all(&directory).expect("plugin directory must be created");
            fs::write(
                directory.join("manifest.json"),
                format!(
                    r#"{{"id":"{id}","name":"{name}","version":"1.0.0","entry":"index.html"}}"#
                ),
            )
            .expect("manifest must be written");
            fs::write(directory.join("index.html"), "<!doctype html>")
                .expect("entry must be written");
        }

        let plugins = discover_web_plugins_in(&root).expect("plugins must be discovered");
        assert_eq!(plugins.len(), 2);
        assert!(plugins.iter().any(|plugin| {
            plugin.id == "com.memomind.counter" && plugin.path.ends_with("plugins/counter")
        }));
        assert!(plugins.iter().any(|plugin| {
            plugin.id == "com.memomind.tictactoe" && plugin.path.ends_with("tictactoe/dist")
        }));
        fs::remove_dir_all(root).expect("test directory must be removable");
    }

    #[test]
    fn serves_mmpkg_with_size_and_sha256() {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock must be after epoch")
            .as_nanos();
        let root = std::env::temp_dir().join(format!(
            "gm-mmpkg-server-test-{}-{suffix}",
            std::process::id()
        ));
        fs::create_dir_all(&root).expect("test directory must be created");
        let package = root.join("counter.mmpkg");
        write_test_mmpkg(&package);
        let expected = fs::read(&package).expect("package must be readable");

        let (server, port) = start_mmpkg_server(package).expect("server must start");
        let mut client = TcpStream::connect((Ipv4Addr::LOCALHOST, port))
            .expect("client must connect to package server");
        client
            .write_all(MMPKG_REQUEST_LINE)
            .expect("request must be written");
        let mut response = BufReader::new(client);
        let mut header = String::new();
        response
            .read_line(&mut header)
            .expect("response header must be readable");
        assert_eq!(
            header,
            format!(
                "MMPKG/1 OK {} {:x}\n",
                expected.len(),
                Sha256::digest(&expected)
            )
        );
        let mut body = Vec::new();
        response
            .read_to_end(&mut body)
            .expect("package body must be readable");
        assert_eq!(body, expected);
        server.stop();
        fs::remove_dir_all(root).expect("test directory must be removable");
    }

    #[test]
    fn serves_gmp_with_size_and_sha256() {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock must be after epoch")
            .as_nanos();
        let root = std::env::temp_dir().join(format!(
            "gm-gmp-server-test-{}-{suffix}",
            std::process::id()
        ));
        fs::create_dir_all(&root).expect("test directory must be created");
        let package = root.join("minimal.gmp");
        let mut expected = b"GMPK".to_vec();
        expected.resize(MIN_GMP_BYTES as usize, 0);
        fs::write(&package, &expected).expect("package must be written");

        let (server, port) = start_gmp_server(package).expect("server must start");
        let mut client = TcpStream::connect((Ipv4Addr::LOCALHOST, port))
            .expect("client must connect to package server");
        client
            .write_all(GMP_REQUEST_LINE)
            .expect("request must be written");
        let mut response = BufReader::new(client);
        let mut header = String::new();
        response
            .read_line(&mut header)
            .expect("response header must be readable");
        assert_eq!(
            header,
            format!(
                "GMP/1 OK {} {:x}\n",
                expected.len(),
                Sha256::digest(&expected)
            )
        );
        let mut body = Vec::new();
        response
            .read_to_end(&mut body)
            .expect("package body must be readable");
        assert_eq!(body, expected);
        server.stop();
        fs::remove_dir_all(root).expect("test directory must be removable");
    }

    #[test]
    fn builds_mmpkg_with_app_file_hashes() {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock must be after epoch")
            .as_nanos();
        let root = std::env::temp_dir().join(format!(
            "gm-native-mmpkg-test-{}-{suffix}",
            std::process::id()
        ));
        let source = root.join("counter");
        fs::create_dir_all(&source).expect("source directory must be created");
        fs::write(
            source.join("manifest.json"),
            r#"{"id":"com.memomind.counter","name":"Counter","version":"1.0.0","entry":"index.html","bridgeVersion":"1.0","permissions":["display"]}"#,
        )
        .expect("manifest must be written");
        let html = b"<!doctype html><title>Counter</title>";
        fs::write(source.join("index.html"), html).expect("entry must be written");
        let output = root.join("release/counter.mmpkg");

        build_mmpkg(&source, &output).expect("package must build");
        let mut archive =
            zip::ZipArchive::new(File::open(&output).expect("generated package must be readable"))
                .expect("generated package must be a ZIP");
        let mut manifest = String::new();
        archive
            .by_name("manifest.json")
            .expect("package manifest must exist")
            .read_to_string(&mut manifest)
            .expect("package manifest must be readable");
        let manifest: serde_json::Value =
            serde_json::from_str(&manifest).expect("package manifest must be JSON");
        assert_eq!(manifest["schemaVersion"], 1);
        assert_eq!(
            manifest["files"]["index.html"],
            format!("sha256:{:x}", Sha256::digest(html))
        );
        assert_eq!(archive.len(), 2);
        fs::remove_dir_all(root).expect("test directory must be removable");
    }

    #[test]
    fn rejects_mmpkg_payload_hash_mismatch() {
        let html = b"<!doctype html>";
        let manifest = serde_json::json!({
            "schemaVersion": 1,
            "id": "com.memomind.tampered",
            "name": "Tampered",
            "version": "1.0.0",
            "entry": "index.html",
            "bridgeVersion": "1.0",
            "permissions": [],
            "files": { "index.html": "sha256:0000000000000000000000000000000000000000000000000000000000000000" }
        });
        let cursor = Cursor::new(Vec::new());
        let mut archive = zip::ZipWriter::new(cursor);
        let options = zip::write::SimpleFileOptions::default();
        archive.start_file("manifest.json", options).unwrap();
        archive
            .write_all(serde_json::to_string(&manifest).unwrap().as_bytes())
            .unwrap();
        archive.start_file("index.html", options).unwrap();
        archive.write_all(html).unwrap();
        let bytes = archive.finish().unwrap().into_inner();
        assert!(validate_mmpkg_bytes(&bytes)
            .unwrap_err()
            .contains("hash mismatch"));
    }

    #[test]
    fn rejects_mmpkg_duplicate_paths() {
        let cursor = Cursor::new(Vec::new());
        let mut archive = zip::ZipWriter::new(cursor);
        let options = zip::write::SimpleFileOptions::default();
        archive.start_file("manifest.json", options).unwrap();
        archive.write_all(b"{}").unwrap();
        archive.start_file("manifest.json", options).unwrap();
        archive.write_all(b"{}").unwrap();
        let bytes = archive.finish().unwrap().into_inner();
        assert!(validate_mmpkg_bytes(&bytes)
            .unwrap_err()
            .contains("duplicate path"));
    }
}
