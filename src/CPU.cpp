#include "CPU.h"
#include <fstream>
#include <iostream>

// CPU constructor
CPU::CPU() : m_halted(false), m_stopped(false), m_interruptsEnabled(false), 
             m_pendingInterruptEnable(false), m_cycles(0), m_memory(Memory::getInstance()) {
    reset();
    initializeOpcodes();
}

// Reset CPU
void CPU::reset() {
    // Reset registers
    m_registers.af = 0x01B0;
    m_registers.bc = 0x0013;
    m_registers.de = 0x00D8;
    m_registers.hl = 0x014D;
    m_registers.sp = 0xFFFE;
    m_registers.pc = 0x0000;
    
    // Reset CPU state
    m_halted = false;
    m_stopped = false;
    m_interruptsEnabled = false;
    m_pendingInterruptEnable = false;
    m_cycles = 0;
}

// Step CPU
void CPU::step() {
    // Handle interrupts
    handleInterrupts();
    
    // If CPU is halted or stopped, don't execute instructions
    if (m_halted || m_stopped) {
        m_cycles += 4;
        return;
    }
    
    // Fetch opcode
    u8 opcode = m_memory.read(m_registers.pc++);
    
    // Execute opcode
    executeOpcode(opcode);
    
    // Handle pending interrupt enable
    if (m_pendingInterruptEnable) {
        m_interruptsEnabled = true;
        m_pendingInterruptEnable = false;
    }
}

// Handle interrupts
void CPU::handleInterrupts() {
    if (!m_interruptsEnabled) {
        return;
    }
    
    // Get interrupt flags and enabled interrupts
    u8 interruptFlag = m_memory.read(0xFF0F);
    u8 interruptEnable = m_memory.read(0xFFFF);
    u8 interrupts = interruptFlag & interruptEnable;
    
    if (interrupts == 0) {
        return;
    }
    
    // Unhalt CPU
    m_halted = false;
    
    // Check each interrupt
    for (u8 i = 0; i < 5; i++) {
        if (bit_test(interrupts, i)) {
            // Disable interrupts
            m_interruptsEnabled = false;
            
            // Clear interrupt flag
            interruptFlag = bit_reset(interruptFlag, i);
            m_memory.write(0xFF0F, interruptFlag);
            
            // Push PC to stack
            push(m_registers.pc);
            
            // Jump to interrupt handler
            switch (i) {
                case 0: m_registers.pc = 0x0040; break; // V-Blank
                case 1: m_registers.pc = 0x0048; break; // LCD STAT
                case 2: m_registers.pc = 0x0050; break; // Timer
                case 3: m_registers.pc = 0x0058; break; // Serial
                case 4: m_registers.pc = 0x0060; break; // Joypad
            }
            
            // Add cycles
            m_cycles += 20;
            
            // Only handle one interrupt at a time
            break;
        }
    }
}

// Request interrupt
void CPU::requestInterrupt(u8 interrupt) {
    u8 interruptFlag = m_memory.read(0xFF0F);
    interruptFlag = bit_set(interruptFlag, interrupt);
    m_memory.write(0xFF0F, interruptFlag);
}

// Load opcodes from JSON
bool CPU::loadOpcodes(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open opcode file: " << filename << std::endl;
        return false;
    }
    
    try {
        nlohmann::json json;
        file >> json;
        parseOpcodeJson(json);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse opcode file: " << e.what() << std::endl;
        return false;
    }
}

// Parse opcode JSON
void CPU::parseOpcodeJson(const nlohmann::json& json) {
    // Parse unprefixed opcodes
    auto unprefixed = json["unprefixed"];
    for (auto it = unprefixed.begin(); it != unprefixed.end(); ++it) {
        // Get opcode number
        u8 opcode = std::stoi(it.key(), nullptr, 16);
        
        // Get mnemonic
        std::string mnemonic = it.value()["mnemonic"];
        
        // Map mnemonic to function (this is a simplified version)
        // In a full implementation, you would map all mnemonics to their functions
        if (mnemonic == "NOP") {
            m_opcodeTable[opcode] = [this]() { NOP(); };
        } else if (mnemonic == "LD BC,d16") {
            m_opcodeTable[opcode] = [this]() { LD_BC_d16(); };
        } else if (mnemonic == "LD DE,d16") {
            m_opcodeTable[opcode] = [this]() { LD_DE_d16(); };
        } else if (mnemonic == "LD HL,d16") {
            m_opcodeTable[opcode] = [this]() { LD_HL_d16(); };
        } else if (mnemonic == "LD SP,d16") {
            m_opcodeTable[opcode] = [this]() { LD_SP_d16(); };
        } else if (mnemonic == "XOR A") {
            m_opcodeTable[opcode] = [this]() { XOR_A(); };
        } else if (mnemonic == "JP a16") {
            m_opcodeTable[opcode] = [this]() { JP_a16(); };
        } else if (mnemonic == "DI") {
            m_opcodeTable[opcode] = [this]() { DI(); };
        } else if (mnemonic == "EI") {
            m_opcodeTable[opcode] = [this]() { EI(); };
        }
        // Add more mappings as needed
    }
    
    // Parse CB-prefixed opcodes
    auto cbprefixed = json["cbprefixed"];
    for (auto it = cbprefixed.begin(); it != cbprefixed.end(); ++it) {
        // Get opcode number
        u8 opcode = std::stoi(it.key(), nullptr, 16);
        
        // Get mnemonic
        std::string mnemonic = it.value()["mnemonic"];
        
        // Map mnemonic to function (this is a simplified version)
        if (mnemonic == "BIT 7,H") {
            m_cbOpcodeTable[opcode] = [this]() { BIT_7_H(); };
        }
        // Add more mappings as needed
    }
}

// Initialize opcodes
void CPU::initializeOpcodes() {
    // Initialize opcode tables with NOP
    for (u16 i = 0; i < 256; i++) {
        m_opcodeTable[i] = [this]() { NOP(); };
        m_cbOpcodeTable[i] = [this]() { NOP(); };
    }
    
    // Set up opcode functions
    // This is a minimal set of opcodes needed for the boot ROM
    // In a full implementation, you would set up all opcodes
    
    // 0x00: NOP
    m_opcodeTable[0x00] = [this]() { NOP(); };
    
    // 0x01: LD BC,d16
    m_opcodeTable[0x01] = [this]() { LD_BC_d16(); };
    
    // 0x11: LD DE,d16
    m_opcodeTable[0x11] = [this]() { LD_DE_d16(); };
    
    // 0x21: LD HL,d16
    m_opcodeTable[0x21] = [this]() { LD_HL_d16(); };
    
    // 0x31: LD SP,d16
    m_opcodeTable[0x31] = [this]() { LD_SP_d16(); };
    
    // 0x32: LD (HL-),A
    m_opcodeTable[0x32] = [this]() { LD_HL_A(); };
    
    // 0x3E: LD A,d8
    m_opcodeTable[0x3E] = [this]() { LD_A_d8(); };
    
    // 0xAF: XOR A
    m_opcodeTable[0xAF] = [this]() { XOR_A(); };
    
    // 0xC3: JP a16
    m_opcodeTable[0xC3] = [this]() { JP_a16(); };
    
    // 0xCB: PREFIX CB
    m_opcodeTable[0xCB] = [this]() {
        u8 opcode = readPC();
        executeCBOpcode(opcode);
    };
    
    // 0xF3: DI
    m_opcodeTable[0xF3] = [this]() { DI(); };
    
    // 0xFB: EI
    m_opcodeTable[0xFB] = [this]() { EI(); };
    
    // CB-prefixed opcodes
    
    // 0xCB 0x7C: BIT 7,H
    m_cbOpcodeTable[0x7C] = [this]() { BIT_7_H(); };
}

// Execute opcode
void CPU::executeOpcode(u8 opcode) {
    // Call opcode function
    m_opcodeTable[opcode]();
}

// Execute CB opcode
void CPU::executeCBOpcode(u8 opcode) {
    // Call CB opcode function
    m_cbOpcodeTable[opcode]();
}

// Read from PC
u8 CPU::readPC() {
    return m_memory.read(m_registers.pc++);
}

// Read 16-bit value from PC
u16 CPU::readPC16() {
    u8 low = readPC();
    u8 high = readPC();
    return (high << 8) | low;
}

// Push value to stack
void CPU::push(u16 value) {
    m_registers.sp -= 2;
    m_memory.write(m_registers.sp, value & 0xFF);
    m_memory.write(m_registers.sp + 1, value >> 8);
}

// Pop value from stack
u16 CPU::pop() {
    u8 low = m_memory.read(m_registers.sp);
    u8 high = m_memory.read(m_registers.sp + 1);
    m_registers.sp += 2;
    return (high << 8) | low;
}

// Get flag
bool CPU::getFlag(Flags flag) const {
    return bit_test(m_registers.f, flag);
}

// Set flag
void CPU::setFlag(Flags flag, bool value) {
    if (value) {
        m_registers.f = bit_set(m_registers.f, flag);
    } else {
        m_registers.f = bit_reset(m_registers.f, flag);
    }
}

// Instruction implementations

// NOP
void CPU::NOP() {
    m_cycles += 4;
}

// LD BC,d16
void CPU::LD_BC_d16() {
    m_registers.bc = readPC16();
    m_cycles += 12;
}

// LD DE,d16
void CPU::LD_DE_d16() {
    m_registers.de = readPC16();
    m_cycles += 12;
}

// LD HL,d16
void CPU::LD_HL_d16() {
    m_registers.hl = readPC16();
    m_cycles += 12;
}

// LD SP,d16
void CPU::LD_SP_d16() {
    m_registers.sp = readPC16();
    m_cycles += 12;
}

// LD A,d8
void CPU::LD_A_d8() {
    m_registers.a = readPC();
    m_cycles += 8;
}

// LD (HL-),A
void CPU::LD_HL_A() {
    m_memory.write(m_registers.hl--, m_registers.a);
    m_cycles += 8;
}

// XOR A
void CPU::XOR_A() {
    m_registers.a ^= m_registers.a;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// JP a16
void CPU::JP_a16() {
    m_registers.pc = readPC16();
    m_cycles += 16;
}

// DI
void CPU::DI() {
    m_interruptsEnabled = false;
    m_cycles += 4;
}

// EI
void CPU::EI() {
    m_pendingInterruptEnable = true;
    m_cycles += 4;
}

// BIT 7,H
void CPU::BIT_7_H() {
    bool bit = bit_test(m_registers.h, 7);
    
    // Set flags
    setFlag(FLAG_Z, !bit);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
} 