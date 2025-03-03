#include "MainWindow.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Create main window
    MainWindow& window = MainWindow::getInstance();
    if (!window.create(hInstance, nCmdShow)) {
        return 1;
    }
    
    // Run message loop
    return window.messageLoop();
} 