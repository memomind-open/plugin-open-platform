#include "previewer.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum ControlId {
    ID_OPEN = 1001, ID_START, ID_STOP, ID_PAUSE,
    ID_CONNECTED, ID_WEARING, ID_CHARGING, ID_BATTERY,
    ID_GYRO_X, ID_GYRO_Y, ID_GYRO_Z, ID_PITCH, ID_APPLY_DEVICE,
    ID_HEAD_UP, ID_HEAD_DOWN, ID_HEAD_LEFT, ID_HEAD_RIGHT, ID_HEAD_CENTER,
    ID_BUTTON_SINGLE, ID_BUTTON_DOUBLE, ID_BUTTON_LONG,
    ID_GESTURE_NOD, ID_GESTURE_LEFT, ID_GESTURE_RIGHT, ID_GESTURE_SHAKE,
    ID_BT_CHANNEL, ID_BT_PAYLOAD, ID_BT_SEND,
    ID_STATUS, ID_LOG,
};

constexpr int CanvasX = 16;
constexpr int CanvasY = 62;
constexpr int CanvasWidth = 600;
constexpr int CanvasHeight = 350;

std::wstring fromUtf8(const std::string &text)
{
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return L"[invalid UTF-8]";
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}

std::string toUtf8(const std::wstring &text)
{
    if (text.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                    nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring controlText(HWND control)
{
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<size_t>(length));
    return result;
}

int controlNumber(HWND control, int fallback)
{
    try { return std::stoi(controlText(control)); }
    catch (...) { return fallback; }
}

void setControlNumber(HWND control, int value)
{
    const std::wstring text = std::to_wstring(value);
    SetWindowTextW(control, text.c_str());
}

HFONT createPreviewFont(HDC dc, int lvgl_line_height)
{
    // A negative lfHeight requests glyph height, while GDI's actual text row
    // also includes internal leading. LVGL's font line_height already includes
    // all vertical metrics, so creating a font with -line_height can make the
    // GDI row two or three pixels taller and clip CJK glyphs at the bottom.
    // Firmware XBF metrics keep the glyph smaller than the line box:
    // xgimi_17: line 34, CJK glyph/advance about 22/23 pixels;
    // xgimi_20: line 41, CJK glyph/advance about 28/29 pixels.
    int character_height = lvgl_line_height >= 40 ? 30 : 24;
    HFONT font = nullptr;
    for (int attempt = 0; attempt < 3; ++attempt) {
        font = CreateFontW(-character_height, 0, 0, 0, FW_NORMAL,
                           FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font) return nullptr;
        HGDIOBJ previous = SelectObject(dc, font);
        TEXTMETRICW metrics{};
        const BOOL measured = GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        if (!measured || metrics.tmHeight <= lvgl_line_height) return font;

        const int overflow = metrics.tmHeight - lvgl_line_height;
        DeleteObject(font);
        font = nullptr;
        character_height = std::max(1, character_height - overflow);
    }
    return font;
}

std::vector<uint8_t> readFile(const std::wstring &path)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open the selected GMP file");
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        throw std::runtime_error("invalid or unexpectedly large GMP file");
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != bytes.size()) throw std::runtime_error("could not read the complete GMP file");
    return bytes;
}

std::vector<uint8_t> readResource(HINSTANCE instance, int identifier)
{
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(identifier), RT_RCDATA);
    if (!resource) throw std::runtime_error("embedded firmware font resource is missing");
    const DWORD size = SizeofResource(instance, resource);
    HGLOBAL loaded = LoadResource(instance, resource);
    const void *bytes = loaded ? LockResource(loaded) : nullptr;
    if (!bytes || size == 0) throw std::runtime_error("could not load embedded firmware font");
    const auto *begin = static_cast<const uint8_t *>(bytes);
    return std::vector<uint8_t>(begin, begin + size);
}

class Window {
public:
    HWND hwnd = nullptr;
    gmpreview::Previewer previewer;

    void loadPath(const std::wstring &path) { load(path); }

    LRESULT message(UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message) {
        case WM_CREATE: createControls(); return 0;
        case WM_COMMAND: command(LOWORD(wparam)); return 0;
        case WM_TIMER: timer(); return 0;
        case WM_PAINT: paint(); return 0;
        case WM_SIZE: layout(); return 0;
        case WM_DROPFILES: drop(reinterpret_cast<HDROP>(wparam)); return 0;
        case WM_GETMINMAXINFO: {
            auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
            info->ptMinTrackSize.x = 980;
            info->ptMinTrackSize.y = 740;
            return 0;
        }
        case WM_DESTROY: KillTimer(hwnd, 1); PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd, message, wparam, lparam);
        }
    }

private:
    enum class HeadMotion { None, Up, Down, Left, Right };

    HWND status_ = nullptr, log_ = nullptr;
    HWND connected_ = nullptr, wearing_ = nullptr, charging_ = nullptr;
    HWND battery_ = nullptr, gyro_x_ = nullptr, gyro_y_ = nullptr;
    HWND gyro_z_ = nullptr, pitch_ = nullptr, head_pose_ = nullptr;
    HWND bt_channel_ = nullptr, bt_payload_ = nullptr, pause_ = nullptr;
    HFONT ui_font_ = nullptr;
    size_t shown_logs_ = 0;
    std::wstring current_path_;
    int yaw_degrees_ = 0;
    HeadMotion head_motion_ = HeadMotion::None;
    int head_motion_phase_ = 0;
    int head_motion_ticks_ = 0;

    static constexpr int kPoseStepDegrees = 3;

    HWND add(const wchar_t *kind, const wchar_t *text, DWORD style,
             int x, int y, int width, int height, int id = 0)
    {
        HWND control = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
                                       x, y, width, height, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleW(nullptr), nullptr);
        if (ui_font_) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
        return control;
    }

    void createControls()
    {
        ui_font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        add(L"BUTTON", L"Open .gmp", BS_PUSHBUTTON, 16, 16, 108, 30, ID_OPEN);
        add(L"BUTTON", L"Start / Restart", BS_PUSHBUTTON, 132, 16, 124, 30, ID_START);
        add(L"BUTTON", L"Stop", BS_PUSHBUTTON, 264, 16, 76, 30, ID_STOP);
        pause_ = add(L"BUTTON", L"Pause", BS_PUSHBUTTON, 348, 16, 76, 30, ID_PAUSE);
        add(L"STATIC", L"Drop a .gmp onto this window", SS_LEFT, 442, 22, 320, 22);

        const int panel = 636;
        status_ = add(L"STATIC", L"No plugin loaded\r\n600×350 · GRAY4 · LVGL 8.3 approximate Host",
                      SS_LEFT, panel, 62, 520, 48, ID_STATUS);
        add(L"BUTTON", L"Simulated device state", BS_GROUPBOX, panel, 112, 520, 116);
        connected_ = add(L"BUTTON", L"Phone connected", BS_AUTOCHECKBOX, panel + 12, 134, 140, 24, ID_CONNECTED);
        wearing_ = add(L"BUTTON", L"Wearing", BS_AUTOCHECKBOX, panel + 166, 134, 100, 24, ID_WEARING);
        charging_ = add(L"BUTTON", L"Charging", BS_AUTOCHECKBOX, panel + 278, 134, 100, 24, ID_CHARGING);
        SendMessageW(connected_, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(wearing_, BM_SETCHECK, BST_CHECKED, 0);
        add(L"STATIC", L"Battery", SS_LEFT, panel + 12, 168, 48, 22);
        battery_ = add(L"EDIT", L"86", WS_BORDER | ES_NUMBER, panel + 62, 165, 38, 24, ID_BATTERY);
        add(L"STATIC", L"GX", SS_LEFT, panel + 110, 168, 22, 22);
        gyro_x_ = add(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, panel + 134, 165, 44, 24, ID_GYRO_X);
        add(L"STATIC", L"GY", SS_LEFT, panel + 186, 168, 22, 22);
        gyro_y_ = add(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, panel + 210, 165, 44, 24, ID_GYRO_Y);
        add(L"STATIC", L"GZ", SS_LEFT, panel + 262, 168, 22, 22);
        gyro_z_ = add(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, panel + 286, 165, 44, 24, ID_GYRO_Z);
        add(L"STATIC", L"Pitch", SS_LEFT, panel + 338, 168, 36, 22);
        pitch_ = add(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, panel + 376, 165, 44, 24, ID_PITCH);
        add(L"BUTTON", L"Apply", BS_PUSHBUTTON, panel + 430, 164, 78, 26, ID_APPLY_DEVICE);
        add(L"STATIC", L"Changes affect future Host API reads; connection emits an event.", SS_LEFT,
            panel + 12, 198, 490, 20);

        add(L"BUTTON", L"Head pose / raw IMU · one click = 3°", BS_GROUPBOX,
            panel, 234, 520, 124);
        add(L"BUTTON", L"Raise", BS_PUSHBUTTON, panel + 98, 254, 82, 28, ID_HEAD_UP);
        add(L"BUTTON", L"Turn left", BS_PUSHBUTTON, panel + 12, 286, 82, 28, ID_HEAD_LEFT);
        add(L"BUTTON", L"Center", BS_PUSHBUTTON, panel + 98, 286, 82, 28, ID_HEAD_CENTER);
        add(L"BUTTON", L"Turn right", BS_PUSHBUTTON, panel + 184, 286, 82, 28, ID_HEAD_RIGHT);
        add(L"BUTTON", L"Lower", BS_PUSHBUTTON, panel + 98, 318, 82, 28, ID_HEAD_DOWN);
        head_pose_ = add(L"STATIC", L"Pitch 0° · Yaw 0°", SS_LEFT,
                         panel + 286, 258, 216, 24);
        add(L"STATIC", L"Pitch persists; yaw has no Host field. Raw gyro motion is emitted.",
            SS_LEFT, panel + 286, 288, 216, 52);

        add(L"BUTTON", L"Input events", BS_GROUPBOX, panel, 364, 520, 104);
        add(L"BUTTON", L"Single", BS_PUSHBUTTON, panel + 12, 386, 68, 28, ID_BUTTON_SINGLE);
        add(L"BUTTON", L"Double", BS_PUSHBUTTON, panel + 84, 386, 68, 28, ID_BUTTON_DOUBLE);
        add(L"BUTTON", L"Long", BS_PUSHBUTTON, panel + 156, 386, 68, 28, ID_BUTTON_LONG);
        add(L"BUTTON", L"Nod", BS_PUSHBUTTON, panel + 236, 386, 60, 28, ID_GESTURE_NOD);
        add(L"BUTTON", L"Left", BS_PUSHBUTTON, panel + 300, 386, 60, 28, ID_GESTURE_LEFT);
        add(L"BUTTON", L"Right", BS_PUSHBUTTON, panel + 364, 386, 60, 28, ID_GESTURE_RIGHT);
        add(L"BUTTON", L"Shake", BS_PUSHBUTTON, panel + 428, 386, 68, 28, ID_GESTURE_SHAKE);
        add(L"STATIC", L"Button events", SS_LEFT, panel + 12, 426, 210, 20);
        add(L"STATIC", L"Recognized IMU gesture events", SS_LEFT, panel + 236, 426, 260, 20);

        add(L"BUTTON", L"Bluetooth message from phone", BS_GROUPBOX, panel, 474, 520, 74);
        add(L"STATIC", L"Channel", SS_LEFT, panel + 12, 500, 58, 22);
        bt_channel_ = add(L"EDIT", L"1", WS_BORDER | ES_NUMBER, panel + 72, 497, 54, 25, ID_BT_CHANNEL);
        bt_payload_ = add(L"EDIT", L"Hello plugin", WS_BORDER | ES_AUTOHSCROLL,
                          panel + 136, 497, 274, 25, ID_BT_PAYLOAD);
        add(L"BUTTON", L"Send", BS_PUSHBUTTON, panel + 420, 496, 76, 27, ID_BT_SEND);

        add(L"STATIC", L"Host / plugin log", SS_LEFT, 16, 560, 180, 22);
        log_ = add(L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL |
                   ES_READONLY | WS_VSCROLL, 16, 582, 1140, 140, ID_LOG);
        DragAcceptFiles(hwnd, TRUE);
        SetTimer(hwnd, 1, 33, nullptr);
    }

    void layout()
    {
        if (!log_) return;
        RECT client{};
        GetClientRect(hwnd, &client);
        MoveWindow(log_, 16, 582, std::max(200L, client.right - 32),
                   std::max(100L, client.bottom - 598), TRUE);
    }

    void showError(const std::exception &error)
    {
        MessageBoxW(hwnd, fromUtf8(error.what()).c_str(), L"GM Plugin Previewer",
                    MB_OK | MB_ICONERROR);
    }

    void openDialog()
    {
        wchar_t path[MAX_PATH]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = hwnd;
        dialog.lpstrFilter = L"GM plugin package (*.gmp)\0*.gmp\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = MAX_PATH;
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        dialog.lpstrDefExt = L"gmp";
        if (GetOpenFileNameW(&dialog)) load(path);
    }

    void load(const std::wstring &path)
    {
        try {
            const std::vector<uint8_t> bytes = readFile(path);
            previewer.loadBytes(bytes, toUtf8(path));
            previewer.start();
            SetWindowTextW(pause_, L"Pause");
            current_path_ = path;
            shown_logs_ = 0;
            SetWindowTextW(log_, L"");
            refreshLog();
            updateStatus();
            InvalidateRect(hwnd, nullptr, FALSE);
        } catch (const std::exception &error) { showError(error); }
    }

    void drop(HDROP drop_handle)
    {
        wchar_t path[MAX_PATH]{};
        if (DragQueryFileW(drop_handle, 0, path, MAX_PATH)) load(path);
        DragFinish(drop_handle);
    }

    void applyDevice(bool connection_event)
    {
        auto &device = previewer.device();
        const bool connected = SendMessageW(connected_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        device.wearing = SendMessageW(wearing_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        device.charging = SendMessageW(charging_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        device.battery_percent = static_cast<uint8_t>(std::clamp(controlNumber(battery_, 86), 0, 100));
        device.gyro[0] = static_cast<int16_t>(std::clamp(controlNumber(gyro_x_, 0), -32768, 32767));
        device.gyro[1] = static_cast<int16_t>(std::clamp(controlNumber(gyro_y_, 0), -32768, 32767));
        device.gyro[2] = static_cast<int16_t>(std::clamp(controlNumber(gyro_z_, 0), -32768, 32767));
        device.pitch = static_cast<int16_t>(std::clamp(controlNumber(pitch_, 0), -32768, 32767));
        head_motion_ = HeadMotion::None;
        head_motion_phase_ = 0;
        head_motion_ticks_ = 0;
        if (connection_event && connected != device.connected && previewer.running())
            previewer.sendConnection(connected);
        else device.connected = connected;
        updateHeadPose();
    }

    static std::wstring signedDegrees(int value)
    {
        return (value > 0 ? L"+" : L"") + std::to_wstring(value) + L"°";
    }

    void updateHeadPose()
    {
        if (!head_pose_) return;
        const std::wstring text = L"Pitch " + signedDegrees(previewer.device().pitch) +
                                  L" · Yaw " + signedDegrees(yaw_degrees_);
        SetWindowTextW(head_pose_, text.c_str());
    }

    void restoreManualGyro()
    {
        auto &gyro = previewer.device().gyro;
        gyro[0] = static_cast<int16_t>(std::clamp(controlNumber(gyro_x_, 0), -32768, 32767));
        gyro[1] = static_cast<int16_t>(std::clamp(controlNumber(gyro_y_, 0), -32768, 32767));
        gyro[2] = static_cast<int16_t>(std::clamp(controlNumber(gyro_z_, 0), -32768, 32767));
    }

    void setMotionGyro(bool returning)
    {
        restoreManualGyro();
        auto &gyro = previewer.device().gyro;
        const int vertical = returning ? 30 : 80;
        const int horizontal = returning ? 20 : 60;
        const int direction = returning ? -1 : 1;
        auto add = [](int16_t value, int delta) {
            return static_cast<int16_t>(std::clamp(static_cast<int>(value) + delta,
                                                   -32768, 32767));
        };
        switch (head_motion_) {
        case HeadMotion::Up:
            gyro[2] = add(gyro[2], direction * vertical);
            break;
        case HeadMotion::Down:
            gyro[2] = add(gyro[2], -direction * vertical);
            break;
        case HeadMotion::Left:
            gyro[0] = add(gyro[0], -direction * horizontal);
            gyro[1] = add(gyro[1], direction * horizontal);
            break;
        case HeadMotion::Right:
            gyro[0] = add(gyro[0], direction * horizontal);
            gyro[1] = add(gyro[1], -direction * horizontal);
            break;
        case HeadMotion::None:
            break;
        }
    }

    void moveHead(HeadMotion motion)
    {
        // Pick up manual raw values and any manually entered pitch before
        // applying the convenient one-step pose control.
        applyDevice(false);
        auto &device = previewer.device();
        switch (motion) {
        case HeadMotion::Up:
            device.pitch = static_cast<int16_t>(std::min(90,
                static_cast<int>(device.pitch) + kPoseStepDegrees));
            setControlNumber(pitch_, device.pitch);
            break;
        case HeadMotion::Down:
            device.pitch = static_cast<int16_t>(std::max(-90,
                static_cast<int>(device.pitch) - kPoseStepDegrees));
            setControlNumber(pitch_, device.pitch);
            break;
        case HeadMotion::Left:
            yaw_degrees_ = std::max(-180, yaw_degrees_ - kPoseStepDegrees);
            break;
        case HeadMotion::Right:
            yaw_degrees_ = std::min(180, yaw_degrees_ + kPoseStepDegrees);
            break;
        case HeadMotion::None:
            break;
        }
        head_motion_ = motion;
        head_motion_phase_ = 1;
        head_motion_ticks_ = 3;
        setMotionGyro(false);
        updateHeadPose();
    }

    void centerHead()
    {
        setControlNumber(gyro_x_, 0);
        setControlNumber(gyro_y_, 0);
        setControlNumber(gyro_z_, 0);
        setControlNumber(pitch_, 0);
        yaw_degrees_ = 0;
        applyDevice(false);
    }

    void advanceHeadMotion()
    {
        if (head_motion_ == HeadMotion::None || --head_motion_ticks_ > 0) return;
        if (head_motion_phase_ == 1) {
            // A smaller opposite-rate sample models braking/settling and lets
            // raw-IMU games observe that the physical movement has ended.
            head_motion_phase_ = 2;
            head_motion_ticks_ = 2;
            setMotionGyro(true);
        } else {
            restoreManualGyro();
            head_motion_ = HeadMotion::None;
            head_motion_phase_ = 0;
            head_motion_ticks_ = 0;
        }
    }

    void command(int id)
    {
        try {
            switch (id) {
            case ID_OPEN: openDialog(); break;
            case ID_START:
                if (previewer.running()) previewer.stop();
                previewer.start();
                SetWindowTextW(pause_, L"Pause");
                break;
            case ID_STOP: previewer.stop(); SetWindowTextW(pause_, L"Pause"); break;
            case ID_PAUSE:
                if (!previewer.running()) break;
                if (previewer.suspended()) { previewer.resume(); SetWindowTextW(pause_, L"Pause"); }
                else { previewer.suspend(); SetWindowTextW(pause_, L"Resume"); }
                break;
            case ID_APPLY_DEVICE: applyDevice(true); break;
            case ID_CONNECTED: applyDevice(true); break;
            case ID_WEARING: case ID_CHARGING: applyDevice(false); break;
            case ID_HEAD_UP: moveHead(HeadMotion::Up); break;
            case ID_HEAD_DOWN: moveHead(HeadMotion::Down); break;
            case ID_HEAD_LEFT: moveHead(HeadMotion::Left); break;
            case ID_HEAD_RIGHT: moveHead(HeadMotion::Right); break;
            case ID_HEAD_CENTER: centerHead(); break;
            case ID_BUTTON_SINGLE: previewer.sendButton(1); break;
            case ID_BUTTON_DOUBLE: previewer.sendButton(2); break;
            case ID_BUTTON_LONG: previewer.sendButton(3); break;
            case ID_GESTURE_NOD: previewer.sendGesture(1); break;
            case ID_GESTURE_LEFT: previewer.sendGesture(7); break;
            case ID_GESTURE_RIGHT: previewer.sendGesture(8); break;
            case ID_GESTURE_SHAKE: previewer.sendGesture(4); break;
            case ID_BT_SEND: {
                const int channel = std::clamp(controlNumber(bt_channel_, 1), 0, 65535);
                const std::string text = toUtf8(controlText(bt_payload_));
                previewer.sendBluetooth(static_cast<uint16_t>(channel),
                    std::vector<uint8_t>(text.begin(), text.end()));
                break;
            }
            default: break;
            }
            refreshLog();
            updateStatus();
            InvalidateRect(hwnd, nullptr, FALSE);
        } catch (const std::exception &error) { showError(error); }
    }

    void timer()
    {
        try {
            if (previewer.running() && !previewer.suspended()) previewer.tick(33);
            advanceHeadMotion();
            refreshLog();
            updateStatus();
            InvalidateRect(hwnd, nullptr, FALSE);
        } catch (const std::exception &error) {
            try { previewer.stop(); } catch (...) {}
            showError(error);
        }
    }

    void refreshLog()
    {
        if (!log_) return;
        const auto &lines = previewer.logs();
        while (shown_logs_ < lines.size()) {
            std::wstring line = fromUtf8(lines[shown_logs_++]) + L"\r\n";
            SendMessageW(log_, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
            SendMessageW(log_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
        }
        SendMessageW(log_, EM_SCROLLCARET, 0, 0);
    }

    void updateStatus()
    {
        if (!status_) return;
        std::wstring state = previewer.loaded() ? (previewer.running() ? L"Running" : L"Loaded") : L"No plugin loaded";
        if (previewer.suspended()) state = L"Paused";
        if (previewer.exitRequested()) state += L" · exit requested";
        state += L" · objects " + std::to_wstring(previewer.objectCount());
        state += L" · " + std::to_wstring(previewer.monotonicMs()) + L" ms\r\n";
        state += L"SIMULATED · 600×350 GRAY4 · LVGL 8.3 approximation";
        SetWindowTextW(status_, state.c_str());
    }

    void paint()
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT canvas{CanvasX, CanvasY, CanvasX + CanvasWidth, CanvasY + CanvasHeight};
        HBRUSH border = CreateSolidBrush(RGB(52, 70, 64));
        FrameRect(dc, &canvas, border);
        DeleteObject(border);
        try {
            const auto &gray = previewer.renderFrame();
            std::vector<uint32_t> pixels(gray.size());
            for (size_t i = 0; i < gray.size(); ++i) {
                const uint8_t value = gray[i];
                const uint8_t red = static_cast<uint8_t>(value / 12);
                const uint8_t green = value;
                const uint8_t blue = static_cast<uint8_t>(value / 5);
                pixels[i] = static_cast<uint32_t>(red) << 16 |
                            static_cast<uint32_t>(green) << 8 | blue;
            }
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = gmpreview::Previewer::kDisplayWidth;
            info.bmiHeader.biHeight = -gmpreview::Previewer::kDisplayHeight;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(dc, CanvasX, CanvasY, CanvasWidth, CanvasHeight,
                          0, 0, gmpreview::Previewer::kDisplayWidth,
                          gmpreview::Previewer::kDisplayHeight,
                          pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
            SetBkMode(dc, TRANSPARENT);
            const int saved_text_dc = SaveDC(dc);
            IntersectClipRect(dc, CanvasX, CanvasY,
                             CanvasX + CanvasWidth, CanvasY + CanvasHeight);
            for (const auto &overlay : previewer.textOverlays()) {
                RECT area{CanvasX + overlay.x, CanvasY + overlay.y,
                          CanvasX + overlay.x + overlay.width,
                          CanvasY + overlay.y + overlay.height};
                const uint8_t value = overlay.gray;
                SetTextColor(dc, RGB(value / 12, value, value / 5));
                HFONT font = createPreviewFont(dc, overlay.font_height);
                if (!font) continue;
                HGDIOBJ previous = SelectObject(dc, font);
                // Font fallback glyphs can overhang their GDI layout box by a
                // pixel or two. Keep LVGL positioning/wrapping but clip only
                // to the glasses canvas, not to the synthetic label rectangle.
                UINT flags = DT_NOPREFIX | DT_WORDBREAK | DT_NOCLIP;
                if (overlay.alignment == 2) flags |= DT_CENTER;
                else if (overlay.alignment == 3) flags |= DT_RIGHT;
                else flags |= DT_LEFT;
                const std::wstring text = fromUtf8(overlay.utf8);
                DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &area, flags);
                SelectObject(dc, previous);
                DeleteObject(font);
            }
            RestoreDC(dc, saved_text_dc);
        } catch (...) {
            HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(dc, &canvas, black);
            DeleteObject(black);
        }
        EndPaint(hwnd, &paint);
    }
};

LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    Window *window = reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        window = static_cast<Window *>(create->lpCreateParams);
        window->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    return window ? window->message(message, wparam, lparam)
                  : DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int show)
{
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = windowProcedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = CreateSolidBrush(RGB(242, 246, 244));
    window_class.lpszClassName = L"GMPluginPreviewerWindow";
    if (!RegisterClassExW(&window_class)) return 1;

    Window window;
#if GM_PREVIEW_HAS_EMBEDDED_FONTS
    try {
        window.previewer.setFontData(readResource(instance, 101),
                                     readResource(instance, 102));
    } catch (const std::exception &error) {
        MessageBoxW(nullptr, fromUtf8(error.what()).c_str(),
                    L"GM Plugin Previewer", MB_OK | MB_ICONERROR);
        return 1;
    }
#endif
    HWND hwnd = CreateWindowExW(0, window_class.lpszClassName,
                                L"GM Plugin Previewer — simulated glasses Host",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1190, 790, nullptr, nullptr, instance, &window);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    if (command_line && *command_line) {
        int count = 0;
        LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments && count > 1) window.loadPath(arguments[1]);
        if (arguments) LocalFree(arguments);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
