#pragma once

#include "Common.h"
#include "Emulator.h"
#include <windows.h>
#include <string>

// MainWindow class
class MainWindow {
public:
    // Meyer's Singleton pattern
    static MainWindow& getInstance() {
        static MainWindow instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Window creation and message loop
    bool create(HINSTANCE hInstance, int nCmdShow);
    int messageLoop();

    // Window properties
    HWND getHandle() const { return m_hwnd; }
    void setTitle(const std::string& title);

    // File dialog
    std::string openFileDialog(const std::string& filter);

private:
    // Private constructor for singleton
    MainWindow();

    // Window properties
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    std::string m_title;
    int m_width;
    int m_height;

    // Window procedure
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // Message handlers
    void onCreate();
    void onDestroy();
    void onSize(int width, int height);
    void onCommand(int id);
    void onKeyDown(int key);
    void onKeyUp(int key);

    // Menu handlers
    void onFileOpen();
    void onFileReset();
    void onFileExit();
    void onEmulationPause();
}; 