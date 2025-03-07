#pragma once

#include "Common.h"
#include "Memory.h"

// GameBoy PPU (Picture Processing Unit) class
class PPU {
public:
    // Meyer's Singleton pattern
    static PPU& getInstance() {
        static PPU instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    PPU(const PPU&) = delete;
    PPU& operator=(const PPU&) = delete;

    // Initialize PPU
    void initialize();
    
    // Reset PPU state
    void reset();
    
    // Update PPU state based on CPU cycles
    void update(u32 cycles);
    
    // Get screen buffer
    const std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT>& getScreenBuffer() const { return m_screenBuffer; }
    
    // PPU modes
    enum class Mode {
        HBLANK = 0,     // Horizontal blank
        VBLANK = 1,     // Vertical blank
        OAM_SCAN = 2,   // Searching OAM for sprites
        PIXEL_TRANSFER = 3  // Transferring data to LCD driver
    };
    
    // Get current PPU mode
    Mode getMode() const { return m_mode; }
    
    // Get current scanline
    u8 getCurrentScanline() const { return m_scanline; }

private:
    // Private constructor for singleton
    PPU();
    
    // Reference to memory
    Memory& m_memory;
    
    // Screen buffer (160x144 pixels, 2 bits per pixel)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT> m_screenBuffer;
    
    // PPU state
    Mode m_mode;
    u8 m_scanline;
    u32 m_modeClock;
    
    // LCD Control register (LCDC) - 0xFF40
    bool isLCDEnabled() const;
    bool isWindowTileMapHigh() const;
    bool isWindowEnabled() const;
    bool isBGWindowTileDataHigh() const;
    bool isBGTileMapHigh() const;
    bool isSpriteSizeLarge() const;
    bool isSpritesEnabled() const;
    bool isBGWindowEnabled() const;
    
    // LCD Status register (STAT) - 0xFF41
    void updateLCDStatus();
    
    // Rendering methods
    void renderScanline();
    void renderBackground();
    void renderWindow();
    void renderSprites();
    
    // Helper methods
    u8 getColorFromPalette(u8 colorId, u8 palette) const;
    void setPixel(u16 x, u16 y, u8 colorId);
}; 