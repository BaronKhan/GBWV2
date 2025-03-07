#include "PPU.h"

// PPU constructor
PPU::PPU() : m_memory(Memory::getInstance()), m_mode(Mode::OAM_SCAN), m_scanline(0), m_modeClock(0) {
    // Initialize screen buffer to white
    m_screenBuffer.fill(0);
}

// Initialize PPU
void PPU::initialize() {
    reset();
}

// Reset PPU state
void PPU::reset() {
    m_mode = Mode::OAM_SCAN;
    m_scanline = 0;
    m_modeClock = 0;
    m_screenBuffer.fill(0);
}

// Update PPU state based on CPU cycles
void PPU::update(u32 cycles) {
    // If LCD is disabled, don't do anything
    if (!isLCDEnabled()) {
        return;
    }
    
    // Add cycles to mode clock
    m_modeClock += cycles;
    
    // Process based on current mode
    switch (m_mode) {
        case Mode::OAM_SCAN:
            // OAM scan takes 80 cycles
            if (m_modeClock >= 80) {
                m_modeClock -= 80;
                m_mode = Mode::PIXEL_TRANSFER;
                updateLCDStatus();
            }
            break;
            
        case Mode::PIXEL_TRANSFER:
            // Pixel transfer takes 172 cycles
            if (m_modeClock >= 172) {
                m_modeClock -= 172;
                m_mode = Mode::HBLANK;
                updateLCDStatus();
                
                // Render the current scanline
                renderScanline();
            }
            break;
            
        case Mode::HBLANK:
            // HBlank takes 204 cycles
            if (m_modeClock >= 204) {
                m_modeClock -= 204;
                
                // Increment scanline
                m_scanline++;
                
                // Check if we've reached VBlank
                if (m_scanline == 144) {
                    m_mode = Mode::VBLANK;
                    
                    // Request VBlank interrupt
                    m_memory.write(0xFF0F, m_memory.read(0xFF0F) | 0x01);
                } else {
                    m_mode = Mode::OAM_SCAN;
                }
                
                updateLCDStatus();
            }
            break;
            
        case Mode::VBLANK:
            // Each scanline in VBlank takes 456 cycles
            if (m_modeClock >= 456) {
                m_modeClock -= 456;
                
                // Increment scanline
                m_scanline++;
                
                // Check if VBlank is done
                if (m_scanline > 153) {
                    m_scanline = 0;
                    m_mode = Mode::OAM_SCAN;
                }
                
                updateLCDStatus();
            }
            break;
    }
    
    // Update LY register (current scanline)
    m_memory.write(0xFF44, m_scanline);
}

// Update LCD Status register
void PPU::updateLCDStatus() {
    // Get current STAT register value
    u8 stat = m_memory.read(0xFF41);
    
    // Clear mode bits (0-1)
    stat &= 0xFC;
    
    // Set mode bits
    stat |= static_cast<u8>(m_mode);
    
    // Set coincidence flag (bit 2)
    if (m_scanline == m_memory.read(0xFF45)) {
        stat |= 0x04;
        
        // Request STAT interrupt if coincidence interrupt is enabled
        if (stat & 0x40) {
            m_memory.write(0xFF0F, m_memory.read(0xFF0F) | 0x02);
        }
    } else {
        stat &= ~0x04;
    }
    
    // Check for mode interrupts
    if ((m_mode == Mode::HBLANK && (stat & 0x08)) ||
        (m_mode == Mode::VBLANK && (stat & 0x10)) ||
        (m_mode == Mode::OAM_SCAN && (stat & 0x20))) {
        m_memory.write(0xFF0F, m_memory.read(0xFF0F) | 0x02);
    }
    
    // Write updated STAT register
    m_memory.write(0xFF41, stat);
}

// Render current scanline
void PPU::renderScanline() {
    if (isBGWindowEnabled()) {
        renderBackground();
        
        if (isWindowEnabled()) {
            renderWindow();
        }
    }
    
    if (isSpritesEnabled()) {
        renderSprites();
    }
}

// Render background for current scanline
void PPU::renderBackground() {
    // Get LCDC register
    u8 lcdc = m_memory.read(0xFF40);
    
    // Get background palette
    u8 bgp = m_memory.read(0xFF47);
    
    // Get scroll positions
    u8 scrollY = m_memory.read(0xFF42);
    u8 scrollX = m_memory.read(0xFF43);
    
    // Calculate which tile row to use from background map
    u16 tileMapAddress = isBGTileMapHigh() ? 0x9C00 : 0x9800;
    u8 yPos = scrollY + m_scanline;
    u16 tileRow = (yPos / 8) * 32;
    
    // Calculate which row of pixels to use from the tile
    u8 tilePixelRow = yPos % 8;
    
    // Iterate through all 160 pixels in the current scanline
    for (u16 x = 0; x < SCREEN_WIDTH; x++) {
        // Calculate x position with scroll
        u8 xPos = x + scrollX;
        
        // Get tile index from background map
        u16 tileCol = xPos / 8;
        u16 tileAddress = tileMapAddress + tileRow + tileCol;
        u8 tileIndex = m_memory.read(tileAddress);
        
        // Get tile data address
        u16 tileDataAddress;
        if (isBGWindowTileDataHigh()) {
            // 0x8000 addressing mode (unsigned)
            tileDataAddress = 0x8000 + (tileIndex * 16);
        } else {
            // 0x8800 addressing mode (signed)
            tileDataAddress = 0x9000 + (static_cast<i8>(tileIndex) * 16);
        }
        
        // Get the specific byte for the current pixel row (each tile row is 2 bytes)
        u16 tileDataRowAddress = tileDataAddress + (tilePixelRow * 2);
        u8 tileLowByte = m_memory.read(tileDataRowAddress);
        u8 tileHighByte = m_memory.read(tileDataRowAddress + 1);
        
        // Get the specific bit for the current pixel column
        u8 tilePixelCol = 7 - (xPos % 8);
        u8 colorBit0 = (tileLowByte >> tilePixelCol) & 0x01;
        u8 colorBit1 = (tileHighByte >> tilePixelCol) & 0x01;
        u8 colorId = (colorBit1 << 1) | colorBit0;
        
        // Get actual color from palette
        u8 color = getColorFromPalette(colorId, bgp);
        
        // Set pixel in screen buffer
        setPixel(x, m_scanline, color);
    }
}

// Render window for current scanline
void PPU::renderWindow() {
    // Get window position
    u8 windowX = m_memory.read(0xFF4B) - 7;
    u8 windowY = m_memory.read(0xFF4A);
    
    // Check if window is visible on this scanline
    if (windowY > m_scanline) {
        return;
    }
    
    // Get LCDC register
    u8 lcdc = m_memory.read(0xFF40);
    
    // Get background palette
    u8 bgp = m_memory.read(0xFF47);
    
    // Calculate which tile row to use from window map
    u16 tileMapAddress = isWindowTileMapHigh() ? 0x9C00 : 0x9800;
    u8 yPos = m_scanline - windowY;
    u16 tileRow = (yPos / 8) * 32;
    
    // Calculate which row of pixels to use from the tile
    u8 tilePixelRow = yPos % 8;
    
    // Iterate through all pixels in the current scanline
    for (u16 x = 0; x < SCREEN_WIDTH; x++) {
        // Skip if pixel is outside window
        if (x < windowX) {
            continue;
        }
        
        // Calculate x position within window
        u8 xPos = x - windowX;
        
        // Get tile index from window map
        u16 tileCol = xPos / 8;
        u16 tileAddress = tileMapAddress + tileRow + tileCol;
        u8 tileIndex = m_memory.read(tileAddress);
        
        // Get tile data address
        u16 tileDataAddress;
        if (isBGWindowTileDataHigh()) {
            // 0x8000 addressing mode (unsigned)
            tileDataAddress = 0x8000 + (tileIndex * 16);
        } else {
            // 0x8800 addressing mode (signed)
            tileDataAddress = 0x9000 + (static_cast<i8>(tileIndex) * 16);
        }
        
        // Get the specific byte for the current pixel row (each tile row is 2 bytes)
        u16 tileDataRowAddress = tileDataAddress + (tilePixelRow * 2);
        u8 tileLowByte = m_memory.read(tileDataRowAddress);
        u8 tileHighByte = m_memory.read(tileDataRowAddress + 1);
        
        // Get the specific bit for the current pixel column
        u8 tilePixelCol = 7 - (xPos % 8);
        u8 colorBit0 = (tileLowByte >> tilePixelCol) & 0x01;
        u8 colorBit1 = (tileHighByte >> tilePixelCol) & 0x01;
        u8 colorId = (colorBit1 << 1) | colorBit0;
        
        // Get actual color from palette
        u8 color = getColorFromPalette(colorId, bgp);
        
        // Set pixel in screen buffer
        setPixel(x, m_scanline, color);
    }
}

// Render sprites for current scanline
void PPU::renderSprites() {
    // Get LCDC register
    u8 lcdc = m_memory.read(0xFF40);
    
    // Get sprite palettes
    u8 obp0 = m_memory.read(0xFF48);
    u8 obp1 = m_memory.read(0xFF49);
    
    // Determine sprite height (8x8 or 8x16)
    u8 spriteHeight = isSpriteSizeLarge() ? 16 : 8;
    
    // Maximum of 10 sprites per scanline
    u8 spritesOnLine = 0;
    
    // Iterate through all 40 sprites in OAM
    for (u16 i = 0; i < 40; i++) {
        // Get sprite attributes from OAM
        u16 oamAddress = 0xFE00 + (i * 4);
        u8 spriteY = m_memory.read(oamAddress) - 16;
        u8 spriteX = m_memory.read(oamAddress + 1) - 8;
        u8 tileIndex = m_memory.read(oamAddress + 2);
        u8 attributes = m_memory.read(oamAddress + 3);
        
        // Check if sprite is on current scanline
        if (m_scanline < spriteY || m_scanline >= spriteY + spriteHeight) {
            continue;
        }
        
        // Limit to 10 sprites per scanline
        if (spritesOnLine >= 10) {
            break;
        }
        
        spritesOnLine++;
        
        // Get sprite flags
        bool flipY = (attributes & 0x40) != 0;
        bool flipX = (attributes & 0x20) != 0;
        bool usePalette1 = (attributes & 0x10) != 0;
        bool behindBG = (attributes & 0x80) != 0;
        
        // Calculate which row of the sprite to use
        u8 tileRow = m_scanline - spriteY;
        if (flipY) {
            tileRow = spriteHeight - 1 - tileRow;
        }
        
        // For 8x16 sprites, the bottom half uses the next tile
        if (spriteHeight == 16) {
            if (tileRow >= 8) {
                tileIndex |= 1;  // Use next tile for bottom half
                tileRow -= 8;
            } else {
                tileIndex &= 0xFE;  // Use even tile for top half
            }
        }
        
        // Get tile data address (sprites always use 0x8000 addressing mode)
        u16 tileDataAddress = 0x8000 + (tileIndex * 16) + (tileRow * 2);
        u8 tileLowByte = m_memory.read(tileDataAddress);
        u8 tileHighByte = m_memory.read(tileDataAddress + 1);
        
        // Get palette
        u8 palette = usePalette1 ? obp1 : obp0;
        
        // Draw sprite pixels for this scanline
        for (u8 x = 0; x < 8; x++) {
            // Skip if pixel is off-screen
            if (spriteX + x >= SCREEN_WIDTH) {
                continue;
            }
            
            // Get the specific bit for the current pixel column
            u8 tilePixelCol = flipX ? x : (7 - x);
            u8 colorBit0 = (tileLowByte >> tilePixelCol) & 0x01;
            u8 colorBit1 = (tileHighByte >> tilePixelCol) & 0x01;
            u8 colorId = (colorBit1 << 1) | colorBit0;
            
            // Skip transparent pixels (color 0)
            if (colorId == 0) {
                continue;
            }
            
            // Get actual color from palette
            u8 color = getColorFromPalette(colorId, palette);
            
            // Set pixel in screen buffer (if not behind background or if background is transparent)
            if (!behindBG) {
                setPixel(spriteX + x, m_scanline, color);
            }
        }
    }
}

// Get color from palette
u8 PPU::getColorFromPalette(u8 colorId, u8 palette) const {
    // Each palette contains 4 colors, 2 bits per color
    u8 color = (palette >> (colorId * 2)) & 0x03;
    return color;
}

// Set pixel in screen buffer
void PPU::setPixel(u16 x, u16 y, u8 colorId) {
    if (x < SCREEN_WIDTH && y < SCREEN_HEIGHT) {
        m_screenBuffer[y * SCREEN_WIDTH + x] = colorId;
    }
}

// LCDC register bit checks
bool PPU::isLCDEnabled() const {
    return (m_memory.read(0xFF40) & 0x80) != 0;
}

bool PPU::isWindowTileMapHigh() const {
    return (m_memory.read(0xFF40) & 0x40) != 0;
}

bool PPU::isWindowEnabled() const {
    return (m_memory.read(0xFF40) & 0x20) != 0;
}

bool PPU::isBGWindowTileDataHigh() const {
    return (m_memory.read(0xFF40) & 0x10) != 0;
}

bool PPU::isBGTileMapHigh() const {
    return (m_memory.read(0xFF40) & 0x08) != 0;
}

bool PPU::isSpriteSizeLarge() const {
    return (m_memory.read(0xFF40) & 0x04) != 0;
}

bool PPU::isSpritesEnabled() const {
    return (m_memory.read(0xFF40) & 0x02) != 0;
}

bool PPU::isBGWindowEnabled() const {
    return (m_memory.read(0xFF40) & 0x01) != 0;
} 