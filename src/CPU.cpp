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
        
        // Get operands
        std::vector<std::string> operands;
        if (it.value().contains("operands")) {
            for (const auto& operand : it.value()["operands"]) {
                operands.push_back(operand["name"].get<std::string>());
            }
        }
        
        // Create full mnemonic with operands
        std::string fullMnemonic = mnemonic;
        if (!operands.empty()) {
            fullMnemonic += " ";
            for (size_t i = 0; i < operands.size(); ++i) {
                fullMnemonic += operands[i];
                if (i < operands.size() - 1) {
                    fullMnemonic += ",";
                }
            }
        }
        
        // Map the opcode to its implementation
        mapOpcodeToFunction(opcode, fullMnemonic, false);
    }
    
    // Parse CB-prefixed opcodes
    auto cbprefixed = json["cbprefixed"];
    for (auto it = cbprefixed.begin(); it != cbprefixed.end(); ++it) {
        // Get opcode number
        u8 opcode = std::stoi(it.key(), nullptr, 16);
        
        // Get mnemonic
        std::string mnemonic = it.value()["mnemonic"];
        
        // Get operands
        std::vector<std::string> operands;
        if (it.value().contains("operands")) {
            for (const auto& operand : it.value()["operands"]) {
                operands.push_back(operand["name"].get<std::string>());
            }
        }
        
        // Create full mnemonic with operands
        std::string fullMnemonic = mnemonic;
        if (!operands.empty()) {
            fullMnemonic += " ";
            for (size_t i = 0; i < operands.size(); ++i) {
                fullMnemonic += operands[i];
                if (i < operands.size() - 1) {
                    fullMnemonic += ",";
                }
            }
        }
        
        // Map the opcode to its implementation
        mapOpcodeToFunction(opcode, fullMnemonic, true);
    }
}

// Map opcode to function
void CPU::mapOpcodeToFunction(u8 opcode, const std::string& mnemonic, bool isCB) {
    // Choose the appropriate opcode table
    auto& opcodeTable = isCB ? m_cbOpcodeTable : m_opcodeTable;
    
    // Map the mnemonic to the corresponding function
    // Unprefixed opcodes
    if (!isCB) {
        // NOP
        if (mnemonic == "NOP") {
            opcodeTable[opcode] = [this]() { NOP(); };
        }
        // Load immediate 16-bit value into BC
        else if (mnemonic == "LD_BC_n16") {
            opcodeTable[opcode] = [this]() { LD_BC_n16(); };
        }
        // Load A into memory pointed by BC
        else if (mnemonic == "LD_BC_A") {
            opcodeTable[opcode] = [this]() { LD_BC_A(); };
        }
        // Increment BC
        else if (mnemonic == "INC_BC") {
            opcodeTable[opcode] = [this]() { INC_BC(); };
        }
        // Increment B
        else if (mnemonic == "INC_B") {
            opcodeTable[opcode] = [this]() { INC_B(); };
        }
        // Decrement B
        else if (mnemonic == "DEC_B") {
            opcodeTable[opcode] = [this]() { DEC_B(); };
        }
        // Load immediate 8-bit value into B
        else if (mnemonic == "LD_B_n8") {
            opcodeTable[opcode] = [this]() { LD_B_n8(); };
        }
        // ... more mappings will be added
        else {
            std::cerr << "Unknown mnemonic: " << mnemonic << std::endl;
        }
    }
    // CB-prefixed opcodes
    else {
        // RLCA
        if (mnemonic == "RLCA") {
            opcodeTable[opcode] = [this]() { RLCA(); };
        }
        // Load 16-bit immediate value into SP
        else if (mnemonic == "LD_a16_SP") {
            opcodeTable[opcode] = [this]() { LD_a16_SP(); };
        }
        // Add HL and BC
        else if (mnemonic == "ADD_HL_BC") {
            opcodeTable[opcode] = [this]() { ADD_HL_BC(); };
        }
        // Load A into memory pointed by BC
        else if (mnemonic == "LD_A_BC") {
            opcodeTable[opcode] = [this]() { LD_A_BC(); };
        }
        // ... more mappings will be added
        else {
            std::cerr << "Unknown CB-prefixed mnemonic: " << mnemonic << std::endl;
        }
    }
}

// Initialize opcodes
void CPU::initializeOpcodes() {
    // Initialize opcode tables with NOP
    for (u16 i = 0; i < 256; i++) {
        m_opcodeTable[i] = [this]() { NOP(); };
        m_cbOpcodeTable[i] = [this]() { NOP(); };
    }
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

// Load immediate 16-bit value into BC
void CPU::LD_BC_n16() {
    m_registers.bc = readPC16();
    m_cycles += 12;
}

// Load A into memory pointed by BC
void CPU::LD_BC_A() {
    m_memory.write(m_registers.bc, m_registers.a);
    m_cycles += 8;
}

// Increment BC
void CPU::INC_BC() {
    m_registers.bc++;
    m_cycles += 8;
}

// Increment B
void CPU::INC_B() {
    u8 result = m_registers.b + 1;
    
    // Set flags
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.b & 0x0F) == 0x0F);
    
    m_registers.b = result;
    m_cycles += 4;
}

// Decrement B
void CPU::DEC_B() {
    u8 result = m_registers.b - 1;
    
    // Set flags
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.b & 0x0F) == 0x00);
    
    m_registers.b = result;
    m_cycles += 4;
}

// Load immediate 8-bit value into B
void CPU::LD_B_n8() {
    m_registers.b = readPC();
    m_cycles += 8;
}

// Rotate A left through carry
void CPU::RLCA() {
    bool carry = (m_registers.a & 0x80) != 0;
    m_registers.a = (m_registers.a << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 4;
}

// Load 16-bit immediate value into SP
void CPU::LD_a16_SP() {
    u16 address = readPC16();
    m_memory.write(address, m_registers.sp & 0xFF);
    m_memory.write(address + 1, m_registers.sp >> 8);
    m_cycles += 20;
}

// Add HL and BC
void CPU::ADD_HL_BC() {
    u32 result = m_registers.hl + m_registers.bc;
    
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.bc & 0x0FFF) > 0x0FFF);
    setFlag(FLAG_C, result > 0xFFFF);
    
    m_registers.hl = result & 0xFFFF;
    m_cycles += 8;
}

// Load A into memory pointed by BC
void CPU::LD_A_BC() {
    m_registers.a = m_memory.read(m_registers.bc);
    m_cycles += 8;
}

void CPU::DEC_BC() {
    m_registers.bc--;
    m_cycles += 8;
}

void CPU::INC_C() {
    u8 result = m_registers.c + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.c & 0x0F) == 0x0F);
    
    m_registers.c = result;
    m_cycles += 4;
}

void CPU::DEC_C() {
    u8 result = m_registers.c - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.c & 0x0F) == 0x00);
    
    m_registers.c = result;
    m_cycles += 4;
}

void CPU::LD_C_n8() {
    m_registers.c = readPC();
    m_cycles += 8;
}

void CPU::RRCA() {
    bool carry = (m_registers.a & 0x01) != 0;
    m_registers.a = (m_registers.a >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 4;
}

void CPU::STOP() {
    m_stopped = true;
    readPC(); // Skip the next byte
    m_cycles += 4;
}

void CPU::LD_DE_n16() {
    m_registers.de = readPC16();
    m_cycles += 12;
}

void CPU::LD_DE_A() {
    m_memory.write(m_registers.de, m_registers.a);
    m_cycles += 8;
}

void CPU::INC_DE() {
    m_registers.de++;
    m_cycles += 8;
}

void CPU::INC_D() {
    u8 result = m_registers.d + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.d & 0x0F) == 0x0F);
    
    m_registers.d = result;
    m_cycles += 4;
}

void CPU::DEC_D() {
    u8 result = m_registers.d - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.d & 0x0F) == 0x00);
    
    m_registers.d = result;
    m_cycles += 4;
}

void CPU::LD_D_n8() {
    m_registers.d = readPC();
    m_cycles += 8;
}

void CPU::RLA() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.a & 0x80) != 0;
    
    m_registers.a = (m_registers.a << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 4;
}

void CPU::JR_e8() {
    s8 offset = static_cast<s8>(readPC());
    m_registers.pc += offset;
    m_cycles += 12;
}

void CPU::ADD_HL_DE() {
    u32 result = m_registers.hl + m_registers.de;
    
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.de & 0x0FFF) > 0x0FFF);
    setFlag(FLAG_C, result > 0xFFFF);
    
    m_registers.hl = result & 0xFFFF;
    m_cycles += 8;
}

void CPU::LD_A_DE() {
    m_registers.a = m_memory.read(m_registers.de);
    m_cycles += 8;
}

void CPU::DEC_DE() {
    m_registers.de--;
    m_cycles += 8;
}

void CPU::INC_E() {
    u8 result = m_registers.e + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.e & 0x0F) == 0x0F);
    
    m_registers.e = result;
    m_cycles += 4;
}

void CPU::DEC_E() {
    u8 result = m_registers.e - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.e & 0x0F) == 0x00);
    
    m_registers.e = result;
    m_cycles += 4;
}

void CPU::LD_E_n8() {
    m_registers.e = readPC();
    m_cycles += 8;
}

void CPU::RRA() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.a & 0x01) != 0;
    
    m_registers.a = (m_registers.a >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 4;
}

void CPU::JR_NZ_e8() {
    s8 offset = static_cast<s8>(readPC());
    
    if (!getFlag(FLAG_Z)) {
        m_registers.pc += offset;
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

void CPU::LD_HL_n16() {
    m_registers.hl = readPC16();
    m_cycles += 12;
}

void CPU::LD_HLI_A() {
    m_memory.write(m_registers.hl, m_registers.a);
    m_registers.hl++;
    m_cycles += 8;
}

void CPU::INC_HL() {
    m_registers.hl++;
    m_cycles += 8;
}