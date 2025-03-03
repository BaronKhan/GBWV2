#include "MainWindow.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Initialize COM library for WebView2
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    // Create main window
    MainWindow& window = MainWindow::getInstance();
    if (!window.create(hInstance, nCmdShow)) {
        return 1;
    }
    
    // Run message loop
    return window.messageLoop();
} 