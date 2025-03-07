#include "MainWindow.h"
#include <shobjidl.h>
#include <shlobj.h>

// Menu IDs
enum MenuID {
    ID_FILE_OPEN = 1,
    ID_FILE_RESET,
    ID_FILE_EXIT,
    ID_EMULATION_PAUSE
};

// MainWindow constructor
MainWindow::MainWindow() : m_hwnd(nullptr), m_hInstance(nullptr), m_title("GameBoy Emulator"), m_width(640), m_height(480) {
}

// Create window
bool MainWindow::create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;
    
    // // Register window class
    // WNDCLASSEXW wcex = {};
    // wcex.cbSize = sizeof(WNDCLASSEX);
    // wcex.style = CS_HREDRAW | CS_VREDRAW;
    // wcex.lpfnWndProc = windowProc;
    // wcex.cbClsExtra = 0;
    // wcex.cbWndExtra = 0;
    // wcex.hInstance = hInstance;
    // wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    // wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // wcex.lpszMenuName = nullptr;
    // wcex.lpszClassName = L"GameBoyEmulatorWindow";
    // wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    // if (!RegisterClassExW(&wcex)) {
    //     return false;
    // }
    
    // // Create window
    // m_hwnd = CreateWindowW(L"GameBoyEmulatorWindow", L"GameBoy Emulator", WS_OVERLAPPEDWINDOW,
    //     CW_USEDEFAULT, 0, m_width, m_height, nullptr, nullptr, hInstance, this);
    
    // if (!m_hwnd) {
    //     DWORD error = GetLastError();
    //     return false;
    // }

    const wchar_t CLASS_NAME[] = L"GameBoyEmulatorWindow";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        return false;
    }

    // Create window
    m_hwnd = CreateWindowExW(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class
        L"GameBoy Emulator",               // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 360, // Size and position
        nullptr,                        // Parent window    
        nullptr,                        // Menu
        hInstance,                      // Instance handle
        nullptr                         // Additional application data
    );

    if (m_hwnd == nullptr) {
        return false;
    }

    Emulator::getInstance().initialize(m_hwnd);
    onFileOpen();
    if (Emulator::getInstance().isPaused()) {
        Emulator::getInstance().togglePause();
    }
    
    // Create menu
    // HMENU hMenu = CreateMenu();
    // HMENU hFileMenu = CreatePopupMenu();
    // HMENU hEmulationMenu = CreatePopupMenu();
    
    // AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN, L"&Open ROM...");
    // AppendMenuW(hFileMenu, MF_STRING, ID_FILE_RESET, L"&Reset");
    // AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    // AppendMenuW(hFileMenu, MF_STRING, ID_FILE_EXIT, L"&Exit");
    
    // AppendMenuW(hEmulationMenu, MF_STRING, ID_EMULATION_PAUSE, L"&Pause");
    
    // AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    // AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEmulationMenu, L"&Emulation");
    
    // SetMenu(m_hwnd, hMenu);
    
    // Show window
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    
    return true;
}

// Message loop
int MainWindow::messageLoop() {
    MSG msg = {};
    
    // Target 60 FPS (16.67 ms per frame)
    const auto targetFrameTime = std::chrono::microseconds(16667);
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    // Main message loop
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Get current time
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastFrameTime);
        
        // If enough time has passed, run a frame
        if (elapsedTime >= targetFrameTime) {
            Emulator::getInstance().run();
            lastFrameTime = currentTime;
        }
        else {
            // Sleep to avoid maxing out CPU
            auto sleepTime = targetFrameTime - elapsedTime;
            if (sleepTime > std::chrono::microseconds(1000)) {
                Sleep(static_cast<DWORD>(sleepTime.count() / 1000));
            }
        }
    }
    
    return (int)msg.wParam;
}

// Set window title
void MainWindow::setTitle(const std::string& title) {
    m_title = title;
    
    // Convert to wide string
    std::wstring wTitle(title.begin(), title.end());
    
    // Set window title
    SetWindowTextW(m_hwnd, wTitle.c_str());
}

// Open file dialog
std::string MainWindow::openFileDialog(const std::string& filter) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return "";
    }
    
    // Create file dialog
    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr)) {
        CoUninitialize();
        return "";
    }
    
    // Set file types
    COMDLG_FILTERSPEC fileTypes[] = {
        { L"GameBoy ROM Files", L"*.gb;*.gbc" },
        { L"All Files", L"*.*" }
    };
    pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
    
    // Show dialog
    hr = pFileOpen->Show(m_hwnd);
    if (FAILED(hr)) {
        pFileOpen->Release();
        CoUninitialize();
        return "";
    }
    
    // Get result
    IShellItem* pItem = nullptr;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) {
        pFileOpen->Release();
        CoUninitialize();
        return "";
    }
    
    // Get file path
    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr)) {
        pItem->Release();
        pFileOpen->Release();
        CoUninitialize();
        return "";
    }
    
    // Convert to string
    std::wstring wFilePath(pszFilePath);
    // Convert wide string to string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), (int)wFilePath.size(), nullptr, 0, nullptr, nullptr);
    std::string filePath(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), (int)wFilePath.size(), &filePath[0], size_needed, nullptr, nullptr);
    
    // Clean up
    CoTaskMemFree(pszFilePath);
    pItem->Release();
    pFileOpen->Release();
    CoUninitialize();
    
    return filePath;
}

// Window procedure
LRESULT CALLBACK MainWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;

    // Get window instance
    if (message == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    // } else {
    //     window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    // }

    // Call instance window procedure
    // if (window) {
    //     return window->handleMessage(message, wParam, lParam);
    // }

    MainWindow::getInstance().handleMessage(message, wParam, lParam);

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// Handle window message
LRESULT MainWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            onCreate();
            return 0;
            
        case WM_DESTROY:
            onDestroy();
            return 0;
            
        case WM_SIZE:
            onSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_COMMAND:
            onCommand(LOWORD(wParam));
            return 0;
            
        case WM_KEYDOWN:
            onKeyDown(static_cast<int>(wParam));
            return 0;
            
        case WM_KEYUP:
            onKeyUp(static_cast<int>(wParam));
            return 0;
    }
    
    return DefWindowProc(m_hwnd, message, wParam, lParam);
}

// On create
void MainWindow::onCreate() { 
}

// On destroy
void MainWindow::onDestroy() {
    // Shutdown emulator
    Emulator::getInstance().shutdown();
    
    // Post quit message
    PostQuitMessage(0);
}

// On size
void MainWindow::onSize(int width, int height) {
    m_width = width;
    m_height = height;
}

// On command
void MainWindow::onCommand(int id) {
    switch (id) {
        case ID_FILE_OPEN:
            onFileOpen();
            break;
            
        case ID_FILE_RESET:
            onFileReset();
            break;
            
        case ID_FILE_EXIT:
            onFileExit();
            break;
            
        case ID_EMULATION_PAUSE:
            onEmulationPause();
            break;
    }
}

// On key down
void MainWindow::onKeyDown(int key) {
    // Handle key down
}

// On key up
void MainWindow::onKeyUp(int key) {
    // Handle key up
}

// On file open
void MainWindow::onFileOpen() {
    // Open file dialog
    std::string filename = openFileDialog("GameBoy ROM Files (*.gb;*.gbc)|*.gb;*.gbc|All Files (*.*)|*.*");
    if (filename.empty()) {
        return;
    }
    
    // Load ROM
    if (Emulator::getInstance().loadROM(filename)) {
        // Set window title
        setTitle("GameBoy Emulator - " + filename);
    }
    else {
        // Show error message
        MessageBoxW(m_hwnd, L"Failed to load ROM", L"Error", MB_OK | MB_ICONERROR);
        exit(1);
    }
}

// On file reset
void MainWindow::onFileReset() {
    // Reset emulator
    Emulator::getInstance().reset();
}

// On file exit
void MainWindow::onFileExit() {
    // Close window
    DestroyWindow(m_hwnd);
}

// On emulation pause
void MainWindow::onEmulationPause() {
    // Toggle pause
    Emulator::getInstance().togglePause();
    
    // Update menu
    HMENU hMenu = GetMenu(m_hwnd);
    HMENU hEmulationMenu = GetSubMenu(hMenu, 1);
    
    if (Emulator::getInstance().isPaused()) {
        ModifyMenuW(hEmulationMenu, ID_EMULATION_PAUSE, MF_BYCOMMAND | MF_STRING, ID_EMULATION_PAUSE, L"&Resume");
    } else {
        ModifyMenuW(hEmulationMenu, ID_EMULATION_PAUSE, MF_BYCOMMAND | MF_STRING, ID_EMULATION_PAUSE, L"&Pause");
    }
    
    DrawMenuBar(m_hwnd);
} 