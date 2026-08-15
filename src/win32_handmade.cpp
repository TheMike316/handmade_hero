#include <windows.h>

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {

    MessageBox(
        nullptr,
        "This is a message box",
        "Handmade Hero",
        MB_OK | MB_ICONINFORMATION
        );

    return(0);
}

