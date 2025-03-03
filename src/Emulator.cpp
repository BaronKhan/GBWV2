#include "Emulator.h"
#include "json.hpp"
#include <sstream>
#include <thread>

// Emulator constructor
Emulator::Emulator() : m_initialized(false), m_paused(true), 
                       m_cpu(CPU::getInstance()), m_gpu(GPU::getInstance()), m_memory(Memory::getInstance()) {
}

// Initialize emulator
bool Emulator::initialize(HWND hwnd) {
    if (m_initialized) {
        return true;
    }
    
    m_hwnd = hwnd;
    
    // Initialize WebView2
    if (!initializeWebView2()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

// Shutdown emulator
void Emulator::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // Release WebView2 resources
    if (m_webView) {
        m_webView.Reset();
    }
    
    if (m_webViewController) {
        m_webViewController.Reset();
    }
    
    if (m_webViewEnvironment) {
        m_webViewEnvironment.Reset();
    }
    
    m_initialized = false;
}

// Load ROM
bool Emulator::loadROM(const std::string& filename) {
    if (!m_initialized) {
        return false;
    }
    
    // Load ROM into memory
    if (!m_memory.loadROM(filename)) {
        return false;
    }
    
    // Load opcodes
    if (!m_cpu.loadOpcodes("resources/Opcodes.json")) {
        // If loading opcodes fails, we'll use the hardcoded ones
        std::cerr << "Using hardcoded opcodes" << std::endl;
    }
    
    // Reset emulator
    reset();
    
    return true;
}

// Reset emulator
void Emulator::reset() {
    if (!m_initialized) {
        return;
    }
    
    // Reset components
    m_cpu.reset();
    m_gpu.reset();
    m_memory.reset();
    
    // Reset emulator state
    m_paused = true;
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
}

// Run emulator
void Emulator::run() {
    if (!m_initialized || m_paused) {
        return;
    }
    
    // Emulate one frame
    emulateFrame();
    
    // Update screen
    updateScreen();
}

// Pause emulator
void Emulator::pause() {
    m_paused = true;
}

// Emulate one frame
void Emulator::emulateFrame() {
    // Calculate time since last frame
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_lastFrameTime).count();
    m_lastFrameTime = currentTime;
    
    // Target 60 FPS (16.67 ms per frame)
    const u32 targetCycles = 70224; // Cycles per frame at 60 FPS
    
    // Emulate CPU and GPU cycles
    u32 cycles = 0;
    while (cycles < targetCycles) {
        // Step CPU
        m_cpu.step();
        
        // Get cycles elapsed
        u32 elapsed = m_cpu.getCycles() - cycles;
        cycles = m_cpu.getCycles();
        
        // Step GPU
        m_gpu.step(elapsed);
    }
}

// Update screen
void Emulator::updateScreen() {
    // Send screen data to WebView
    sendScreenDataToWebView();
}

// Initialize WebView2
bool Emulator::initializeWebView2() {
    // Create WebView2 environment
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (SUCCEEDED(result)) {
                    // Store environment
                    m_webViewEnvironment = environment;
                    
                    // Create WebView2 controller
                    m_webViewEnvironment->CreateCoreWebView2Controller(m_hwnd,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                return OnCreateWebView2ControlCompleted(result, controller);
                            }).Get());
                }
                return S_OK;
            }).Get());
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create WebView2 environment" << std::endl;
        return false;
    }
    
    return true;
}

// WebView2 controller created
HRESULT Emulator::OnCreateWebView2ControlCompleted(HRESULT result, ICoreWebView2Controller* controller) {
    if (SUCCEEDED(result)) {
        // Store controller
        m_webViewController = controller;
        
        // Get WebView2
        m_webViewController->get_CoreWebView2(&m_webView);
        
        // Set up event handlers
        m_webView->add_WebMessageReceived(
            Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    return OnWebMessageReceived(sender, args);
                }).Get(), nullptr);
        
        // Resize WebView
        resizeWebView();
        
        // Load HTML page
        loadHTMLPage();
    } else {
        std::cerr << "Failed to create WebView2 controller" << std::endl;
    }
    
    return S_OK;
}

// WebView2 message received
HRESULT Emulator::OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
    // Get message
    LPWSTR message;
    args->TryGetWebMessageAsString(&message);
    
    // Convert to string
    std::wstring wstr(message);
    std::string str(wstr.begin(), wstr.end());
    
    // Parse message
    try {
        nlohmann::json json = nlohmann::json::parse(str);
        
        // Handle message
        if (json["type"] == "ready") {
            // WebView is ready, send initial screen data
            sendScreenDataToWebView();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse WebView message: " << e.what() << std::endl;
    }
    
    // Free message
    CoTaskMemFree(message);
    
    return S_OK;
}

// Resize WebView
void Emulator::resizeWebView() {
    if (!m_webViewController) {
        return;
    }
    
    // Get window size
    RECT bounds;
    GetClientRect(m_hwnd, &bounds);
    
    // Resize WebView
    m_webViewController->put_Bounds(bounds);
}

// Load HTML page
bool Emulator::loadHTMLPage() {
    if (!m_webView) {
        return false;
    }
    
    // Get current directory
    wchar_t path[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, path);
    
    // Build HTML file path
    std::wstring htmlPath = std::wstring(path) + L"\\resources\\index.html";
    
    // Navigate to HTML file
    m_webView->Navigate(htmlPath.c_str());
    
    return true;
}

// Send screen data to WebView
void Emulator::sendScreenDataToWebView() {
    if (!m_webView) {
        return;
    }
    
    // Get screen buffer
    const auto& screenBuffer = m_gpu.getScreenBuffer();
    
    // Create JSON message
    nlohmann::json message;
    message["type"] = "screenUpdate";
    message["pixels"] = nlohmann::json::array();
    
    // Add pixel data
    for (const auto& pixel : screenBuffer) {
        message["pixels"].push_back(pixel);
    }
    
    // Convert to string
    std::string messageStr = message.dump();
    std::wstring messageWstr(messageStr.begin(), messageStr.end());
    
    // Send message to WebView
    m_webView->PostWebMessageAsJson(messageWstr.c_str());
} 