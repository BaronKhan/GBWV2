#pragma once

#include "Common.h"
#include "CPU.h"
#include "Memory.h"
#include "PPU.h"
#include <WebView2.h>
#include <wrl.h>
#include <windows.h>
#include <string>
#include <chrono>

// Emulator class
class Emulator {
public:
    // Meyer's Singleton pattern
    static Emulator& getInstance() {
        static Emulator instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;

    // Initialization and shutdown
    bool initialize(HWND hwnd);
    void shutdown();

    // Emulation control
    bool loadROM(const std::string& filename);
    void reset();
    void run();
    void pause();
    bool isPaused() const { return m_paused; }
    void togglePause() { m_paused = !m_paused; }

    // WebView2 event handlers
    HRESULT OnCreateWebView2ControlCompleted(HRESULT result, ICoreWebView2Controller* controller);
    HRESULT OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

private:
    // Private constructor for singleton
    Emulator();

    // Emulation state
    bool m_initialized;
    bool m_paused;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrameTime;
    
    // WebView2 components
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_webViewEnvironment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_webViewController;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;
    HWND m_hwnd;
    
    // References to other components
    CPU& m_cpu;
    Memory& m_memory;
    PPU& m_ppu;
    
    // Emulation methods
    void emulateFrame();
    void updateScreen();
    
    // WebView2 methods
    bool initializeWebView2();
    void resizeWebView();
    bool loadHTMLPage();
    void sendScreenDataToWebView();
}; 