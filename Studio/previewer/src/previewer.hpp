#pragma once

#include "rv32.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gmpreview {

inline constexpr uint8_t kPluginServiceId = 0x0f;
inline constexpr uint8_t kPluginCommandPhoneToGlasses = 0x28;
inline constexpr uint8_t kPluginCommandGlassesToPhone = 0x29;

struct PackageInfo {
    uint16_t format_version = 0;
    uint16_t abi_version = 0;
    uint32_t image_size = 0;
    uint32_t memory_size = 0;
    uint32_t entry_offset = 0;
    uint32_t relocation_count = 0;
    uint32_t crc32 = 0;
};

struct DeviceState {
    uint8_t battery_percent = 86;
    bool charging = false;
    bool wearing = true;
    bool connected = true;
    bool display_power = true;
    uint8_t brightness = 6;
    uint8_t distance = 4;
    uint8_t height = 4;
    std::string locale = "zh-CN";
    int16_t accel[3] = {0, 0, 16384};
    int16_t gyro[3] = {0, 0, 0};
    int16_t temperature = 2500;
    int16_t pitch = 0;
};

struct TextOverlay {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int font_height = 17;
    int alignment = 1; // 1 left, 2 center, 3 right
    uint8_t gray = 255;
    std::string utf8;
};

struct BluetoothMessage {
    uint8_t service = kPluginServiceId;
    uint8_t command = kPluginCommandGlassesToPhone;
    uint16_t channel = 0;
    std::vector<uint8_t> payload;
};

class Previewer {
public:
    static constexpr int kDisplayWidth = 600;
    static constexpr int kDisplayHeight = 350;

    Previewer();
    ~Previewer();

    void loadFile(const std::string &path);
    void loadBytes(const std::vector<uint8_t> &package,
                   const std::string &source_name = "memory.gmp");
    void start();
    void stop();
    void suspend();
    void resume();
    void unload();
    void tick(uint32_t elapsed_ms);

    bool sendButton(uint16_t action, uint16_t button = 1);
    bool sendGesture(uint16_t gesture, bool active = true);
    bool simulateDirectionGesture(uint16_t gesture);
    bool sendBluetooth(uint16_t channel, const std::vector<uint8_t> &payload);
    bool sendConnection(bool connected);

    bool loaded() const { return loaded_; }
    bool running() const { return running_; }
    bool suspended() const { return suspended_; }
    bool exitRequested() const { return exit_requested_; }
    uint32_t monotonicMs() const { return monotonic_ms_; }
    uint64_t instructionCount() const { return cpu_.instructions; }
    const PackageInfo &packageInfo() const { return package_info_; }
    const std::string &sourceName() const { return source_name_; }
    DeviceState &device() { return device_; }
    const DeviceState &device() const { return device_; }
    const std::vector<std::string> &logs() const { return logs_; }
    const std::vector<BluetoothMessage> &bluetoothOutbox() const { return bt_outbox_; }
    void clearBluetoothOutbox() { bt_outbox_.clear(); }
    void setFontData(std::vector<uint8_t> default_font,
                     std::vector<uint8_t> large_font);

    const std::vector<uint8_t> &renderFrame();
    const std::vector<TextOverlay> &textOverlays() const { return text_overlays_; }
    size_t objectCount() const;
    void savePgm(const std::string &path);

private:
    enum class NodeType { Root, Object, Label, Arc, Line };
    struct Node {
        uint32_t handle = 0;
        uint32_t parent = 0;
        NodeType type = NodeType::Object;
        int x = 0, y = 0, width = 100, height = 50;
        bool size_set = false;
        uint8_t alignment = 0;
        int align_x = 0, align_y = 0;
        uint32_t flags = 16;
        std::map<uint64_t, uint32_t> styles;
        std::string text;
        uint8_t label_mode = 0;
        int16_t arc_min = 0, arc_max = 100, arc_value = 0;
        uint32_t line_points = 0;
        uint16_t line_point_count = 0;
        uint32_t selection_start = 0xffff;
        uint32_t selection_end = 0xffff;
    };

    struct FontGlyph {
        uint8_t advance = 0;
        uint8_t box_width = 0;
        uint8_t box_height = 0;
        int8_t offset_x = 0;
        int8_t offset_y = 0;
        uint8_t bpp = 0;
        const uint8_t *bitmap = nullptr;
        size_t bitmap_size = 0;
    };

    static constexpr uint32_t kPluginBase = 0x10000000u;
    static constexpr uint32_t kStackBase = 0x20000000u;
    static constexpr uint32_t kStackSize = 128u * 1024u;
    static constexpr uint32_t kHeapBase = 0x30000000u;
    static constexpr uint32_t kHeapSize = 4u * 1024u * 1024u;
    static constexpr uint32_t kSystemBase = 0x40000000u;
    static constexpr uint32_t kFramebufferBase = 0x41000000u;
    static constexpr uint32_t kTrapBase = 0x50000000u;
    static constexpr uint32_t kObjectBase = 0x60000000u;
    static constexpr uint32_t kHostTable = kSystemBase;
    static constexpr uint32_t kLvglTable = kSystemBase + 0x100;
    static constexpr uint32_t kDescriptor = kSystemBase + 0x200;
    static constexpr uint32_t kEvent = kSystemBase + 0x300;
    static constexpr uint32_t kDemoExtension = kSystemBase + 0x800;
    static constexpr uint32_t kLz4Extension = kSystemBase + 0x900;
    // Keep the maximum Bluetooth payload clear of all Host ABI tables.
    static constexpr uint32_t kEventPayload = kSystemBase + 0x1000;
    static constexpr uint32_t kFontDefault = kObjectBase + 4;
    static constexpr uint32_t kFontLarge = kObjectBase + 8;
    static constexpr uint32_t kRootObject = kObjectBase + 0x100;

    Rv32 cpu_;
    PackageInfo package_info_;
    DeviceState device_;
    std::string source_name_;
    bool loaded_ = false;
    bool running_ = false;
    bool suspended_ = false;
    bool exit_requested_ = false;
    bool framebuffer_locked_ = false;
    uint16_t locked_y_ = 0;
    uint16_t locked_height_ = 0;
    uint8_t imu_modes_ = 0;
    bool auto_brightness_blocked_ = false;
    uint32_t monotonic_ms_ = 0;
    uint32_t next_object_ = kObjectBase + 0x104;
    uint32_t callbacks_[8]{};
    uint32_t context_ = 0;
    std::map<uint32_t, uint32_t> allocations_;
    std::map<uint32_t, uint32_t> free_blocks_;
    std::map<uint32_t, Node> nodes_;
    std::vector<std::string> logs_;
    std::vector<BluetoothMessage> bt_outbox_;
    std::vector<uint8_t> frame_;
    std::vector<uint8_t> presented_framebuffer_;
    std::vector<TextOverlay> text_overlays_;
    std::vector<uint8_t> font_default_data_;
    std::vector<uint8_t> font_large_data_;

    void initializeMemory(uint32_t plugin_memory_size);
    void buildHostTables();
    bool handleTrap(Rv32 &cpu, uint32_t address);
    uint32_t trap(unsigned id) const { return kTrapBase + id * 4u; }
    void invokeVoid(uint32_t callback, const std::vector<uint32_t> &arguments);
    int32_t invokeResult(uint32_t callback, const std::vector<uint32_t> &arguments);
    bool dispatchEvent(uint16_t type);
    void honorExitRequest();
    void validateCallback(uint32_t callback) const;

    uint32_t allocate(uint32_t size);
    void release(uint32_t address);
    std::string formatGuestLog();
    void appendLog(const std::string &message);

    Node *node(uint32_t handle);
    const Node *node(uint32_t handle) const;
    uint32_t createNode(NodeType type, uint32_t parent);
    void deleteNode(uint32_t handle, bool allow_root = false);
    void cleanNode(uint32_t handle);
    uint32_t style(const Node &node, uint32_t property, uint32_t selector,
                   uint32_t fallback) const;
    void absoluteBox(const Node &node, int &x, int &y, int &width, int &height) const;
    void renderNode(const Node &node);
    void fillRect(int x, int y, int width, int height, uint8_t gray, uint8_t opacity = 255);
    void strokeRect(int x, int y, int width, int height, int stroke, uint8_t gray);
    void drawLine(int x0, int y0, int x1, int y1, int width, uint8_t gray);
    void drawArc(int cx, int cy, int radius, int width, double start, double end, uint8_t gray);
    uint8_t nativeColorToGray(uint32_t color) const;
    int fontHeight(uint32_t font) const;
    int fontBaseline(uint32_t font) const;
    const std::vector<uint8_t> &fontData(int line_height) const;
    bool findFontGlyph(int line_height, uint32_t codepoint,
                       FontGlyph &glyph) const;
    uint32_t decodeUtf8(const std::string &text, size_t &offset) const;
    int textWidth(const std::string &text, int font_height, int letter_space = 0) const;
    uint32_t utf8NextLine(const std::string &text, int font_height,
                          int letter_space, int max_width, int *used_width) const;
    void drawExactText(int x, int y, int width, int height,
                       const std::string &text, uint32_t font,
                       int letter_space, int line_space, int alignment,
                       bool wrap, uint8_t gray, uint8_t opacity);

    int32_t lz4Compress(uint32_t source, uint32_t destination,
                        int32_t source_size, int32_t capacity);
    int32_t lz4Decompress(uint32_t source, uint32_t destination,
                          int32_t compressed_size, int32_t capacity);
};

} // namespace gmpreview
