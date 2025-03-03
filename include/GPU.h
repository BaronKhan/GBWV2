#pragma once

#include "Common.h"
#include "Memory.h"

// GPU (Graphics Processing Unit) class
class GPU {
public:
    // Meyer's Singleton pattern
    static GPU& getInstance() {
        static GPU instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    GPU(const GPU&) = delete;
    GPU& operator=(const GPU&) = delete;

    // GPU modes
    enum class Mode {
        HBLANK = 0,
        VBLANK = 1,
        OAM_SCAN = 2,
        PIXEL_TRANSFER = 3
    };

    // GPU control
    void reset();
    void step(u32 cycles);
    
    // Screen buffer access
    const std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT>& getScreenBuffer() const { return m_screenBuffer; }
    
    // Debug
    Mode getMode() const { return m_mode; }
    u32 getScanline() const { return m_scanline; }

private:
    // Private constructor for singleton
    GPU();

    // GPU state
    Mode m_mode;
    u32 m_modeClock;
    u32 m_scanline;
    
    // Screen buffer (2 bits per pixel, 4 colors)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT> m_screenBuffer;
    
    // Memory reference
    Memory& m_memory;
    
    // Rendering methods
    void renderScanline();
    void renderBackground(u32 scanline);
    void renderWindow(u32 scanline);
    void renderSprites(u32 scanline);
    
    // Helper methods
    u8 getColorFromPalette(u8 colorId, u8 palette);
    void setPixel(u32 x, u32 y, u8 colorId);
    
    // LCD Control register bits
    bool isLCDEnabled() const;
    bool isWindowEnabled() const;
    bool isSpritesEnabled() const;
    bool isBackgroundEnabled() const;
    u16 getBackgroundTileMapAddress() const;
    u16 getBackgroundTileDataAddress() const;
    u16 getWindowTileMapAddress() const;
    bool isSpritesLarge() const;
}; 