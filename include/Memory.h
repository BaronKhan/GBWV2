#pragma once

#include "Common.h"

// Forward declarations
class Cartridge;
class GPU;

// Memory Management Unit (MMU) class
class Memory {
public:
    // Meyer's Singleton pattern
    static Memory& getInstance() {
        static Memory instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    // Memory access methods
    u8 read(u16 address) const;
    void write(u16 address, u8 value);

    // Load ROM file
    bool loadROM(const std::string& filename);

    // Boot ROM control
    void disableBootROM();
    bool isBootROMEnabled() const { return m_bootROMEnabled; }

    // Reset memory
    void reset();

private:
    // Private constructor for singleton
    Memory();

    // Memory regions
    std::unique_ptr<Cartridge> m_cartridge;
    std::array<u8, VRAM_SIZE> m_vram;     // Video RAM (8KB)
    std::array<u8, WRAM_SIZE> m_wram;     // Work RAM (8KB)
    std::array<u8, OAM_SIZE> m_oam;       // Object Attribute Memory (160B)
    std::array<u8, IO_SIZE> m_io;         // I/O Registers (128B)
    std::array<u8, HRAM_SIZE> m_hram;     // High RAM (127B)
    u8 m_ie;                              // Interrupt Enable register

    // Boot ROM control
    bool m_bootROMEnabled;
};

// Cartridge class (ROM + RAM)
class Cartridge {
public:
    enum class Type {
        ROM_ONLY,
        MBC1,
        MBC2,
        MBC3,
        MBC5,
        UNKNOWN
    };

    Cartridge(const std::vector<u8>& romData);
    ~Cartridge() = default;

    // Memory access
    u8 read(u16 address) const;
    void write(u16 address, u8 value);

    // Cartridge info
    Type getType() const { return m_type; }
    const std::string& getTitle() const { return m_title; }
    u8 getROMBanks() const { return m_romBanks; }
    u8 getRAMBanks() const { return m_ramBanks; }

private:
    // ROM and RAM data
    std::vector<u8> m_rom;
    std::vector<u8> m_ram;

    // Cartridge info
    Type m_type;
    std::string m_title;
    u8 m_romBanks;
    u8 m_ramBanks;

    // MBC state
    u8 m_romBank;
    u8 m_ramBank;
    bool m_ramEnabled;
    bool m_romBankingMode;
}; 