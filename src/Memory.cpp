#include "Memory.h"
#include <fstream>
#include <iostream>

// Memory constructor
Memory::Memory() : m_bootROMEnabled(true) {
    reset();
}

// Reset memory
void Memory::reset() {
    // Clear memory regions
    m_vram.fill(0);
    m_wram.fill(0);
    m_oam.fill(0);
    m_io.fill(0);
    m_hram.fill(0);
    m_ie = 0;
    
    // Reset boot ROM state
    m_bootROMEnabled = true;
}

// Read from memory
u8 Memory::read(u16 address) const {
    // Boot ROM (0x0000 - 0x00FF)
    if (m_bootROMEnabled && address < 0x0100) {
        return BOOT_ROM[address];
    }
    
    // ROM banks (0x0000 - 0x7FFF)
    if (address < 0x8000) {
        if (m_cartridge) {
            return m_cartridge->read(address);
        }
        return 0xFF;
    }
    
    // Video RAM (0x8000 - 0x9FFF)
    if (address < 0xA000) {
        return m_vram[address - 0x8000];
    }
    
    // External RAM (0xA000 - 0xBFFF)
    if (address < 0xC000) {
        if (m_cartridge) {
            return m_cartridge->read(address);
        }
        return 0xFF;
    }
    
    // Work RAM (0xC000 - 0xDFFF)
    if (address < 0xE000) {
        return m_wram[address - 0xC000];
    }
    
    // Echo RAM (0xE000 - 0xFDFF) - mirror of 0xC000 - 0xDDFF
    if (address < 0xFE00) {
        return m_wram[address - 0xE000];
    }
    
    // Object Attribute Memory (0xFE00 - 0xFE9F)
    if (address < 0xFEA0) {
        return m_oam[address - 0xFE00];
    }
    
    // Not usable (0xFEA0 - 0xFEFF)
    if (address < 0xFF00) {
        return 0xFF;
    }
    
    // I/O Registers (0xFF00 - 0xFF7F)
    if (address < 0xFF80) {
        return m_io[address - 0xFF00];
    }
    
    // High RAM (0xFF80 - 0xFFFE)
    if (address < 0xFFFF) {
        return m_hram[address - 0xFF80];
    }
    
    // Interrupt Enable register (0xFFFF)
    return m_ie;
}

// Write to memory
void Memory::write(u16 address, u8 value) {
    // ROM banks (0x0000 - 0x7FFF)
    if (address < 0x8000) {
        if (m_cartridge) {
            m_cartridge->write(address, value);
        }
        return;
    }
    
    // Video RAM (0x8000 - 0x9FFF)
    if (address < 0xA000) {
        m_vram[address - 0x8000] = value;
        return;
    }
    
    // External RAM (0xA000 - 0xBFFF)
    if (address < 0xC000) {
        if (m_cartridge) {
            m_cartridge->write(address, value);
        }
        return;
    }
    
    // Work RAM (0xC000 - 0xDFFF)
    if (address < 0xE000) {
        m_wram[address - 0xC000] = value;
        return;
    }
    
    // Echo RAM (0xE000 - 0xFDFF) - mirror of 0xC000 - 0xDDFF
    if (address < 0xFE00) {
        m_wram[address - 0xE000] = value;
        return;
    }
    
    // Object Attribute Memory (0xFE00 - 0xFE9F)
    if (address < 0xFEA0) {
        m_oam[address - 0xFE00] = value;
        return;
    }
    
    // Not usable (0xFEA0 - 0xFEFF)
    if (address < 0xFF00) {
        return;
    }
    
    // I/O Registers (0xFF00 - 0xFF7F)
    if (address < 0xFF80) {
        // Special handling for some I/O registers
        if (address == 0xFF50 && value != 0) {
            // Disable boot ROM
            m_bootROMEnabled = false;
        }
        
        m_io[address - 0xFF00] = value;
        return;
    }
    
    // High RAM (0xFF80 - 0xFFFE)
    if (address < 0xFFFF) {
        m_hram[address - 0xFF80] = value;
        return;
    }
    
    // Interrupt Enable register (0xFFFF)
    m_ie = value;
}

// Disable boot ROM
void Memory::disableBootROM() {
    m_bootROMEnabled = false;
}

// Load ROM file
bool Memory::loadROM(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename << std::endl;
        return false;
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read file data
    std::vector<u8> romData(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Failed to read ROM file: " << filename << std::endl;
        return false;
    }
    
    // Create cartridge
    try {
        m_cartridge = std::make_unique<Cartridge>(romData);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create cartridge: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

// Cartridge constructor
Cartridge::Cartridge(const std::vector<u8>& romData) {
    // Copy ROM data
    m_rom = romData;
    
    // Parse cartridge header
    if (m_rom.size() < 0x150) {
        throw EmulatorException("Invalid ROM size");
    }
    
    // Get cartridge title
    m_title.reserve(16);
    for (u16 i = 0x134; i < 0x144; ++i) {
        if (m_rom[i] == 0) break;
        m_title.push_back(static_cast<char>(m_rom[i]));
    }
    
    // Get cartridge type
    u8 cartridgeType = m_rom[0x147];
    switch (cartridgeType) {
        case 0x00: m_type = Type::ROM_ONLY; break;
        case 0x01: case 0x02: case 0x03: m_type = Type::MBC1; break;
        case 0x05: case 0x06: m_type = Type::MBC2; break;
        case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: m_type = Type::MBC3; break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: m_type = Type::MBC5; break;
        default: m_type = Type::UNKNOWN; break;
    }
    
    // Get ROM size
    u8 romSize = m_rom[0x148];
    m_romBanks = 2 << romSize;
    
    // Get RAM size
    u8 ramSize = m_rom[0x149];
    switch (ramSize) {
        case 0x00: m_ramBanks = 0; break;
        case 0x01: m_ramBanks = 1; break;  // 2KB
        case 0x02: m_ramBanks = 1; break;  // 8KB
        case 0x03: m_ramBanks = 4; break;  // 32KB
        case 0x04: m_ramBanks = 16; break; // 128KB
        case 0x05: m_ramBanks = 8; break;  // 64KB
        default: m_ramBanks = 0; break;
    }
    
    // Initialize RAM
    if (m_ramBanks > 0) {
        m_ram.resize(m_ramBanks * RAM_BANK_SIZE, 0);
    }
    
    // Initialize MBC state
    m_romBank = 1;
    m_ramBank = 0;
    m_ramEnabled = false;
    m_romBankingMode = true;
}

// Read from cartridge
u8 Cartridge::read(u16 address) const {
    // ROM bank 0 (0x0000 - 0x3FFF)
    if (address < 0x4000) {
        return m_rom[address];
    }
    
    // ROM bank 1-N (0x4000 - 0x7FFF)
    if (address < 0x8000) {
        u32 romAddress = (m_romBank * ROM_BANK_SIZE) + (address - 0x4000);
        if (romAddress < m_rom.size()) {
            return m_rom[romAddress];
        }
        return 0xFF;
    }
    
    // RAM banks (0xA000 - 0xBFFF)
    if (address >= 0xA000 && address < 0xC000) {
        if (m_ramEnabled && m_ramBanks > 0) {
            u32 ramAddress = (m_ramBank * RAM_BANK_SIZE) + (address - 0xA000);
            if (ramAddress < m_ram.size()) {
                return m_ram[ramAddress];
            }
        }
        return 0xFF;
    }
    
    return 0xFF;
}

// Write to cartridge
void Cartridge::write(u16 address, u8 value) {
    // MBC1 implementation
    if (m_type == Type::MBC1) {
        // RAM enable (0x0000 - 0x1FFF)
        if (address < 0x2000) {
            m_ramEnabled = ((value & 0x0F) == 0x0A);
            return;
        }
        
        // ROM bank number (0x2000 - 0x3FFF)
        if (address < 0x4000) {
            u8 bank = value & 0x1F;
            if (bank == 0) bank = 1;
            m_romBank = (m_romBank & 0x60) | bank;
            return;
        }
        
        // RAM bank number or upper bits of ROM bank number (0x4000 - 0x5FFF)
        if (address < 0x6000) {
            if (m_romBankingMode) {
                m_romBank = (m_romBank & 0x1F) | ((value & 0x03) << 5);
            } else {
                m_ramBank = value & 0x03;
            }
            return;
        }
        
        // ROM/RAM mode select (0x6000 - 0x7FFF)
        if (address < 0x8000) {
            m_romBankingMode = (value & 0x01) == 0;
            return;
        }
    }
    
    // RAM banks (0xA000 - 0xBFFF)
    if (address >= 0xA000 && address < 0xC000) {
        if (m_ramEnabled && m_ramBanks > 0) {
            u32 ramAddress = (m_ramBank * RAM_BANK_SIZE) + (address - 0xA000);
            if (ramAddress < m_ram.size()) {
                m_ram[ramAddress] = value;
            }
        }
    }
} 