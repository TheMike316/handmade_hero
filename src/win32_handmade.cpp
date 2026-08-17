#include <windows.h>
#include <cstdint>

// static is a horrible keyword in c/c++
#define local_persist static
#define global_variable static
#define internal static

// TODO these are global for now
global_variable bool Running;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable int bitmap_width;
global_variable int bitmap_height;

internal void
RenderWeirdGradient(int x_offset, int y_offset) {
    int width = bitmap_width;
    int height = bitmap_height;
    int bytes_per_pixel = 4;
    // pitch is the distance between the start of one row and the start of the next
    int pitch = width * bytes_per_pixel;
    // not sure about this fancy c++ stuff
    uint8_t *row = (uint8_t *) bitmap_memory;
    for (int y = 0; y < height; ++y) {
        // this is for demonstration purposes; a pixel is really 4 bytes
        uint8_t *pixel = row;
        for (int x = 0; x < width; ++x) {
            /*
             *                  0  1  2  3
             * Pixel in memory: 00 00 00 00
             * but these are actually little endian
             * so instead of RRGGBBAA, we'd expect to get 0xAABBGGRR
             * but because windows is windows, only RGB is reversed, the alpha channel is still at the end
             * so it is in fact 0xBBGGRRAA
             */
            *pixel = (uint8_t) (x + x_offset);
            ++pixel;

            *pixel = (uint8_t) (y + y_offset);
            ++pixel;

            *pixel = 0;
            ++pixel;

            *pixel = 0;
            ++pixel;
        }
        row += pitch;
    }
}

internal void
Win32ResizeDIBSection(int width, int height) {
    // TODO bulletproof
    // maybe don't free first, free after, then free first if that fails

    if (bitmap_memory) {
        VirtualFree(bitmap_memory, 0, MEM_RELEASE);
    }

    bitmap_width = width;
    bitmap_height = height;

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = bitmap_width;
    // negative bitmap height means the rows are top-down; a bit easier to reason about as a beginner
    bitmap_info.bmiHeader.biHeight = -bitmap_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32; // RGBA;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bytes_per_pixel = 4;
    int bitmap_memory_size = bitmap_width * bitmap_height * bytes_per_pixel;
    bitmap_memory = VirtualAlloc(nullptr, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);

    RenderWeirdGradient(0, 0);
    // // pitch is the distance between the start of one row and the start of the next
    // int pitch = width * bytes_per_pixel;
    // // not sure about this fancy c++ stuff
    // auto *row = static_cast<uint8_t *>(bitmap_memory);
    // for (int y = 0; y < bitmap_height; ++y) {
    //     auto *pixel = reinterpret_cast<uint32_t *>(row);
    //     for (int x = 0; x < bitmap_width; ++x) {
    //        /*
    //         *                  0  1  2  3
    //         * Pixel in memory: 00 00 00 00
    //         * but these are actually little endian
    //         * so instead of RRGGBBAA, we'd expect to get 0xAABBGGRR
    //         * but because windows is windows, only RGB is reversed, the alpha channel is still at the end
    //         * so it is in fact 0xBBGGRRAA
    //         */
    //     }
    //     row += pitch;
    // }
}

internal void
Win32UpdateWindow(HDC device_context, RECT *window_rect, int x, int y, int width, int height) {
    // StretchDIBits(
    //     device_context,
    //     x, y, width, height,
    //     x, y, width, height,
    //     bitmap_memory,
    //     &bitmap_info,
    //     DIB_RGB_COLORS,
    //     SRCCOPY
    // );
    int window_width = window_rect->right - window_rect->left;
    int window_height = window_rect->bottom - window_rect->top;
    StretchDIBits(
        device_context,
        0, 0, bitmap_width, bitmap_height,
        0, 0, window_width, window_height,
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
                Win32UpdateWindow(device_context, &client_rect, x, y, width, height);
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
                while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        Running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
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
