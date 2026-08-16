#include <windows.h>

namespace {
    LRESULT CALLBACK
    MainWindowCallback(
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
            case WM_PAINT: {
                PAINTSTRUCT paint;
                HDC device_context = BeginPaint(window, &paint);
                if (device_context) {
                    int x, y, width, height;
                    x = paint.rcPaint.left;
                    y = paint.rcPaint.top;
                    width = paint.rcPaint.right - paint.rcPaint.left;
                    height = paint.rcPaint.bottom - paint.rcPaint.top;
                    PatBlt(device_context, x, y, width, height, WHITENESS);
                } else {
                    // TODO errohandling
                }
                EndPaint(window, &paint);
            }
            break;
            case WM_DESTROY: {
                OutputDebugString("WM_DESTROY\n");
            }
            break;
            case WM_CLOSE: {
                OutputDebugString("WM_CLOSE\n");
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
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {
    WNDCLASS window_class = {};

    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = MainWindowCallback;
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
            for (;;) {
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
