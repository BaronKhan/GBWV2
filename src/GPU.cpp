#include "GPU.h"

// GPU constructor
GPU::GPU() : m_mode(Mode::OAM_SCAN), m_modeClock(0), m_scanline(0), m_memory(Memory::getInstance()) {
    reset();
}

// Reset GPU
void GPU::reset() {
    m_mode = Mode::OAM_SCAN;
    m_modeClock = 0;
    m_scanline = 0;
    m_screenBuffer.fill(0);
}

// Step GPU
void GPU::step(u32 cycles) {
    // Update mode clock
    m_modeClock += cycles;
    
    // Check if LCD is enabled
    if (!isLCDEnabled()) {
        m_modeClock = 0;
        m_scanline = 0;
        m_memory.write(0xFF44, 0); // LY = 0
        return;
    }
    
    // Update GPU state based on mode
    switch (m_mode) {
        case Mode::OAM_SCAN:
            // OAM scan (80 cycles)
            if (m_modeClock >= 80) {
                m_modeClock -= 80;
                m_mode = Mode::PIXEL_TRANSFER;
            }
            break;
            
        case Mode::PIXEL_TRANSFER:
            // Pixel transfer (172 cycles)
            if (m_modeClock >= 172) {
                m_modeClock -= 172;
                m_mode = Mode::HBLANK;
                
                // Render scanline
                renderScanline();
            }
            break;
            
        case Mode::HBLANK:
            // H-Blank (204 cycles)
            if (m_modeClock >= 204) {
                m_modeClock -= 204;
                
                // Increment scanline
                m_scanline++;
                m_memory.write(0xFF44, m_scanline); // Update LY register
                
                // Check if we've reached the end of the screen
                if (m_scanline == 144) {
                    m_mode = Mode::VBLANK;
                    
                    // Request V-Blank interrupt
                    u8 interruptFlag = m_memory.read(0xFF0F);
                    interruptFlag = bit_set(interruptFlag, 0); // V-Blank interrupt
                    m_memory.write(0xFF0F, interruptFlag);
                } else {
                    m_mode = Mode::OAM_SCAN;
                }
            }
            break;
            
        case Mode::VBLANK:
            // V-Blank (4560 cycles = 10 scanlines * 456 cycles)
            if (m_modeClock >= 456) {
                m_modeClock -= 456;
                
                // Increment scanline
                m_scanline++;
                m_memory.write(0xFF44, m_scanline); // Update LY register
                
                // Check if we've reached the end of V-Blank
                if (m_scanline > 153) {
                    m_scanline = 0;
                    m_memory.write(0xFF44, 0); // Reset LY register
                    m_mode = Mode::OAM_SCAN;
                }
            }
            break;
    }
    
    // Update LCD status register (STAT)
    u8 stat = m_memory.read(0xFF41);
    stat &= 0xFC; // Clear mode bits
    stat |= static_cast<u8>(m_mode);
    
    // Set coincidence flag
    if (m_scanline == m_memory.read(0xFF45)) { // LY == LYC
        stat = bit_set(stat, 2);
    } else {
        stat = bit_reset(stat, 2);
    }
    
    m_memory.write(0xFF41, stat);
}

// Render scanline
void GPU::renderScanline() {
    if (isBackgroundEnabled()) {
        renderBackground(m_scanline);
    }
    
    if (isWindowEnabled()) {
        renderWindow(m_scanline);
    }
    
    if (isSpritesEnabled()) {
        renderSprites(m_scanline);
    }
}

// Render background
void GPU::renderBackground(u32 scanline) {
    // Get background scroll coordinates
    u8 scrollX = m_memory.read(0xFF43);
    u8 scrollY = m_memory.read(0xFF42);
    
    // Get tile map and data addresses
    u16 tileMapAddress = getBackgroundTileMapAddress();
    u16 tileDataAddress = getBackgroundTileDataAddress();
    bool signedTileData = (tileDataAddress == 0x8800);
    
    // Calculate Y position in the background map
    u8 y = (scrollY + scanline) & 0xFF;
    u8 tileRow = y / 8;
    u8 tileY = y % 8;
    
    // Render each pixel in the scanline
    for (u32 x = 0; x < SCREEN_WIDTH; x++) {
        // Calculate X position in the background map
        u8 bgX = (scrollX + x) & 0xFF;
        u8 tileCol = bgX / 8;
        u8 tileX = bgX % 8;
        
        // Get tile index from the tile map
        u16 tileMapOffset = (tileRow * 32) + tileCol;
        u8 tileIndex = m_memory.read(tileMapAddress + tileMapOffset);
        
        // Get tile data address
        u16 tileAddress;
        if (signedTileData) {
            tileAddress = tileDataAddress + ((static_cast<i8>(tileIndex) + 128) * 16);
        } else {
            tileAddress = tileDataAddress + (tileIndex * 16);
        }
        
        // Get tile data for the current row
        u8 tileLow = m_memory.read(tileAddress + (tileY * 2));
        u8 tileHigh = m_memory.read(tileAddress + (tileY * 2) + 1);
        
        // Get color bit for the current pixel
        u8 colorBit = 7 - tileX;
        u8 colorId = ((bit_test(tileHigh, colorBit) ? 2 : 0) | (bit_test(tileLow, colorBit) ? 1 : 0));
        
        // Get color from the palette
        u8 color = getColorFromPalette(colorId, m_memory.read(0xFF47));
        
        // Set pixel in the screen buffer
        setPixel(x, scanline, color);
    }
}

// Render window
void GPU::renderWindow(u32 scanline) {
    // Get window position
    u8 windowX = m_memory.read(0xFF4B) - 7;
    u8 windowY = m_memory.read(0xFF4A);
    
    // Check if the window is visible on this scanline
    if (scanline < windowY) {
        return;
    }
    
    // Get tile map and data addresses
    u16 tileMapAddress = getWindowTileMapAddress();
    u16 tileDataAddress = getBackgroundTileDataAddress();
    bool signedTileData = (tileDataAddress == 0x8800);
    
    // Calculate Y position in the window map
    u8 y = scanline - windowY;
    u8 tileRow = y / 8;
    u8 tileY = y % 8;
    
    // Render each pixel in the scanline
    for (u32 x = 0; x < SCREEN_WIDTH; x++) {
        // Check if the pixel is within the window
        if (x < windowX) {
            continue;
        }
        
        // Calculate X position in the window map
        u8 windowX = x - windowX;
        u8 tileCol = windowX / 8;
        u8 tileX = windowX % 8;
        
        // Get tile index from the tile map
        u16 tileMapOffset = (tileRow * 32) + tileCol;
        u8 tileIndex = m_memory.read(tileMapAddress + tileMapOffset);
        
        // Get tile data address
        u16 tileAddress;
        if (signedTileData) {
            tileAddress = tileDataAddress + ((static_cast<i8>(tileIndex) + 128) * 16);
        } else {
            tileAddress = tileDataAddress + (tileIndex * 16);
        }
        
        // Get tile data for the current row
        u8 tileLow = m_memory.read(tileAddress + (tileY * 2));
        u8 tileHigh = m_memory.read(tileAddress + (tileY * 2) + 1);
        
        // Get color bit for the current pixel
        u8 colorBit = 7 - tileX;
        u8 colorId = ((bit_test(tileHigh, colorBit) ? 2 : 0) | (bit_test(tileLow, colorBit) ? 1 : 0));
        
        // Get color from the palette
        u8 color = getColorFromPalette(colorId, m_memory.read(0xFF47));
        
        // Set pixel in the screen buffer
        setPixel(x, scanline, color);
    }
}

// Render sprites
void GPU::renderSprites(u32 scanline) {
    // Get sprite size (8x8 or 8x16)
    u8 spriteHeight = isSpritesLarge() ? 16 : 8;
    
    // Count visible sprites on this scanline (max 10 per scanline)
    u32 visibleSprites = 0;
    
    // Check each sprite in OAM
    for (u32 i = 0; i < 40 && visibleSprites < 10; i++) {
        // Get sprite attributes
        u16 oamAddress = 0xFE00 + (i * 4);
        u8 spriteY = m_memory.read(oamAddress) - 16;
        u8 spriteX = m_memory.read(oamAddress + 1) - 8;
        u8 tileIndex = m_memory.read(oamAddress + 2);
        u8 attributes = m_memory.read(oamAddress + 3);
        
        // Check if sprite is visible on this scanline
        if (scanline < spriteY || scanline >= spriteY + spriteHeight) {
            continue;
        }
        
        // Increment visible sprite count
        visibleSprites++;
        
        // Get sprite flags
        bool flipX = bit_test(attributes, 5);
        bool flipY = bit_test(attributes, 6);
        bool priority = !bit_test(attributes, 7);
        
        // Get palette
        u8 palette = bit_test(attributes, 4) ? m_memory.read(0xFF49) : m_memory.read(0xFF48);
        
        // Calculate tile row
        u8 tileY = scanline - spriteY;
        if (flipY) {
            tileY = spriteHeight - 1 - tileY;
        }
        
        // For 8x16 sprites, adjust tile index
        if (spriteHeight == 16) {
            if (tileY >= 8) {
                tileIndex |= 1;
                tileY -= 8;
            } else {
                tileIndex &= 0xFE;
            }
        }
        
        // Get tile data address
        u16 tileAddress = 0x8000 + (tileIndex * 16);
        
        // Get tile data for the current row
        u8 tileLow = m_memory.read(tileAddress + (tileY * 2));
        u8 tileHigh = m_memory.read(tileAddress + (tileY * 2) + 1);
        
        // Render sprite pixels
        for (u32 x = 0; x < 8; x++) {
            // Check if pixel is visible on screen
            if (spriteX + x >= SCREEN_WIDTH) {
                continue;
            }
            
            // Get color bit
            u8 colorBit = flipX ? x : (7 - x);
            u8 colorId = ((bit_test(tileHigh, colorBit) ? 2 : 0) | (bit_test(tileLow, colorBit) ? 1 : 0));
            
            // Skip transparent pixels
            if (colorId == 0) {
                continue;
            }
            
            // Get color from the palette
            u8 color = getColorFromPalette(colorId, palette);
            
            // Set pixel in the screen buffer (if priority allows)
            if (priority || m_screenBuffer[scanline * SCREEN_WIDTH + (spriteX + x)] == 0) {
                setPixel(spriteX + x, scanline, color);
            }
        }
    }
}

// Get color from palette
u8 GPU::getColorFromPalette(u8 colorId, u8 palette) {
    // Extract color from palette
    u8 color = (palette >> (colorId * 2)) & 0x03;
    return color;
}

// Set pixel in screen buffer
void GPU::setPixel(u32 x, u32 y, u8 colorId) {
    if (x < SCREEN_WIDTH && y < SCREEN_HEIGHT) {
        m_screenBuffer[y * SCREEN_WIDTH + x] = colorId;
    }
}

// LCD Control register bits
bool GPU::isLCDEnabled() const {
    return bit_test(m_memory.read(0xFF40), 7);
}

bool GPU::isWindowEnabled() const {
    return bit_test(m_memory.read(0xFF40), 5);
}

bool GPU::isSpritesEnabled() const {
    return bit_test(m_memory.read(0xFF40), 1);
}

bool GPU::isBackgroundEnabled() const {
    return bit_test(m_memory.read(0xFF40), 0);
}

u16 GPU::getBackgroundTileMapAddress() const {
    return bit_test(m_memory.read(0xFF40), 3) ? 0x9C00 : 0x9800;
}

u16 GPU::getBackgroundTileDataAddress() const {
    return bit_test(m_memory.read(0xFF40), 4) ? 0x8000 : 0x8800;
}

u16 GPU::getWindowTileMapAddress() const {
    return bit_test(m_memory.read(0xFF40), 6) ? 0x9C00 : 0x9800;
}

bool GPU::isSpritesLarge() const {
    return bit_test(m_memory.read(0xFF40), 2);
}