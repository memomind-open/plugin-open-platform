#include "previewer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace gmpreview {
namespace {

enum TrapId : unsigned {
    HLog = 1, HMonotonic, HAlloc, HFree, HDisplayInfo,
    HFramebufferLock, HFramebufferUnlock,
    HPowerGet, HPowerSet, HBrightnessGet, HBrightnessSet,
    HDistanceGet, HDistanceSet, HHeightGet, HHeightSet, HAutoBrightness,
    HBtSend, HImuEnable, HImuRead, HBattery, HCharging, HWearing,
    HLocale, HAppExit, HExtensionGet,

    LRootGet = 100, LObjCreate, LObjDelete, LObjClean, LObjSetPos,
    LObjSetSize, LObjAlign, LObjAddFlag, LObjClearFlag, LObjInvalidate,
    LObjGetWidth, LObjGetHeight, LStyleSet, LLabelCreate, LLabelSetText,
    LLabelSetLongMode, LArcCreate, LArcSetRange, LArcSetValue,
    LLineCreate, LLineSetPoints, LFontLineHeight, LTextGetSize,
    LTextGetNextLine, LSelectionStart, LSelectionEnd,

    XLz4Bound = 210, XLz4Compress, XLz4Decompress,
};

constexpr int32_t OK = 0;
constexpr int32_t GM_EINVAL = -1;
constexpr int32_t GM_ENOTSUP = -2;
constexpr int32_t GM_ENOMEM = -4;
constexpr int32_t GM_EIO = -5;
constexpr int32_t GM_ESTATE = -7;
constexpr uint16_t ABI_1_0 = 0x0100;
constexpr uint32_t CAPABILITIES = 0xff;

uint16_t read16(const std::vector<uint8_t> &bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes.at(offset) |
                                 static_cast<uint16_t>(bytes.at(offset + 1)) << 8);
}

uint32_t read32(const std::vector<uint8_t> &bytes, size_t offset)
{
    return static_cast<uint32_t>(bytes.at(offset)) |
           static_cast<uint32_t>(bytes.at(offset + 1)) << 8 |
           static_cast<uint32_t>(bytes.at(offset + 2)) << 16 |
           static_cast<uint32_t>(bytes.at(offset + 3)) << 24;
}

uint32_t crc32Package(const std::vector<uint8_t> &bytes)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < bytes.size(); ++i) {
        uint8_t value = (i >= 24 && i < 28) ? 0 : bytes[i];
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) &
                                static_cast<uint32_t>(-static_cast<int32_t>(crc & 1)));
    }
    return crc ^ UINT32_MAX;
}

std::string hex32(uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

} // namespace

Previewer::Previewer()
    : frame_(kDisplayWidth * kDisplayHeight, 0),
      presented_framebuffer_(320u * kDisplayHeight, 0)
{
    cpu_.setTrapHandler([this](Rv32 &cpu, uint32_t address) {
        return handleTrap(cpu, address);
    });
}

Previewer::~Previewer()
{
    try { unload(); } catch (...) {}
}

void Previewer::setFontData(std::vector<uint8_t> default_font,
                            std::vector<uint8_t> large_font)
{
    auto valid = [](const std::vector<uint8_t> &data) {
        if (data.size() < 16 || data[8] == 0 || data[8] > 4) return false;
        const uint32_t count = static_cast<uint32_t>(data[12]) |
                               static_cast<uint32_t>(data[13]) << 8 |
                               static_cast<uint32_t>(data[14]) << 16 |
                               static_cast<uint32_t>(data[15]) << 24;
        return count > 0 && 16ull + static_cast<uint64_t>(count) * 13ull <= data.size();
    };
    if (!valid(default_font) || !valid(large_font))
        throw std::runtime_error("invalid embedded XGIMI font data");
    font_default_data_ = std::move(default_font);
    font_large_data_ = std::move(large_font);
}

void Previewer::loadFile(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open GMP file: " + path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    loadBytes(bytes, path);
}

void Previewer::loadBytes(const std::vector<uint8_t> &package,
                          const std::string &source_name)
{
    unload();
    logs_.clear();
    bt_outbox_.clear();
    source_name_ = source_name;
    exit_requested_ = false;
    monotonic_ms_ = 0;

    if (package.size() < 28) throw std::runtime_error("GMP package is shorter than its 28-byte header");
    if (std::memcmp(package.data(), "GMPK", 4) != 0)
        throw std::runtime_error("not a GMP package (GMPK magic is missing)");
    package_info_.format_version = read16(package, 4);
    package_info_.abi_version = read16(package, 6);
    package_info_.image_size = read32(package, 8);
    package_info_.memory_size = read32(package, 12);
    package_info_.entry_offset = read32(package, 16);
    package_info_.relocation_count = read32(package, 20);
    package_info_.crc32 = read32(package, 24);

    if (package_info_.format_version != 1)
        throw std::runtime_error("unsupported GMP package version " + std::to_string(package_info_.format_version));
    if ((package_info_.abi_version >> 8) != 1 || package_info_.abi_version > ABI_1_0)
        throw std::runtime_error("plugin requires an incompatible Host ABI");
    if (package_info_.image_size == 0 || package_info_.image_size > 200u * 1024u ||
        package_info_.memory_size < package_info_.image_size ||
        package_info_.memory_size > 200u * 1024u)
        throw std::runtime_error("invalid GMP image or runtime memory size");
    if (package_info_.entry_offset >= package_info_.image_size ||
        (package_info_.entry_offset & 1u))
        throw std::runtime_error("invalid GMP entry offset");
    if (package_info_.relocation_count > UINT32_MAX / 4u)
        throw std::runtime_error("invalid GMP relocation count");
    uint32_t relocation_offset = 28u + package_info_.image_size;
    if (package_info_.relocation_count) relocation_offset = (relocation_offset + 3u) & ~3u;
    const uint64_t relocation_end = static_cast<uint64_t>(relocation_offset) +
                                    static_cast<uint64_t>(package_info_.relocation_count) * 4u;
    const uint64_t expected_size = std::max<uint64_t>(relocation_end, package_info_.memory_size);
    if (package.size() != expected_size)
        throw std::runtime_error("GMP package size does not match its header");
    if (crc32Package(package) != package_info_.crc32)
        throw std::runtime_error("GMP CRC32 check failed");

    initializeMemory(package_info_.memory_size);
    cpu_.memory.write(kPluginBase, package.data() + 28, package_info_.image_size);
    for (uint32_t i = 0; i < package_info_.relocation_count; ++i) {
        const uint32_t offset = read32(package, relocation_offset + i * 4u);
        if ((offset & 3u) || offset > package_info_.image_size - 4u)
            throw std::runtime_error("invalid relocation offset at index " + std::to_string(i));
        const uint32_t value = cpu_.memory.read32(kPluginBase + offset);
        if (value >= package_info_.memory_size)
            throw std::runtime_error("relocation target is outside plugin memory at index " + std::to_string(i));
        cpu_.memory.write32(kPluginBase + offset, kPluginBase + value);
    }
    buildHostTables();
    cpu_.memory.write16(kDescriptor, 40);

    int32_t result;
    try {
        result = cpu_.call(kPluginBase + package_info_.entry_offset,
                           {kHostTable, kDescriptor}, kStackBase + kStackSize);
    } catch (const std::exception &error) {
        throw std::runtime_error(std::string("plugin entry failed: ") + error.what());
    }
    if (result != OK) throw std::runtime_error("plugin entry rejected the preview Host: " + std::to_string(result));
    if (cpu_.memory.read16(kDescriptor) < 40 ||
        cpu_.memory.read16(kDescriptor + 2) != package_info_.abi_version)
        throw std::runtime_error("plugin returned an incompatible lifecycle descriptor");
    context_ = cpu_.memory.read32(kDescriptor + 4);
    for (size_t i = 0; i < 8; ++i) {
        callbacks_[i] = cpu_.memory.read32(kDescriptor + 8 + static_cast<uint32_t>(i) * 4);
        validateCallback(callbacks_[i]);
    }

    if (callbacks_[0]) {
        try {
            result = invokeResult(callbacks_[0], {context_});
            if (result != OK)
                throw std::runtime_error("on_load rejected activation: " + std::to_string(result));
        } catch (...) {
            const std::exception_ptr load_error = std::current_exception();
            try {
                if (callbacks_[7]) invokeVoid(callbacks_[7], {context_});
            } catch (const std::exception &error) {
                appendLog(std::string("[Previewer] on_unload after failed on_load also failed: ") +
                          error.what());
            }
            std::fill(std::begin(callbacks_), std::end(callbacks_), 0);
            context_ = 0;
            allocations_.clear();
            std::rethrow_exception(load_error);
        }
    }
    loaded_ = true;
    appendLog("[Previewer] Loaded " + source_name_ + " (image " +
              std::to_string(package_info_.image_size) + " bytes, runtime " +
              std::to_string(package_info_.memory_size) + " bytes)");
}

void Previewer::initializeMemory(uint32_t plugin_memory_size)
{
    cpu_.memory.clear();
    cpu_.reset();
    cpu_.memory.add(kPluginBase, plugin_memory_size, "plugin image");
    cpu_.memory.add(kStackBase, kStackSize, "plugin stack");
    cpu_.memory.add(kHeapBase, kHeapSize, "simulated Host heap");
    cpu_.memory.add(kSystemBase, 128u * 1024u, "Host ABI tables and event scratch");
    cpu_.memory.add(kFramebufferBase, 320u * kDisplayHeight, "GRAY4 framebuffer");
    allocations_.clear();
    free_blocks_.clear();
    free_blocks_[kHeapBase] = kHeapSize;
    nodes_.clear();
    next_object_ = kObjectBase + 0x104;
    framebuffer_locked_ = false;
    imu_modes_ = 0;
    auto_brightness_blocked_ = false;
    std::fill(frame_.begin(), frame_.end(), 0);
    std::fill(presented_framebuffer_.begin(), presented_framebuffer_.end(), 0);
}

void Previewer::validateCallback(uint32_t callback) const
{
    if (callback == 0) return;
    if ((callback & 1u) || callback < kPluginBase ||
        callback - kPluginBase >= package_info_.image_size)
        throw std::runtime_error("plugin returned an invalid callback address " + hex32(callback));
}

int32_t Previewer::invokeResult(uint32_t callback, const std::vector<uint32_t> &arguments)
{
    if (!callback) return OK;
    return cpu_.call(callback, arguments, kStackBase + kStackSize);
}

void Previewer::invokeVoid(uint32_t callback, const std::vector<uint32_t> &arguments)
{
    if (callback) (void)cpu_.call(callback, arguments, kStackBase + kStackSize);
}

void Previewer::start()
{
    if (!loaded_) throw std::runtime_error("load a GMP package before starting it");
    if (running_) return;
    nodes_.clear();
    Node root;
    root.handle = kRootObject;
    root.type = NodeType::Root;
    root.width = kDisplayWidth;
    root.height = kDisplayHeight;
    root.size_set = true;
    root.flags = 0;
    nodes_[root.handle] = root;
    device_.display_power = true;
    exit_requested_ = false;
    int32_t result = OK;
    try {
        result = callbacks_[1] ? invokeResult(callbacks_[1], {context_}) : OK;
    } catch (...) {
        nodes_.clear();
        throw;
    }
    if (result != OK) {
        nodes_.clear();
        throw std::runtime_error("on_start failed: " + std::to_string(result));
    }
    running_ = true;
    suspended_ = false;
    appendLog("[Previewer] Plugin started");
    honorExitRequest();
}

void Previewer::stop()
{
    if (!running_) return;
    if (callbacks_[6]) invokeVoid(callbacks_[6], {context_});
    running_ = false;
    suspended_ = false;
    imu_modes_ = 0;
    auto_brightness_blocked_ = false;
    framebuffer_locked_ = false;
    nodes_.clear();
    appendLog("[Previewer] Plugin stopped");
}

void Previewer::suspend()
{
    if (!running_ || suspended_) return;
    if (callbacks_[5]) invokeVoid(callbacks_[5], {context_});
    suspended_ = true;
    appendLog("[Previewer] Plugin suspended");
    honorExitRequest();
}

void Previewer::resume()
{
    if (!running_ || !suspended_) return;
    suspended_ = false;
    if (callbacks_[2]) invokeVoid(callbacks_[2], {context_});
    appendLog("[Previewer] Plugin resumed");
    honorExitRequest();
}

void Previewer::unload()
{
    if (running_) stop();
    if (loaded_ && callbacks_[7]) invokeVoid(callbacks_[7], {context_});
    if (loaded_ && !allocations_.empty()) {
        uint64_t bytes = 0;
        for (const auto &allocation : allocations_) bytes += allocation.second;
        appendLog("[Previewer] Host allocation leak: " + std::to_string(allocations_.size()) +
                  " block(s), " + std::to_string(bytes) + " bytes");
    }
    loaded_ = false;
    running_ = false;
    suspended_ = false;
    std::fill(std::begin(callbacks_), std::end(callbacks_), 0);
    context_ = 0;
    nodes_.clear();
    allocations_.clear();
}

void Previewer::tick(uint32_t elapsed_ms)
{
    monotonic_ms_ += elapsed_ms;
    if (running_ && !suspended_ && callbacks_[3]) {
        invokeVoid(callbacks_[3], {context_, elapsed_ms});
        honorExitRequest();
    }
}

void Previewer::honorExitRequest()
{
    if (exit_requested_ && running_) stop();
}

void Previewer::buildHostTables()
{
    auto writeFunction = [this](uint32_t offset, unsigned id) {
        cpu_.memory.write32(kHostTable + offset, trap(id));
    };
    cpu_.memory.write16(kHostTable, 112);
    cpu_.memory.write16(kHostTable + 2, ABI_1_0);
    cpu_.memory.write32(kHostTable + 4, CAPABILITIES);
    writeFunction(8, HLog);
    writeFunction(12, HMonotonic);
    writeFunction(16, HAlloc);
    writeFunction(20, HFree);
    writeFunction(24, HDisplayInfo);
    cpu_.memory.write32(kHostTable + 28, kLvglTable);
    writeFunction(32, HFramebufferLock);
    writeFunction(36, HFramebufferUnlock);
    writeFunction(40, HPowerGet);
    writeFunction(44, HPowerSet);
    writeFunction(48, HBrightnessGet);
    writeFunction(52, HBrightnessSet);
    writeFunction(56, HDistanceGet);
    writeFunction(60, HDistanceSet);
    writeFunction(64, HHeightGet);
    writeFunction(68, HHeightSet);
    writeFunction(72, HAutoBrightness);
    writeFunction(76, HBtSend);
    writeFunction(80, HImuEnable);
    writeFunction(84, HImuRead);
    writeFunction(88, HBattery);
    writeFunction(92, HCharging);
    writeFunction(96, HWearing);
    writeFunction(100, HLocale);
    writeFunction(104, HAppExit);
    writeFunction(108, HExtensionGet);

    cpu_.memory.write16(kLvglTable, 116);
    cpu_.memory.write16(kLvglTable + 2, ABI_1_0);
    for (unsigned index = 0; index <= 20; ++index)
        cpu_.memory.write32(kLvglTable + 4 + index * 4, trap(LRootGet + index));
    cpu_.memory.write32(kLvglTable + 88, kFontDefault);
    cpu_.memory.write32(kLvglTable + 92, kFontLarge);
    for (unsigned index = 0; index < 5; ++index)
        cpu_.memory.write32(kLvglTable + 96 + index * 4, trap(LFontLineHeight + index));

    cpu_.memory.write32(kLz4Extension, trap(XLz4Bound));
    cpu_.memory.write32(kLz4Extension + 4, trap(XLz4Compress));
    cpu_.memory.write32(kLz4Extension + 8, trap(XLz4Decompress));
}

uint32_t Previewer::allocate(uint32_t size)
{
    if (size == 0) return 0;
    size = (size + 7u) & ~7u;
    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        if (it->second < size) continue;
        const uint32_t address = it->first;
        const uint32_t remaining = it->second - size;
        free_blocks_.erase(it);
        if (remaining) free_blocks_[address + size] = remaining;
        allocations_[address] = size;
        return address;
    }
    return 0;
}

void Previewer::release(uint32_t address)
{
    if (!address) return;
    auto it = allocations_.find(address);
    if (it == allocations_.end()) {
        appendLog("[Previewer] Rejected free of untracked pointer " + hex32(address));
        return;
    }
    const uint32_t size = it->second;
    allocations_.erase(it);
    free_blocks_[address] = size;
    auto current = free_blocks_.find(address);
    if (current != free_blocks_.begin()) {
        auto previous = std::prev(current);
        if (previous->first + previous->second == current->first) {
            previous->second += current->second;
            free_blocks_.erase(current);
            current = previous;
        }
    }
    auto next = std::next(current);
    if (next != free_blocks_.end() && current->first + current->second == next->first) {
        current->second += next->second;
        free_blocks_.erase(next);
    }
}

void Previewer::appendLog(const std::string &message)
{
    std::string clean = message;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r')) clean.pop_back();
    logs_.push_back(clean);
    if (logs_.size() > 2000) logs_.erase(logs_.begin(), logs_.begin() + 500);
}

std::string Previewer::formatGuestLog()
{
    const std::string format = cpu_.memory.readString(cpu_.argument(0));
    std::ostringstream out;
    size_t argument_index = 1;
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] != '%') { out << format[i]; continue; }
        if (i + 1 < format.size() && format[i + 1] == '%') { out << '%'; ++i; continue; }
        size_t begin = i++;
        while (i < format.size() && std::strchr("-+ #0", format[i])) ++i;
        int width = 0;
        while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
            width = width * 10 + format[i] - '0';
            ++i;
        }
        if (i < format.size() && format[i] == '.') {
            ++i;
            while (i < format.size() && format[i] >= '0' && format[i] <= '9') ++i;
        }
        while (i < format.size() && std::strchr("hljztL", format[i])) ++i;
        if (i >= format.size()) { out << format.substr(begin); break; }
        const char conversion = format[i];
        const uint32_t value = cpu_.argument(argument_index++);
        if (width) out << std::setw(width) << std::setfill(format[begin + 1] == '0' ? '0' : ' ');
        switch (conversion) {
        case 'd': case 'i': out << static_cast<int32_t>(value); break;
        case 'u': out << value; break;
        case 'x': out << std::hex << std::nouppercase << value << std::dec; break;
        case 'X': out << std::hex << std::uppercase << value << std::nouppercase << std::dec; break;
        case 'p': out << hex32(value); break;
        case 'c': out << static_cast<char>(value); break;
        case 's': out << (value ? cpu_.memory.readString(value) : "(null)"); break;
        default: out << format.substr(begin, i - begin + 1) << "[" << value << "]"; break;
        }
    }
    return out.str();
}

bool Previewer::sendButton(uint16_t action, uint16_t button)
{
    for (uint32_t i = 0; i < 20; ++i) cpu_.memory.write8(kEvent + i, 0);
    cpu_.memory.write16(kEvent + 8, button);
    cpu_.memory.write16(kEvent + 10, action);
    return dispatchEvent(1);
}

bool Previewer::sendGesture(uint16_t gesture, bool active)
{
    for (uint32_t i = 0; i < 20; ++i) cpu_.memory.write8(kEvent + i, 0);
    cpu_.memory.write16(kEvent + 8, gesture);
    cpu_.memory.write8(kEvent + 10, active ? 1 : 0);
    return dispatchEvent(2);
}

bool Previewer::simulateDirectionGesture(uint16_t gesture)
{
    if (gesture != 2 && gesture != 3 && gesture != 7 && gesture != 8)
        return false;

    const bool handled = sendGesture(gesture, true);
    const int16_t saved_gyro[3] = {
        device_.gyro[0], device_.gyro[1], device_.gyro[2]
    };
    auto set_gyro = [this](int16_t x, int16_t y, int16_t z) {
        device_.gyro[0] = x;
        device_.gyro[1] = y;
        device_.gyro[2] = z;
    };
    auto restore_gyro = [this, &saved_gyro]() {
        device_.gyro[0] = saved_gyro[0];
        device_.gyro[1] = saved_gyro[1];
        device_.gyro[2] = saved_gyro[2];
    };

    try {
        // Give raw-direction consumers a centered interval so their detector
        // is armed even when this is the first simulated input after start.
        set_gyro(0, 0, 0);
        for (int index = 0; index < 4; ++index) tick(33);
        switch (gesture) {
        case 2: set_gyro(0, 0, 80); break;
        case 3: set_gyro(0, 0, -80); break;
        case 7: set_gyro(-60, 60, 0); break;
        case 8: set_gyro(60, -60, 0); break;
        default: break;
        }
        tick(33);
        switch (gesture) {
        case 2: set_gyro(0, 0, -30); break;
        case 3: set_gyro(0, 0, 30); break;
        case 7: set_gyro(20, -20, 0); break;
        case 8: set_gyro(-20, 20, 0); break;
        default: break;
        }
        tick(33);
        set_gyro(0, 0, 0);
        for (int index = 0; index < 4; ++index) tick(33);
    } catch (...) {
        restore_gyro();
        throw;
    }
    restore_gyro();
    return handled;
}

bool Previewer::sendBluetooth(uint16_t channel, const std::vector<uint8_t> &payload)
{
    if (payload.size() > kPluginMessageMaxPayloadBytes) {
        throw std::runtime_error("simulated Bluetooth payload is too large");
    }
    for (uint32_t i = 0; i < 20; ++i) cpu_.memory.write8(kEvent + i, 0);
    if (!payload.empty()) cpu_.memory.write(kEventPayload, payload.data(), payload.size());
    cpu_.memory.write16(kEvent + 8, channel);
    cpu_.memory.write16(kEvent + 10, 0);
    cpu_.memory.write32(kEvent + 12, payload.empty() ? 0 : kEventPayload);
    cpu_.memory.write32(kEvent + 16, static_cast<uint32_t>(payload.size()));
    return dispatchEvent(3);
}

bool Previewer::sendConnection(bool connected)
{
    device_.connected = connected;
    for (uint32_t i = 0; i < 20; ++i) cpu_.memory.write8(kEvent + i, 0);
    cpu_.memory.write8(kEvent + 8, connected ? 1 : 0);
    return dispatchEvent(4);
}

bool Previewer::dispatchEvent(uint16_t type)
{
    if (!running_ || suspended_ || !callbacks_[4]) return false;
    cpu_.memory.write16(kEvent, 20);
    cpu_.memory.write16(kEvent + 2, type);
    cpu_.memory.write32(kEvent + 4, monotonic_ms_);
    const bool handled = invokeResult(callbacks_[4], {context_, kEvent}) != 0;
    honorExitRequest();
    return handled;
}

bool Previewer::handleTrap(Rv32 &cpu, uint32_t address)
{
    if (address < kTrapBase || ((address - kTrapBase) & 3u)) return false;
    const unsigned id = (address - kTrapBase) / 4u;
    auto finish = [&cpu](uint32_t result = 0) {
        cpu.setReturn(result);
        cpu.pc = cpu.x[1];
    };
    auto requireLvgl = [this]() {
        if (framebuffer_locked_)
            throw std::runtime_error("plugin called LVGL while a direct framebuffer slice was locked");
    };

    switch (id) {
    case HLog:
        appendLog(formatGuestLog());
        finish();
        return true;
    case HMonotonic: finish(monotonic_ms_); return true;
    case HAlloc: finish(allocate(cpu.argument(0))); return true;
    case HFree: release(cpu.argument(0)); finish(); return true;
    case HDisplayInfo: {
        const uint32_t output = cpu.argument(0);
        if (!output) { finish(static_cast<uint32_t>(GM_EINVAL)); return true; }
        cpu.memory.write16(output, kDisplayWidth);
        cpu.memory.write16(output + 2, kDisplayHeight);
        cpu.memory.write16(output + 4, 30);
        cpu.memory.write8(output + 6, 1); // GRAY_4
        cpu.memory.write8(output + 7, 1);
        finish(OK);
        return true;
    }
    case HFramebufferLock: {
        const uint16_t y = static_cast<uint16_t>(cpu.argument(0));
        const uint32_t output = cpu.argument(1);
        if (!output || y >= kDisplayHeight) { finish(static_cast<uint32_t>(GM_EINVAL)); return true; }
        if (framebuffer_locked_) { finish(static_cast<uint32_t>(GM_ESTATE)); return true; }
        locked_y_ = y < 175 ? 0 : 175;
        locked_height_ = 175;
        framebuffer_locked_ = true;
        cpu.memory.write32(output, kFramebufferBase + static_cast<uint32_t>(locked_y_) * 320u);
        cpu.memory.write16(output + 4, locked_y_);
        cpu.memory.write16(output + 6, kDisplayWidth);
        cpu.memory.write16(output + 8, locked_height_);
        cpu.memory.write16(output + 10, 320);
        finish(OK);
        return true;
    }
    case HFramebufferUnlock: {
        if (!framebuffer_locked_) { finish(static_cast<uint32_t>(GM_ESTATE)); return true; }
        int32_t result = OK;
        const uint32_t dirty = cpu.argument(0);
        if (dirty) {
            const int16_t x = static_cast<int16_t>(cpu.memory.read16(dirty));
            const int16_t y = static_cast<int16_t>(cpu.memory.read16(dirty + 2));
            const uint16_t width = cpu.memory.read16(dirty + 4);
            const uint16_t height = cpu.memory.read16(dirty + 6);
            if (x < 0 || y < locked_y_ || width == 0 || height == 0 ||
                static_cast<uint32_t>(x) + width > kDisplayWidth ||
                static_cast<uint32_t>(y) + height > static_cast<uint32_t>(locked_y_) + locked_height_)
                result = GM_EINVAL;
        }
        if (result == OK && dirty && cpu.argument(1)) {
            const GuestMemory::Region *framebuffer = cpu_.memory.find(kFramebufferBase);
            if (framebuffer && framebuffer->bytes.size() >= presented_framebuffer_.size()) {
                std::copy_n(framebuffer->bytes.begin(), presented_framebuffer_.size(),
                            presented_framebuffer_.begin());
            }
        }
        framebuffer_locked_ = false;
        finish(static_cast<uint32_t>(result));
        return true;
    }
    case HPowerGet: finish(device_.display_power); return true;
    case HPowerSet: device_.display_power = cpu.argument(0) != 0; finish(OK); return true;
    case HBrightnessGet: finish(device_.brightness); return true;
    case HBrightnessSet:
        if (cpu.argument(0) < 1 || cpu.argument(0) > 10) finish(static_cast<uint32_t>(GM_EINVAL));
        else { device_.brightness = static_cast<uint8_t>(cpu.argument(0)); finish(OK); }
        return true;
    case HDistanceGet: finish(device_.distance); return true;
    case HDistanceSet:
        if (cpu.argument(0) > 8) finish(static_cast<uint32_t>(GM_EINVAL));
        else { device_.distance = static_cast<uint8_t>(cpu.argument(0)); finish(OK); }
        return true;
    case HHeightGet: finish(device_.height); return true;
    case HHeightSet:
        if (cpu.argument(0) > 8) finish(static_cast<uint32_t>(GM_EINVAL));
        else { device_.height = static_cast<uint8_t>(cpu.argument(0)); finish(OK); }
        return true;
    case HAutoBrightness: auto_brightness_blocked_ = cpu.argument(0) != 0; finish(OK); return true;
    case HBtSend: {
        const uint16_t channel = static_cast<uint16_t>(cpu.argument(0));
        const uint32_t data = cpu.argument(1), length = cpu.argument(2);
        if (!data || length == 0 || length > kPluginMessageMaxPayloadBytes) {
            finish(static_cast<uint32_t>(GM_EINVAL));
            return true;
        }
        if (!device_.connected) { finish(static_cast<uint32_t>(GM_EIO)); return true; }
        BluetoothMessage message;
        message.channel = channel;
        message.payload.resize(length);
        cpu.memory.read(data, message.payload.data(), length);
        bt_outbox_.push_back(std::move(message));
        appendLog("[Bluetooth out] service=0x0f command=0x29 channel=" +
                  std::to_string(channel) + " bytes=" +
                  std::to_string(length));
        finish(OK);
        return true;
    }
    case HImuEnable:
        if (cpu.argument(0) & ~3u) finish(static_cast<uint32_t>(GM_EINVAL));
        else { imu_modes_ = static_cast<uint8_t>(cpu.argument(0)); finish(OK); }
        return true;
    case HImuRead: {
        const uint32_t output = cpu.argument(0);
        if (!output) { finish(static_cast<uint32_t>(GM_EINVAL)); return true; }
        if ((imu_modes_ & 2u) == 0) { finish(static_cast<uint32_t>(GM_ESTATE)); return true; }
        for (int i = 0; i < 3; ++i) cpu.memory.write16(output + i * 2, static_cast<uint16_t>(device_.accel[i]));
        for (int i = 0; i < 3; ++i) cpu.memory.write16(output + 6 + i * 2, static_cast<uint16_t>(device_.gyro[i]));
        cpu.memory.write16(output + 12, static_cast<uint16_t>(device_.temperature));
        cpu.memory.write16(output + 14, static_cast<uint16_t>(device_.pitch));
        finish(OK);
        return true;
    }
    case HBattery: finish(device_.battery_percent); return true;
    case HCharging: finish(device_.charging); return true;
    case HWearing: finish(device_.wearing); return true;
    case HLocale: {
        const uint32_t output = cpu.argument(0);
        if (!output) { finish(static_cast<uint32_t>(GM_EINVAL)); return true; }
        if (device_.locale.size() >= 16) {
            cpu.memory.write8(output, 0);
            finish(static_cast<uint32_t>(GM_ENOMEM));
        } else {
            cpu.memory.write(output, device_.locale.c_str(), device_.locale.size() + 1);
            finish(OK);
        }
        return true;
    }
    case HAppExit:
        exit_requested_ = true;
        appendLog("[Previewer] Plugin requested app exit");
        finish();
        return true;
    case HExtensionGet: {
        const uint32_t extension_id = cpu.argument(0), output = cpu.argument(1);
        if (!output) { finish(static_cast<uint32_t>(GM_EINVAL)); return true; }
        uint32_t table = 0;
        if (extension_id == 2) table = kLz4Extension;
        cpu.memory.write32(output, table);
        finish(table ? OK : static_cast<uint32_t>(GM_ENOTSUP));
        return true;
    }

    case LRootGet:
        requireLvgl();
        finish(running_ || nodes_.count(kRootObject) ? kRootObject : 0);
        return true;
    case LObjCreate: requireLvgl(); finish(createNode(NodeType::Object, cpu.argument(0))); return true;
    case LObjDelete: requireLvgl(); deleteNode(cpu.argument(0)); finish(); return true;
    case LObjClean: requireLvgl(); cleanNode(cpu.argument(0)); finish(); return true;
    case LObjSetPos: {
        requireLvgl();
        if (Node *object = node(cpu.argument(0))) {
            object->x = static_cast<int32_t>(cpu.argument(1));
            object->y = static_cast<int32_t>(cpu.argument(2));
            object->alignment = 0;
        }
        finish(); return true;
    }
    case LObjSetSize: {
        requireLvgl();
        if (Node *object = node(cpu.argument(0))) {
            object->width = std::max(0, static_cast<int32_t>(cpu.argument(1)));
            object->height = std::max(0, static_cast<int32_t>(cpu.argument(2)));
            object->size_set = true;
        }
        finish(); return true;
    }
    case LObjAlign: {
        requireLvgl();
        if (Node *object = node(cpu.argument(0))) {
            object->alignment = static_cast<uint8_t>(cpu.argument(1));
            object->align_x = static_cast<int32_t>(cpu.argument(2));
            object->align_y = static_cast<int32_t>(cpu.argument(3));
        }
        finish(); return true;
    }
    case LObjAddFlag: requireLvgl(); if (Node *o = node(cpu.argument(0))) o->flags |= cpu.argument(1); finish(); return true;
    case LObjClearFlag: requireLvgl(); if (Node *o = node(cpu.argument(0))) o->flags &= ~cpu.argument(1); finish(); return true;
    case LObjInvalidate: requireLvgl(); finish(); return true;
    case LObjGetWidth: {
        requireLvgl();
        int x, y, w = 0, h;
        if (const Node *o = node(cpu.argument(0))) absoluteBox(*o, x, y, w, h);
        finish(static_cast<uint32_t>(w)); return true;
    }
    case LObjGetHeight: {
        requireLvgl();
        int x, y, w, h = 0;
        if (const Node *o = node(cpu.argument(0))) absoluteBox(*o, x, y, w, h);
        finish(static_cast<uint32_t>(h)); return true;
    }
    case LStyleSet: {
        requireLvgl();
        if (Node *o = node(cpu.argument(0))) {
            const uint64_t key = static_cast<uint64_t>(cpu.argument(1)) << 32 | cpu.argument(3);
            o->styles[key] = cpu.argument(2);
        }
        finish(); return true;
    }
    case LLabelCreate: requireLvgl(); finish(createNode(NodeType::Label, cpu.argument(0))); return true;
    case LLabelSetText:
        requireLvgl();
        if (Node *o = node(cpu.argument(0))) o->text = cpu.argument(1) ? cpu.memory.readString(cpu.argument(1)) : "";
        finish(); return true;
    case LLabelSetLongMode:
        requireLvgl(); if (Node *o = node(cpu.argument(0))) o->label_mode = static_cast<uint8_t>(cpu.argument(1)); finish(); return true;
    case LArcCreate: requireLvgl(); finish(createNode(NodeType::Arc, cpu.argument(0))); return true;
    case LArcSetRange:
        requireLvgl(); if (Node *o = node(cpu.argument(0))) { o->arc_min = static_cast<int16_t>(cpu.argument(1)); o->arc_max = static_cast<int16_t>(cpu.argument(2)); } finish(); return true;
    case LArcSetValue:
        requireLvgl(); if (Node *o = node(cpu.argument(0))) o->arc_value = static_cast<int16_t>(cpu.argument(1)); finish(); return true;
    case LLineCreate: requireLvgl(); finish(createNode(NodeType::Line, cpu.argument(0))); return true;
    case LLineSetPoints:
        requireLvgl(); if (Node *o = node(cpu.argument(0))) { o->line_points = cpu.argument(1); o->line_point_count = static_cast<uint16_t>(cpu.argument(2)); } finish(); return true;
    case LFontLineHeight: finish(fontHeight(cpu.argument(0))); return true;
    case LTextGetSize: {
        const uint32_t output = cpu.argument(0);
        const std::string text = cpu.argument(1) ? cpu.memory.readString(cpu.argument(1)) : "";
        const int height = fontHeight(cpu.argument(2));
        const int letter = static_cast<int32_t>(cpu.argument(3));
        const int line_space = static_cast<int32_t>(cpu.argument(4));
        const int max_width = static_cast<int32_t>(cpu.argument(5));
        int lines = 1, widest = 0;
        size_t position = 0;
        while (position < text.size()) {
            int used = 0;
            uint32_t next = utf8NextLine(text.substr(position), height, letter, max_width, &used);
            widest = std::max(widest, used);
            position += std::max<uint32_t>(next, 1);
            if (position < text.size()) ++lines;
        }
        if (output) { cpu.memory.write32(output, widest); cpu.memory.write32(output + 4, lines * height + (lines - 1) * line_space); }
        finish(); return true;
    }
    case LTextGetNextLine: {
        const std::string text = cpu.argument(0) ? cpu.memory.readString(cpu.argument(0)) : "";
        int used = 0;
        const uint32_t next = utf8NextLine(text, fontHeight(cpu.argument(1)),
                                           static_cast<int32_t>(cpu.argument(2)),
                                           static_cast<int32_t>(cpu.argument(3)), &used);
        if (cpu.argument(4)) cpu.memory.write32(cpu.argument(4), static_cast<uint32_t>(used));
        finish(next); return true;
    }
    case LSelectionStart: requireLvgl(); if (Node *o = node(cpu.argument(0))) o->selection_start = cpu.argument(1); finish(); return true;
    case LSelectionEnd: requireLvgl(); if (Node *o = node(cpu.argument(0))) o->selection_end = cpu.argument(1); finish(); return true;

    case XLz4Bound: {
        const int32_t size = static_cast<int32_t>(cpu.argument(0));
        finish(size > 0 && size <= 0x7e000000 ? static_cast<uint32_t>(size + size / 255 + 16) : 0);
        return true;
    }
    case XLz4Compress:
        finish(static_cast<uint32_t>(lz4Compress(cpu.argument(0), cpu.argument(1),
                                                 static_cast<int32_t>(cpu.argument(2)),
                                                 static_cast<int32_t>(cpu.argument(3)))));
        return true;
    case XLz4Decompress:
        finish(static_cast<uint32_t>(lz4Decompress(cpu.argument(0), cpu.argument(1),
                                                   static_cast<int32_t>(cpu.argument(2)),
                                                   static_cast<int32_t>(cpu.argument(3)))));
        return true;
    default:
        return false;
    }
}

Previewer::Node *Previewer::node(uint32_t handle)
{
    auto found = nodes_.find(handle);
    return found == nodes_.end() ? nullptr : &found->second;
}

const Previewer::Node *Previewer::node(uint32_t handle) const
{
    auto found = nodes_.find(handle);
    return found == nodes_.end() ? nullptr : &found->second;
}

uint32_t Previewer::createNode(NodeType type, uint32_t parent)
{
    if (!node(parent)) return 0;
    Node object;
    object.handle = next_object_;
    next_object_ += 4;
    object.parent = parent;
    object.type = type;
    if (type == NodeType::Label) {
        object.width = 0;
        object.height = 0;
    } else if (type == NodeType::Arc) {
        object.width = 100;
        object.height = 100;
    } else if (type == NodeType::Line) {
        object.width = 0;
        object.height = 0;
    }
    nodes_[object.handle] = object;
    return object.handle;
}

void Previewer::deleteNode(uint32_t handle, bool allow_root)
{
    if (handle == kRootObject && !allow_root) {
        appendLog("[Previewer] Ignored attempt to delete the Host-owned LVGL root");
        return;
    }
    if (!node(handle)) return;
    std::vector<uint32_t> children;
    for (const auto &entry : nodes_)
        if (entry.second.parent == handle) children.push_back(entry.first);
    for (uint32_t child : children) deleteNode(child, true);
    nodes_.erase(handle);
}

void Previewer::cleanNode(uint32_t handle)
{
    std::vector<uint32_t> children;
    for (const auto &entry : nodes_)
        if (entry.second.parent == handle) children.push_back(entry.first);
    for (uint32_t child : children) deleteNode(child, true);
}

uint32_t Previewer::style(const Node &object, uint32_t property,
                          uint32_t selector, uint32_t fallback) const
{
    const uint64_t exact = static_cast<uint64_t>(property) << 32 | selector;
    auto found = object.styles.find(exact);
    if (found != object.styles.end()) return found->second;
    const uint64_t main = static_cast<uint64_t>(property) << 32;
    found = object.styles.find(main);
    return found == object.styles.end() ? fallback : found->second;
}

int Previewer::fontHeight(uint32_t font) const
{
    // These are the real LVGL line heights from lv_font_xgimi_17/20.
    // The 17/20 suffix is the font family level, not its pixel line height.
    return font == kFontLarge ? 41 : 34;
}

int Previewer::fontBaseline(uint32_t font) const
{
    return font == kFontLarge ? 9 : 7;
}

const std::vector<uint8_t> &Previewer::fontData(int line_height) const
{
    return line_height >= 40 ? font_large_data_ : font_default_data_;
}

bool Previewer::findFontGlyph(int line_height, uint32_t codepoint,
                              FontGlyph &glyph) const
{
    const std::vector<uint8_t> &data = fontData(line_height);
    if (data.size() < 16) return false;
    const uint32_t count = read32(data, 12);
    const uint64_t descriptor_offset = 16ull + static_cast<uint64_t>(count) * 4ull;
    const uint64_t bitmap_offset = descriptor_offset + static_cast<uint64_t>(count) * 9ull;
    if (count == 0 || bitmap_offset > data.size()) return false;

    uint32_t left = 0, right = count;
    while (left < right) {
        const uint32_t middle = left + (right - left) / 2;
        const uint32_t value = read32(data, 16u + static_cast<size_t>(middle) * 4u);
        if (value < codepoint) left = middle + 1;
        else right = middle;
    }
    if (left >= count || read32(data, 16u + static_cast<size_t>(left) * 4u) != codepoint)
        return false;

    const size_t descriptor = static_cast<size_t>(descriptor_offset) +
                              static_cast<size_t>(left) * 9u;
    const uint32_t bitmap_index = read32(data, descriptor);
    uint32_t next_bitmap_index;
    if (left + 1 < count) next_bitmap_index = read32(data, descriptor + 9u);
    else next_bitmap_index = static_cast<uint32_t>(data.size() - bitmap_offset);
    if (next_bitmap_index < bitmap_index ||
        bitmap_offset + next_bitmap_index > data.size()) return false;

    glyph.advance = data[descriptor + 4];
    glyph.box_height = data[descriptor + 5];
    glyph.box_width = data[descriptor + 6];
    glyph.offset_x = static_cast<int8_t>(data[descriptor + 7]);
    glyph.offset_y = static_cast<int8_t>(data[descriptor + 8]);
    glyph.bpp = data[8];
    glyph.bitmap = data.data() + static_cast<size_t>(bitmap_offset) + bitmap_index;
    glyph.bitmap_size = next_bitmap_index - bitmap_index;
    const size_t required_bits = static_cast<size_t>(glyph.box_width) *
                                 glyph.box_height * glyph.bpp;
    return glyph.bpp != 0 && glyph.bitmap_size * 8u >= required_bits;
}

uint32_t Previewer::decodeUtf8(const std::string &text, size_t &offset) const
{
    if (offset >= text.size()) return 0;
    const uint8_t first = static_cast<uint8_t>(text[offset++]);
    if (first < 0x80) return first;
    unsigned continuation_count;
    uint32_t codepoint;
    if ((first & 0xe0) == 0xc0) { continuation_count = 1; codepoint = first & 0x1f; }
    else if ((first & 0xf0) == 0xe0) { continuation_count = 2; codepoint = first & 0x0f; }
    else if ((first & 0xf8) == 0xf0) { continuation_count = 3; codepoint = first & 0x07; }
    else return 0xfffd;
    for (unsigned i = 0; i < continuation_count; ++i) {
        if (offset >= text.size()) return 0xfffd;
        const uint8_t next = static_cast<uint8_t>(text[offset]);
        if ((next & 0xc0) != 0x80) return 0xfffd;
        ++offset;
        codepoint = codepoint << 6 | (next & 0x3f);
    }
    return codepoint;
}

int Previewer::textWidth(const std::string &text, int font_height, int letter_space) const
{
    int width = 0, widest = 0;
    for (size_t i = 0; i < text.size();) {
        const uint32_t codepoint = decodeUtf8(text, i);
        if (codepoint == '\n') { widest = std::max(widest, width); width = 0; continue; }
        FontGlyph exact;
        const int glyph = findFontGlyph(font_height, codepoint, exact) ? exact.advance :
                          (codepoint < 0x80 ? (font_height >= 40 ? 18 : 14)
                                            : (font_height >= 40 ? 29 : 23));
        if (width) width += letter_space;
        width += glyph;
    }
    return std::max(widest, width);
}

uint32_t Previewer::utf8NextLine(const std::string &text, int font_height,
                                 int letter_space, int max_width, int *used_width) const
{
    int width = 0;
    size_t i = 0;
    if (max_width <= 0) max_width = std::numeric_limits<int>::max();
    while (i < text.size()) {
        const size_t character_start = i;
        const uint32_t codepoint = decodeUtf8(text, i);
        if (codepoint == '\n') break;
        FontGlyph exact;
        const int glyph = findFontGlyph(font_height, codepoint, exact) ? exact.advance :
                          (codepoint < 0x80 ? (font_height >= 40 ? 18 : 14)
                                            : (font_height >= 40 ? 29 : 23));
        const int next_width = width + (width ? letter_space : 0) + glyph;
        if (next_width > max_width && character_start != 0) {
            i = character_start;
            break;
        }
        width = next_width;
    }
    if (used_width) *used_width = width;
    return static_cast<uint32_t>(i);
}

void Previewer::absoluteBox(const Node &object, int &x, int &y,
                            int &width, int &height) const
{
    width = static_cast<int32_t>(style(object, 1, 0, static_cast<uint32_t>(object.width)));
    height = static_cast<int32_t>(style(object, 4, 0, static_cast<uint32_t>(object.height)));
    if (object.type == NodeType::Label && !object.size_set &&
        object.styles.find(static_cast<uint64_t>(1) << 32) == object.styles.end()) {
        const int font = fontHeight(style(object, 87, 0, kFontDefault));
        width = textWidth(object.text, font,
                          static_cast<int32_t>(style(object, 88, 0, 0)));
        height = font;
    }
    width = std::max(0, width);
    height = std::max(0, height);
    if (object.type == NodeType::Root || object.parent == 0) {
        x = object.x;
        y = object.y;
        return;
    }
    const Node *parent = node(object.parent);
    int px = 0, py = 0, pw = kDisplayWidth, ph = kDisplayHeight;
    if (parent) absoluteBox(*parent, px, py, pw, ph);
    const int own_x = static_cast<int32_t>(style(object, 7, 0, static_cast<uint32_t>(object.x)));
    const int own_y = static_cast<int32_t>(style(object, 8, 0, static_cast<uint32_t>(object.y)));
    const uint8_t alignment = static_cast<uint8_t>(style(object, 9, 0, object.alignment));
    if (!alignment) {
        x = px + own_x;
        y = py + own_y;
        return;
    }
    int ax = px, ay = py;
    switch (alignment) {
    case 1: ax = px; ay = py; break;
    case 2: ax = px + (pw - width) / 2; ay = py; break;
    case 3: ax = px + pw - width; ay = py; break;
    case 4: ax = px; ay = py + ph - height; break;
    case 5: ax = px + (pw - width) / 2; ay = py + ph - height; break;
    case 6: ax = px + pw - width; ay = py + ph - height; break;
    case 7: ax = px; ay = py + (ph - height) / 2; break;
    case 8: ax = px + pw - width; ay = py + (ph - height) / 2; break;
    case 9: ax = px + (pw - width) / 2; ay = py + (ph - height) / 2; break;
    default: break;
    }
    x = ax + object.align_x + (own_x - object.x);
    y = ay + object.align_y + (own_y - object.y);
}

uint8_t Previewer::nativeColorToGray(uint32_t color) const
{
    return static_cast<uint8_t>(color & 0xff);
}

void Previewer::fillRect(int x, int y, int width, int height,
                         uint8_t gray, uint8_t opacity)
{
    const int left = std::max(0, x), top = std::max(0, y);
    const int right = std::min(kDisplayWidth, x + width);
    const int bottom = std::min(kDisplayHeight, y + height);
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            uint8_t &pixel = frame_[py * kDisplayWidth + px];
            pixel = static_cast<uint8_t>((static_cast<unsigned>(pixel) * (255 - opacity) +
                                          static_cast<unsigned>(gray) * opacity) / 255);
        }
    }
}

void Previewer::strokeRect(int x, int y, int width, int height,
                           int stroke, uint8_t gray)
{
    if (stroke <= 0) return;
    fillRect(x, y, width, stroke, gray);
    fillRect(x, y + height - stroke, width, stroke, gray);
    fillRect(x, y, stroke, height, gray);
    fillRect(x + width - stroke, y, stroke, height, gray);
}

void Previewer::drawLine(int x0, int y0, int x1, int y1, int width, uint8_t gray)
{
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    width = std::max(1, width);
    while (true) {
        fillRect(x0 - width / 2, y0 - width / 2, width, width, gray);
        if (x0 == x1 && y0 == y1) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void Previewer::drawArc(int cx, int cy, int radius, int width,
                        double start, double end, uint8_t gray)
{
    width = std::max(1, width);
    const int steps = std::max(16, static_cast<int>(std::abs(end - start) * radius / 25.0));
    int previous_x = cx + static_cast<int>(std::cos(start) * radius);
    int previous_y = cy + static_cast<int>(std::sin(start) * radius);
    for (int i = 1; i <= steps; ++i) {
        const double angle = start + (end - start) * i / steps;
        const int x = cx + static_cast<int>(std::cos(angle) * radius);
        const int y = cy + static_cast<int>(std::sin(angle) * radius);
        drawLine(previous_x, previous_y, x, y, width, gray);
        previous_x = x;
        previous_y = y;
    }
}

void Previewer::drawExactText(int x, int y, int width, int height,
                              const std::string &text, uint32_t font,
                              int letter_space, int line_space, int alignment,
                              bool wrap, uint8_t gray, uint8_t opacity)
{
    const int line_height = fontHeight(font);
    const int baseline = fontBaseline(font);
    size_t offset = 0;
    int line_y = y;
    while (offset < text.size() && line_y < y + height) {
        const std::string remaining = text.substr(offset);
        int measured_width = 0;
        uint32_t consumed = utf8NextLine(remaining, line_height, letter_space,
                                         wrap ? width : std::numeric_limits<int>::max(),
                                         &measured_width);
        if (consumed == 0) break;
        std::string line = remaining.substr(0, consumed);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        measured_width = textWidth(line, line_height, letter_space);
        int line_x = x;
        if (alignment == 2) line_x += (width - measured_width) / 2;
        else if (alignment == 3) line_x += width - measured_width;

        int cursor = 0;
        size_t character = 0;
        bool first = true;
        while (character < line.size()) {
            const uint32_t codepoint = decodeUtf8(line, character);
            if (!first) cursor += letter_space;
            first = false;
            FontGlyph glyph;
            if (!findFontGlyph(line_height, codepoint, glyph)) {
                cursor += codepoint < 0x80 ? (line_height >= 40 ? 18 : 14)
                                           : (line_height >= 40 ? 29 : 23);
                continue;
            }
            const int glyph_x = line_x + cursor + glyph.offset_x;
            const int glyph_y = line_y + (line_height - baseline) -
                                glyph.box_height - glyph.offset_y;
            const unsigned mask = (1u << glyph.bpp) - 1u;
            for (unsigned gy = 0; gy < glyph.box_height; ++gy) {
                const int pixel_y = glyph_y + static_cast<int>(gy);
                if (pixel_y < y || pixel_y >= y + height ||
                    pixel_y < 0 || pixel_y >= kDisplayHeight) continue;
                for (unsigned gx = 0; gx < glyph.box_width; ++gx) {
                    const int pixel_x = glyph_x + static_cast<int>(gx);
                    if (pixel_x < x || pixel_x >= x + width ||
                        pixel_x < 0 || pixel_x >= kDisplayWidth) continue;
                    const size_t pixel_index = static_cast<size_t>(gy) * glyph.box_width + gx;
                    const size_t bit_index = pixel_index * glyph.bpp;
                    const size_t byte_index = bit_index / 8u;
                    if (byte_index >= glyph.bitmap_size) continue;
                    const unsigned shift = 8u - glyph.bpp - static_cast<unsigned>(bit_index % 8u);
                    const unsigned coverage = (glyph.bitmap[byte_index] >> shift) & mask;
                    if (!coverage) continue;
                    const uint8_t alpha = static_cast<uint8_t>(coverage * opacity / mask);
                    fillRect(pixel_x, pixel_y, 1, 1, gray, alpha);
                }
            }
            cursor += glyph.advance;
        }
        offset += consumed;
        line_y += line_height + line_space;
    }
}

void Previewer::renderNode(const Node &object)
{
    if (object.flags & 1u) return; // hidden
    int x, y, width, height;
    absoluteBox(object, x, y, width, height);
    const uint8_t overall_opacity = static_cast<uint8_t>(style(object, 96, 0, 255));
    const bool has_box = object.type == NodeType::Object || object.type == NodeType::Label;
    const int border = has_box
        ? std::max(0, static_cast<int32_t>(style(object, 50, 0, 0)))
        : 0;
    const bool border_visible = border > 0 && style(object, 49, 0, 255);
    const bool border_post = style(object, 52, 0, 0) != 0;

    if (has_box) {
        uint8_t bg_opacity = static_cast<uint8_t>(style(object, 33, 0, 0));
        bg_opacity = static_cast<uint8_t>(static_cast<unsigned>(bg_opacity) * overall_opacity / 255);
        if (bg_opacity) fillRect(x, y, width, height,
                                 nativeColorToGray(style(object, 32, 0, 0)), bg_opacity);
        if (border_visible && !border_post)
            strokeRect(x, y, width, height, border,
                       nativeColorToGray(style(object, 48, 0, 255)));
        const int outline = std::max(0, static_cast<int32_t>(style(object, 53, 0, 0)));
        if (outline && style(object, 55, 0, 255))
            strokeRect(x - outline, y - outline, width + outline * 2,
                       height + outline * 2, outline,
                       nativeColorToGray(style(object, 54, 0, 128)));
    }

    if (object.type == NodeType::Label && !object.text.empty()) {
        const int pad_left = static_cast<int32_t>(style(object, 18, 0, 0));
        const int pad_right = static_cast<int32_t>(style(object, 19, 0, 0));
        const int pad_top = static_cast<int32_t>(style(object, 16, 0, 0));
        const int pad_bottom = static_cast<int32_t>(style(object, 17, 0, 0));
        const int text_x = x + pad_left;
        const int text_y = y + pad_top;
        const int text_width = std::max(0, width - pad_left - pad_right);
        const int text_height = std::max(0, height - pad_top - pad_bottom);
        const uint32_t font = style(object, 87, 0, kFontDefault);
        const int alignment = static_cast<int>(style(object, 91, 0, 1));
        const uint8_t text_gray = nativeColorToGray(style(object, 85, 0, 255));
        const uint8_t text_opacity = static_cast<uint8_t>(
            static_cast<unsigned>(style(object, 86, 0, 255)) * overall_opacity / 255u);
        if (!fontData(fontHeight(font)).empty()) {
            drawExactText(text_x, text_y, text_width, text_height, object.text,
                          font, static_cast<int32_t>(style(object, 88, 0, 0)),
                          static_cast<int32_t>(style(object, 89, 0, 0)),
                          alignment, object.label_mode == 0,
                          text_gray, text_opacity);
        } else {
            TextOverlay overlay;
            overlay.x = text_x;
            overlay.y = text_y;
            overlay.width = text_width;
            overlay.height = text_height;
            overlay.font_height = fontHeight(font);
            overlay.alignment = alignment;
            overlay.letter_space = static_cast<int32_t>(style(object, 88, 0, 0));
            overlay.line_space = static_cast<int32_t>(style(object, 89, 0, 0));
            overlay.gray = text_gray;
            overlay.opacity = text_opacity;
            overlay.wrap = object.label_mode == 0;
            overlay.utf8 = object.text;
            text_overlays_.push_back(std::move(overlay));
        }
    } else if (object.type == NodeType::Arc) {
        const int radius = std::max(1, std::min(width, height) / 2 - 8);
        const int arc_width = std::max(1, static_cast<int32_t>(style(object, 80, 0, 8)));
        constexpr double pi = 3.14159265358979323846;
        const double start = 135.0 * pi / 180.0;
        const double end = 405.0 * pi / 180.0;
        drawArc(x + width / 2, y + height / 2, radius, arc_width, start, end,
                nativeColorToGray(style(object, 82, 0, 80)));
        double fraction = 0;
        if (object.arc_max > object.arc_min)
            fraction = static_cast<double>(object.arc_value - object.arc_min) /
                       (object.arc_max - object.arc_min);
        fraction = std::max(0.0, std::min(1.0, fraction));
        const uint32_t indicator = 0x020000;
        drawArc(x + width / 2, y + height / 2, radius,
                std::max(1, static_cast<int32_t>(style(object, 80, indicator, arc_width))),
                start, start + (end - start) * fraction,
                nativeColorToGray(style(object, 82, indicator, 255)));
    } else if (object.type == NodeType::Line && object.line_points && object.line_point_count >= 2) {
        int parent_x = 0, parent_y = 0, parent_w = 0, parent_h = 0;
        if (const Node *parent = node(object.parent)) absoluteBox(*parent, parent_x, parent_y, parent_w, parent_h);
        int32_t previous_x = static_cast<int32_t>(cpu_.memory.read32(object.line_points));
        int32_t previous_y = static_cast<int32_t>(cpu_.memory.read32(object.line_points + 4));
        const int line_width = std::max(1, static_cast<int32_t>(style(object, 73, 0, 1)));
        const uint8_t color = nativeColorToGray(style(object, 77, 0, 255));
        for (uint16_t i = 1; i < object.line_point_count; ++i) {
            const int32_t next_x = static_cast<int32_t>(cpu_.memory.read32(object.line_points + i * 8u));
            const int32_t next_y = static_cast<int32_t>(cpu_.memory.read32(object.line_points + i * 8u + 4));
            drawLine(parent_x + previous_x, parent_y + previous_y,
                     parent_x + next_x, parent_y + next_y, line_width, color);
            previous_x = next_x;
            previous_y = next_y;
        }
    }

    for (const auto &entry : nodes_)
        if (entry.second.parent == object.handle) renderNode(entry.second);

    if (border_visible && border_post)
        strokeRect(x, y, width, height, border,
                   nativeColorToGray(style(object, 48, 0, 255)));
}

const std::vector<uint8_t> &Previewer::renderFrame()
{
    text_overlays_.clear();
    if (!device_.display_power) {
        std::fill(frame_.begin(), frame_.end(), 0);
        return frame_;
    }
    for (int y = 0; y < kDisplayHeight; ++y) {
        for (int x = 0; x < kDisplayWidth; ++x) {
            const uint8_t packed = presented_framebuffer_[
                static_cast<size_t>(y) * 320u + static_cast<size_t>(x / 2)];
            const uint8_t nibble = (x & 1) ? (packed & 0x0f) : (packed >> 4);
            frame_[y * kDisplayWidth + x] = static_cast<uint8_t>(nibble * 17);
        }
    }
    if (const Node *root = node(kRootObject)) {
        for (const auto &entry : nodes_)
            if (entry.second.parent == root->handle) renderNode(entry.second);
    }
    return frame_;
}

size_t Previewer::objectCount() const
{
    return nodes_.size() - (nodes_.count(kRootObject) ? 1 : 0);
}

void Previewer::savePgm(const std::string &path)
{
    const std::vector<uint8_t> &pixels = renderFrame();
    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot create screenshot: " + path);
    stream << "P5\n" << kDisplayWidth << " " << kDisplayHeight << "\n255\n";
    stream.write(reinterpret_cast<const char *>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
}

int32_t Previewer::lz4Compress(uint32_t source, uint32_t destination,
                               int32_t source_size, int32_t capacity)
{
    if (!source || !destination || source_size <= 0 || capacity <= 0) return 0;
    std::vector<uint8_t> output;
    output.reserve(static_cast<size_t>(source_size) + 16);
    output.push_back(static_cast<uint8_t>(std::min(source_size, 15) << 4));
    if (source_size >= 15) {
        int32_t remaining = source_size - 15;
        while (remaining >= 255) { output.push_back(255); remaining -= 255; }
        output.push_back(static_cast<uint8_t>(remaining));
    }
    for (int32_t i = 0; i < source_size; ++i) output.push_back(cpu_.memory.read8(source + i));
    if (static_cast<int32_t>(output.size()) > capacity) return 0;
    cpu_.memory.write(destination, output.data(), output.size());
    return static_cast<int32_t>(output.size());
}

int32_t Previewer::lz4Decompress(uint32_t source, uint32_t destination,
                                 int32_t compressed_size, int32_t capacity)
{
    if (!source || !destination || compressed_size < 0 || capacity < 0) return -1;
    int32_t input = 0, output = 0;
    auto byte = [&](int32_t offset) -> uint8_t {
        if (offset < 0 || offset >= compressed_size) throw std::runtime_error("malformed LZ4 block");
        return cpu_.memory.read8(source + static_cast<uint32_t>(offset));
    };
    try {
        while (input < compressed_size) {
            const uint8_t token = byte(input++);
            int32_t literal_length = token >> 4;
            if (literal_length == 15) {
                uint8_t extra;
                do { extra = byte(input++); literal_length += extra; } while (extra == 255);
            }
            if (literal_length > compressed_size - input || literal_length > capacity - output) return -1;
            for (int32_t i = 0; i < literal_length; ++i)
                cpu_.memory.write8(destination + output++, byte(input++));
            if (input == compressed_size) break;
            if (compressed_size - input < 2) return -1;
            const uint32_t offset = byte(input) | static_cast<uint32_t>(byte(input + 1)) << 8;
            input += 2;
            if (offset == 0 || offset > static_cast<uint32_t>(output)) return -1;
            int32_t match_length = token & 0x0f;
            if (match_length == 15) {
                uint8_t extra;
                do { extra = byte(input++); match_length += extra; } while (extra == 255);
            }
            match_length += 4;
            if (match_length > capacity - output) return -1;
            for (int32_t i = 0; i < match_length; ++i) {
                const uint8_t value = cpu_.memory.read8(destination + output - offset);
                cpu_.memory.write8(destination + output++, value);
            }
        }
    } catch (...) { return -1; }
    return output;
}

} // namespace gmpreview
