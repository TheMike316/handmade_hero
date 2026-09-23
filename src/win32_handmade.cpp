#include <windows.h>
#include <cstdint>
#include <iostream>
#include <xinput.h>

// static is a horrible keyword in c/c++
#define local_persist static
#define global_variable static
#define internal static

// in c++ bool needs to be either 0 or 1, so the compiler creates implicit "conversion" from any int > 0 to 1, which is unnecessary
typedef int32_t bool32;

struct win32_offscreen_buffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct win32_window_dimensions {
    int width;
    int height;
};

// NOTE(mike): manually loading xinput functions to deal with potentially missing/incorrect library
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dw_user_index, XINPUT_STATE *p_out_state)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dw_user_index, XINPUT_VIBRATION *p_vibration)

//typedef DWORD WINAPI x_input_set_state(DWORD dw_user_index, XINPUT_VIBRATION *p_vibration);
typedef X_INPUT_SET_STATE(x_input_set_state);

//typedef DWORD WINAPI x_input_get_state(DWORD dw_user_index, XINPUT_STATE *p_out_state);
typedef X_INPUT_GET_STATE(x_input_get_state);

// NOTE(mike): we create stubs so that we don't crash if xinput is not supported
X_INPUT_GET_STATE(XInputGetStateStub) {
    // if we can't load xinput, we return DEVICE_NOT_CONNECTED as an error code because if no lib the pad is essentially not connected
    return ERROR_DEVICE_NOT_CONNECTED;
}

X_INPUT_SET_STATE(XInputSetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
// NOTE(mike): maybe getting too clever here
#define XInputSetState XInputSetState_
#define XInputGetState XInputGetState_

internal void
Win32LoadXInput(void) {
    // NOTE(mike): we try to manually load the xinput functions we want to use
    HMODULE xinput_lib = LoadLibrary("xinput1_3.dll");
    if (xinput_lib) {
        XInputGetState = (x_input_get_state *) GetProcAddress(xinput_lib, "XInputGetState");
        XInputSetState = (x_input_set_state *) GetProcAddress(xinput_lib, "XInputSetState");
    }
}


internal win32_window_dimensions
get_window_dimensions(HWND window) {
    RECT client_rect;
    GetClientRect(window, &client_rect);

    win32_window_dimensions dimensions = {};
    dimensions.width = client_rect.right - client_rect.left;
    dimensions.height = client_rect.bottom - client_rect.top;

    return dimensions;
}

// TODO these are global for now
global_variable bool32 Running;
global_variable win32_offscreen_buffer global_backbuffer;


internal void
RenderWeirdGradient(const win32_offscreen_buffer *buffer, int x_offset, int y_offset) {
    // TODO decide whether to pass by ref or val
    // not sure about this fancy c++ stuff
    uint8_t *row = (uint8_t *) buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        uint32_t *pixel = (uint32_t *) row;
        for (int x = 0; x < buffer->width; ++x) {
            /*
             *                  0  1  2  3
             * Pixel in memory: 00 00 00 00
             * but these are actually little endian
             * so instead of RRGGBBAA, we'd expect to get 0xAABBGGRR
             * but because windows is windows, only RGB is reversed, the alpha channel is still at the end
             * so it is in fact 0xBBGGRRAA
             */
            uint8_t blue = x + x_offset;
            uint8_t green = y + y_offset;

            *pixel++ = (green << 8) | blue;
        }
        row += buffer->pitch;
    }
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height) {
    // TODO bulletproof
    // maybe don't free first, free after, then free first if that fails

    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    // negative bitmap height means the rows are top-down; a bit easier to reason about as a beginner
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32; // RGBA;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    buffer->bytes_per_pixel = 4;
    int bitmap_memory_size = buffer->width * buffer->height * buffer->bytes_per_pixel;
    buffer->memory = VirtualAlloc(nullptr, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);

    buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

internal void
Win32DisplayBufferInWindow(const win32_offscreen_buffer *buffer, HDC device_context, int window_width,
                           int window_height,
                           int x, int y, int width, int height) {
    // TODO aspect ratio correction
    StretchDIBits(
        device_context,
        0, 0, window_width, window_height,
        0, 0, buffer->width, buffer->height,
        buffer->memory,
        &(buffer->info),
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

internal LRESULT CALLBACK
Win32MainWindowCallback(
    _In_ HWND window,
    _In_ UINT message,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    LRESULT result = 0;
    switch (message) {
        case WM_SIZE: {
            OutputDebugString("WM_SIZE\n");
        }
        break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            uint32_t vk_code = LOWORD(wParam);
            // VK codes are keys that don't have a direct ansi mapping
            // otherwise they map to the capitalized letter, for example 'W'
            // if (vk_code == 'W')
            //    printf("big W\n");

            // lParam at bit 30 tells us whether the key WAS down previously
            bool32 was_down = (lParam & (1 << 30)) != 0;
            // lParam at bit 31 tells us whether the key IS down currently
            bool32 is_down = (lParam & (1 << 31)) != 0;
            // lParam does contain key repeats, so holding a key down would simultaneously send is_down and was_down
            if (is_down != was_down) {
                if (vk_code == 'W') {
                    OutputDebugString("W");
                } else if (vk_code == 'S') {
                    OutputDebugString("S");
                } else if (vk_code == 'A') {
                    OutputDebugString("A");
                } else if (vk_code == 'D') {
                    OutputDebugString("D");
                } else if (vk_code == 'Q') {
                    OutputDebugString("Q");
                } else if (vk_code == 'E') {
                    OutputDebugString("E");
                } else if (vk_code == VK_UP) {
                    OutputDebugString("UP");
                } else if (vk_code == VK_DOWN) {
                    OutputDebugString("DOWN");
                } else if (vk_code == VK_LEFT) {
                    OutputDebugString("LEFT");
                } else if (vk_code == VK_RIGHT) {
                    OutputDebugString("RIGHT");
                } else if (vk_code == VK_ESCAPE) {
                    OutputDebugString("ESC");
                } else if (vk_code == VK_SPACE) {
                    OutputDebugString("SPACE");
                }
            }

            // implementing ALT + F4
            bool32 alt_key_down = (bool32) lParam & (1 << 29);
            if (vk_code == VK_F4 && alt_key_down) {
                Running = false;
            }
        }
        break;
        case WM_PAINT: {
            RECT client_rect;
            GetClientRect(window, &client_rect);

            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            if (device_context) {
                int x, y, width, height;
                x = paint.rcPaint.left;
                y = paint.rcPaint.top;
                width = paint.rcPaint.right - paint.rcPaint.left;
                height = paint.rcPaint.bottom - paint.rcPaint.top;
                const win32_window_dimensions dimensions = get_window_dimensions(window);
                Win32DisplayBufferInWindow(&global_backbuffer, device_context, dimensions.width, dimensions.height, x,
                                           y,
                                           width, height);
            } else {
                // TODO errohandling
            }
            EndPaint(window, &paint);
        }
        break;
        case WM_DESTROY: {
            // TODO handle this as an error...potentially recreate window
            Running = false;
        }
        break;
        case WM_CLOSE: {
            Running = false;
        }
        break;
        case WM_ACTIVATEAPP: {
            OutputDebugString("WM_ACTIVATEAPP\n");
            break;
        }
        default: {
            OutputDebugString("default\n");
            // tell windows this is a message we want to ignore
            result = DefWindowProc(window, message, wParam, lParam);
        }
        break;
    }

    return result;
}

int CALLBACK
WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {
    Win32LoadXInput();

    WNDCLASS window_class = {};

    Win32ResizeDIBSection(&global_backbuffer, 1200, 720);

    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = hInstance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&window_class)) {
        HWND window_handle = CreateWindowEx(
            0,
            window_class.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            nullptr);
        if (window_handle) {
            MSG message;

            int x_offset = 0;
            int y_offset = 0;

            Running = true;
            while (Running) {
                while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        Running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                // handling inputs
                // TODO(mike): poll more frequently
                DWORD dwResult;
                for (DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; ++controller_idx) {
                    XINPUT_STATE controller_state;
                    dwResult = XInputGetState(controller_idx, &controller_state);
                    if (dwResult == ERROR_SUCCESS) {
                        // controller is connected
                        // TODO(mike): see if dwPacketNumber increases too frequently
                        XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
                        bool32 pad_d_up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool32 pad_d_down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool32 pad_d_left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool32 pad_d_right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool32 pad_start = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool32 pad_back = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool32 pad_left_shoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool32 pad_right_shoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool32 pad_a = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool32 pad_b = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool32 pad_x = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool32 pad_y = pad->wButtons & XINPUT_GAMEPAD_Y;

                        int16_t stick_x = controller_state.Gamepad.sThumbLX;
                        int16_t stick_y = controller_state.Gamepad.sThumbLY;

                        if (pad_a) {
                            y_offset += 2;
                        }
                    } else {
                        // controller is not connected
                    }
                }

                // test rumble; left for reference
                // XINPUT_VIBRATION vib;
                // vib.wLeftMotorSpeed = 60000;
                // vib.wRightMotorSpeed = 60000;
                // XInputSetState(0, &vib);

                RenderWeirdGradient(&global_backbuffer, x_offset, y_offset);

                HDC device_context = GetDC(window_handle);
                const win32_window_dimensions dimensions = get_window_dimensions(window_handle);
                Win32DisplayBufferInWindow(&global_backbuffer, device_context, dimensions.width, dimensions.height, 0,
                                           0,
                                           dimensions.width, dimensions.height);

                ReleaseDC(window_handle, device_context);

                ++x_offset;
                // y_offset += 2; testing controller input
            }
        } else {
            // todo nice error handling
        }
    } else {
        // todo error handling
    }

    return (0);
}
