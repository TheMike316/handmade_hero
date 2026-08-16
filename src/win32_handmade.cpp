#include <windows.h>

// static is a horrible keyword in c/c++
#define local_persist static
#define global_variable static
#define internal static

// TODO these are global for now
global_variable bool Running;
global_variable BITMAPINFO bitmap_info;
global_variable void **bitmap_memory;
global_variable HBITMAP bitmap_handle;
global_variable HDC bitmap_device_context;

internal void
Win32ResizeDIBSection(int width, int height) {
    // TODO bulletproof
    // maybe don't free first, free after, then free first if that fails

    if (bitmap_handle) {
        DeleteObject(bitmap_handle);
    }

    if (!bitmap_device_context) {
        // TODO should we recreate these under certain circumstances?
        bitmap_device_context = CreateCompatibleDC(nullptr);
    }

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32; // RGBA;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    bitmap_handle = CreateDIBSection(
        bitmap_device_context,
        &bitmap_info,
        DIB_RGB_COLORS,
        bitmap_memory,
        0, 0
    );
}

internal void
Win32UpdateWindow(HDC device_context, int x, int y, int width, int height) {
    StretchDIBits(
        device_context,
        x, y, width, height,
        x, y, width, height,
        bitmap_memory,
        &bitmap_info,
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
            RECT client_rect;
            GetClientRect(window, &client_rect);
            int width, height;
            width = client_rect.right - client_rect.left;
            height = client_rect.bottom - client_rect.top;
            Win32ResizeDIBSection(width, height);
            OutputDebugString("WM_SIZE\n");
        }
        break;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            if (device_context) {
                int x, y, width, height;
                x = paint.rcPaint.left;
                y = paint.rcPaint.top;
                width = paint.rcPaint.right - paint.rcPaint.left;
                height = paint.rcPaint.bottom - paint.rcPaint.top;
                Win32UpdateWindow(device_context, x, y, width, height);
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

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {
    WNDCLASS window_class = {};

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
            Running = true;
            while (Running) {
                BOOL message_result = GetMessage(&message, nullptr, 0, 0);
                if (message_result > 0) {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                } else {
                    break;
                }
            }
        } else {
            // todo nice error handling
        }
    } else {
        // todo error handling
    }

    return (0);
}
