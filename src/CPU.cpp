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
        // LD r,r and LD r,n
        else if (mnemonic == "LD B,B") {
            opcodeTable[opcode] = [this]() { LD_B_B(); };
        }
        else if (mnemonic == "LD B,C") {
            opcodeTable[opcode] = [this]() { LD_B_C(); };
        }
        else if (mnemonic == "LD B,D") {
            opcodeTable[opcode] = [this]() { LD_B_D(); };
        }
        else if (mnemonic == "LD B,E") {
            opcodeTable[opcode] = [this]() { LD_B_E(); };
        }
        else if (mnemonic == "LD B,H") {
            opcodeTable[opcode] = [this]() { LD_B_H(); };
        }
        else if (mnemonic == "LD B,L") {
            opcodeTable[opcode] = [this]() { LD_B_L(); };
        }
        else if (mnemonic == "LD B,(HL)") {
            opcodeTable[opcode] = [this]() { LD_B_HL(); };
        }
        else if (mnemonic == "LD B,A") {
            opcodeTable[opcode] = [this]() { LD_B_A(); };
        }
        else if (mnemonic == "LD C,B") {
            opcodeTable[opcode] = [this]() { LD_C_B(); };
        }
        else if (mnemonic == "LD C,C") {
            opcodeTable[opcode] = [this]() { LD_C_C(); };
        }
        else if (mnemonic == "LD C,D") {
            opcodeTable[opcode] = [this]() { LD_C_D(); };
        }
        else if (mnemonic == "LD C,E") {
            opcodeTable[opcode] = [this]() { LD_C_E(); };
        }
        else if (mnemonic == "LD C,H") {
            opcodeTable[opcode] = [this]() { LD_C_H(); };
        }
        else if (mnemonic == "LD C,L") {
            opcodeTable[opcode] = [this]() { LD_C_L(); };
        }
        else if (mnemonic == "LD C,(HL)") {
            opcodeTable[opcode] = [this]() { LD_C_HL(); };
        }
        else if (mnemonic == "LD C,A") {
            opcodeTable[opcode] = [this]() { LD_C_A(); };
        }
        else if (mnemonic == "LD D,B") {
            opcodeTable[opcode] = [this]() { LD_D_B(); };
        }
        else if (mnemonic == "LD D,C") {
            opcodeTable[opcode] = [this]() { LD_D_C(); };
        }
        else if (mnemonic == "LD D,D") {
            opcodeTable[opcode] = [this]() { LD_D_D(); };
        }
        else if (mnemonic == "LD D,E") {
            opcodeTable[opcode] = [this]() { LD_D_E(); };
        }
        else if (mnemonic == "LD D,H") {
            opcodeTable[opcode] = [this]() { LD_D_H(); };
        }
        else if (mnemonic == "LD D,L") {
            opcodeTable[opcode] = [this]() { LD_D_L(); };
        }
        else if (mnemonic == "LD D,(HL)") {
            opcodeTable[opcode] = [this]() { LD_D_HL(); };
        }
        else if (mnemonic == "LD D,A") {
            opcodeTable[opcode] = [this]() { LD_D_A(); };
        }
        else if (mnemonic == "LD E,B") {
            opcodeTable[opcode] = [this]() { LD_E_B(); };
        }
        else if (mnemonic == "LD E,C") {
            opcodeTable[opcode] = [this]() { LD_E_C(); };
        }
        else if (mnemonic == "LD E,D") {
            opcodeTable[opcode] = [this]() { LD_E_D(); };
        }
        else if (mnemonic == "LD E,E") {
            opcodeTable[opcode] = [this]() { LD_E_E(); };
        }
        else if (mnemonic == "LD E,H") {
            opcodeTable[opcode] = [this]() { LD_E_H(); };
        }
        else if (mnemonic == "LD E,L") {
            opcodeTable[opcode] = [this]() { LD_E_L(); };
        }
        else if (mnemonic == "LD E,(HL)") {
            opcodeTable[opcode] = [this]() { LD_E_HL(); };
        }
        else if (mnemonic == "LD E,A") {
            opcodeTable[opcode] = [this]() { LD_E_A(); };
        }
        else if (mnemonic == "LD H,B") {
            opcodeTable[opcode] = [this]() { LD_H_B(); };
        }
        else if (mnemonic == "LD H,C") {
            opcodeTable[opcode] = [this]() { LD_H_C(); };
        }
        else if (mnemonic == "LD H,D") {
            opcodeTable[opcode] = [this]() { LD_H_D(); };
        }
        else if (mnemonic == "LD H,E") {
            opcodeTable[opcode] = [this]() { LD_H_E(); };
        }
        else if (mnemonic == "LD H,H") {
            opcodeTable[opcode] = [this]() { LD_H_H(); };
        }
        else if (mnemonic == "LD H,L") {
            opcodeTable[opcode] = [this]() { LD_H_L(); };
        }
        else if (mnemonic == "LD H,(HL)") {
            opcodeTable[opcode] = [this]() { LD_H_HL(); };
        }
        else if (mnemonic == "LD H,A") {
            opcodeTable[opcode] = [this]() { LD_H_A(); };
        }
        else if (mnemonic == "LD L,B") {
            opcodeTable[opcode] = [this]() { LD_L_B(); };
        }
        else if (mnemonic == "LD L,C") {
            opcodeTable[opcode] = [this]() { LD_L_C(); };
        }
        else if (mnemonic == "LD L,D") {
            opcodeTable[opcode] = [this]() { LD_L_D(); };
        }
        else if (mnemonic == "LD L,E") {
            opcodeTable[opcode] = [this]() { LD_L_E(); };
        }
        else if (mnemonic == "LD L,H") {
            opcodeTable[opcode] = [this]() { LD_L_H(); };
        }
        else if (mnemonic == "LD L,L") {
            opcodeTable[opcode] = [this]() { LD_L_L(); };
        }
        else if (mnemonic == "LD L,(HL)") {
            opcodeTable[opcode] = [this]() { LD_L_HL(); };
        }
        else if (mnemonic == "LD L,A") {
            opcodeTable[opcode] = [this]() { LD_L_A(); };
        }
        else if (mnemonic == "LD (HL),B") {
            opcodeTable[opcode] = [this]() { LD_HL_B(); };
        }
        else if (mnemonic == "LD (HL),C") {
            opcodeTable[opcode] = [this]() { LD_HL_C(); };
        }
        else if (mnemonic == "LD (HL),D") {
            opcodeTable[opcode] = [this]() { LD_HL_D(); };
        }
        else if (mnemonic == "LD (HL),E") {
            opcodeTable[opcode] = [this]() { LD_HL_E(); };
        }
        else if (mnemonic == "LD (HL),H") {
            opcodeTable[opcode] = [this]() { LD_HL_H(); };
        }
        else if (mnemonic == "LD (HL),L") {
            opcodeTable[opcode] = [this]() { LD_HL_L(); };
        }
        else if (mnemonic == "LD (HL),A") {
            opcodeTable[opcode] = [this]() { LD_HL_A(); };
        }
        else if (mnemonic == "LD A,B") {
            opcodeTable[opcode] = [this]() { LD_A_B(); };
        }
        else if (mnemonic == "LD A,C") {
            opcodeTable[opcode] = [this]() { LD_A_C(); };
        }
        else if (mnemonic == "LD A,D") {
            opcodeTable[opcode] = [this]() { LD_A_D(); };
        }
        else if (mnemonic == "LD A,E") {
            opcodeTable[opcode] = [this]() { LD_A_E(); };
        }
        else if (mnemonic == "LD A,H") {
            opcodeTable[opcode] = [this]() { LD_A_H(); };
        }
        else if (mnemonic == "LD A,L") {
            opcodeTable[opcode] = [this]() { LD_A_L(); };
        }
        else if (mnemonic == "LD A,(HL)") {
            opcodeTable[opcode] = [this]() { LD_A_HL(); };
        }
        else if (mnemonic == "LD A,A") {
            opcodeTable[opcode] = [this]() { LD_A_A(); };
        }
        else if (mnemonic == "LD A,n8") {
            opcodeTable[opcode] = [this]() { LD_A_d8(); };
        }
        else if (mnemonic == "ADD A,A") {
            opcodeTable[opcode] = [this]() { ADD_A_A(); };
        }
        else if (mnemonic == "ADD A,B") {
            opcodeTable[opcode] = [this]() { ADD_A_B(); };
        }
        else if (mnemonic == "ADD A,C") {
            opcodeTable[opcode] = [this]() { ADD_A_C(); };
        }
        else if (mnemonic == "ADD A,D") {
            opcodeTable[opcode] = [this]() { ADD_A_D(); };
        }
        else if (mnemonic == "ADD A,E") {
            opcodeTable[opcode] = [this]() { ADD_A_E(); };
        }
        else if (mnemonic == "ADD A,H") {
            opcodeTable[opcode] = [this]() { ADD_A_H(); };
        }
        else if (mnemonic == "ADD A,L") {
            opcodeTable[opcode] = [this]() { ADD_A_L(); };
        }
        else if (mnemonic == "ADD A,(HL)") {
            opcodeTable[opcode] = [this]() { ADD_A_HL(); };
        }
        else if (mnemonic == "ADD A,n8") {
            opcodeTable[opcode] = [this]() { ADD_A_n(); };
        }
        else if (mnemonic == "ADC A,A") {
            opcodeTable[opcode] = [this]() { ADC_A_A(); };
        }
        else if (mnemonic == "ADC A,B") {
            opcodeTable[opcode] = [this]() { ADC_A_B(); };
        }
        else if (mnemonic == "ADC A,C") {
            opcodeTable[opcode] = [this]() { ADC_A_C(); };
        }
        else if (mnemonic == "ADC A,D") {
            opcodeTable[opcode] = [this]() { ADC_A_D(); };
        }
        else if (mnemonic == "ADC A,E") {
            opcodeTable[opcode] = [this]() { ADC_A_E(); };
        }
        else if (mnemonic == "ADC A,H") {
            opcodeTable[opcode] = [this]() { ADC_A_H(); };
        }
        else if (mnemonic == "ADC A,L") {
            opcodeTable[opcode] = [this]() { ADC_A_L(); };
        }
        else if (mnemonic == "ADC A,(HL)") {
            opcodeTable[opcode] = [this]() { ADC_A_HL(); };
        }
        else if (mnemonic == "ADC A,n8") {
            opcodeTable[opcode] = [this]() { ADC_A_n(); };
        }
        else if (mnemonic == "SUB A") {
            opcodeTable[opcode] = [this]() { SUB_A(); };
        }
        else if (mnemonic == "SUB B") {
            opcodeTable[opcode] = [this]() { SUB_B(); };
        }
        else if (mnemonic == "SUB C") {
            opcodeTable[opcode] = [this]() { SUB_C(); };
        }
        else if (mnemonic == "SUB D") {
            opcodeTable[opcode] = [this]() { SUB_D(); };
        }
        else if (mnemonic == "SUB E") {
            opcodeTable[opcode] = [this]() { SUB_E(); };
        }
        else if (mnemonic == "SUB H") {
            opcodeTable[opcode] = [this]() { SUB_H(); };
        }
        else if (mnemonic == "SUB L") {
            opcodeTable[opcode] = [this]() { SUB_L(); };
        }
        else if (mnemonic == "SUB (HL)") {
            opcodeTable[opcode] = [this]() { SUB_HL(); };
        }
        else if (mnemonic == "SUB n8") {
            opcodeTable[opcode] = [this]() { SUB_n(); };
        }
        else if (mnemonic == "SBC A,A") {
            opcodeTable[opcode] = [this]() { SBC_A_A(); };
        }
        else if (mnemonic == "SBC A,B") {
            opcodeTable[opcode] = [this]() { SBC_A_B(); };
        }
        else if (mnemonic == "SBC A,C") {
            opcodeTable[opcode] = [this]() { SBC_A_C(); };
        }
        else if (mnemonic == "SBC A,D") {
            opcodeTable[opcode] = [this]() { SBC_A_D(); };
        }
        else if (mnemonic == "SBC A,E") {
            opcodeTable[opcode] = [this]() { SBC_A_E(); };
        }
        else if (mnemonic == "SBC A,H") {
            opcodeTable[opcode] = [this]() { SBC_A_H(); };
        }
        else if (mnemonic == "SBC A,L") {
            opcodeTable[opcode] = [this]() { SBC_A_L(); };
        }
        else if (mnemonic == "SBC A,(HL)") {
            opcodeTable[opcode] = [this]() { SBC_A_HL(); };
        }
        else if (mnemonic == "SBC A,n8") {
            opcodeTable[opcode] = [this]() { SBC_A_n(); };
        }
        else if (mnemonic == "AND A") {
            opcodeTable[opcode] = [this]() { AND_A(); };
        }
        else if (mnemonic == "AND B") {
            opcodeTable[opcode] = [this]() { AND_B(); };
        }
        else if (mnemonic == "AND C") {
            opcodeTable[opcode] = [this]() { AND_C(); };
        }
        else if (mnemonic == "AND D") {
            opcodeTable[opcode] = [this]() { AND_D(); };
        }
        else if (mnemonic == "AND E") {
            opcodeTable[opcode] = [this]() { AND_E(); };
        }
        else if (mnemonic == "AND H") {
            opcodeTable[opcode] = [this]() { AND_H(); };
        }
        else if (mnemonic == "AND L") {
            opcodeTable[opcode] = [this]() { AND_L(); };
        }
        else if (mnemonic == "AND (HL)") {
            opcodeTable[opcode] = [this]() { AND_HL(); };
        }
        else if (mnemonic == "AND n8") {
            opcodeTable[opcode] = [this]() { AND_n(); };
        }
        else if (mnemonic == "OR A") {
            opcodeTable[opcode] = [this]() { OR_A(); };
        }
        else if (mnemonic == "OR B") {
            opcodeTable[opcode] = [this]() { OR_B(); };
        }
        else if (mnemonic == "OR C") {
            opcodeTable[opcode] = [this]() { OR_C(); };
        }
        else if (mnemonic == "OR D") {
            opcodeTable[opcode] = [this]() { OR_D(); };
        }
        else if (mnemonic == "OR E") {
            opcodeTable[opcode] = [this]() { OR_E(); };
        }
        else if (mnemonic == "OR H") {
            opcodeTable[opcode] = [this]() { OR_H(); };
        }
        else if (mnemonic == "OR L") {
            opcodeTable[opcode] = [this]() { OR_L(); };
        }
        else if (mnemonic == "OR (HL)") {
            opcodeTable[opcode] = [this]() { OR_HL(); };
        }
        else if (mnemonic == "OR n8") {
            opcodeTable[opcode] = [this]() { OR_n(); };
        }
        else if (mnemonic == "XOR A") {
            opcodeTable[opcode] = [this]() { XOR_A(); };
        }
        else if (mnemonic == "XOR B") {
            opcodeTable[opcode] = [this]() { XOR_B(); };
        }
        else if (mnemonic == "XOR C") {
            opcodeTable[opcode] = [this]() { XOR_C(); };
        }
        else if (mnemonic == "XOR D") {
            opcodeTable[opcode] = [this]() { XOR_D(); };
        }
        else if (mnemonic == "XOR E") {
            opcodeTable[opcode] = [this]() { XOR_E(); };
        }
        else if (mnemonic == "XOR H") {
            opcodeTable[opcode] = [this]() { XOR_H(); };
        }
        else if (mnemonic == "XOR L") {
            opcodeTable[opcode] = [this]() { XOR_L(); };
        }
        else if (mnemonic == "XOR (HL)") {
            opcodeTable[opcode] = [this]() { XOR_HL(); };
        }
        else if (mnemonic == "XOR n8") {
            opcodeTable[opcode] = [this]() { XOR_d8(); };
        }
        else if (mnemonic == "CP A") {
            opcodeTable[opcode] = [this]() { CP_A(); };
        }
        else if (mnemonic == "CP B") {
            opcodeTable[opcode] = [this]() { CP_B(); };
        }
        else if (mnemonic == "CP C") {
            opcodeTable[opcode] = [this]() { CP_C(); };
        }
        else if (mnemonic == "CP D") {
            opcodeTable[opcode] = [this]() { CP_D(); };
        }
        else if (mnemonic == "CP E") {
            opcodeTable[opcode] = [this]() { CP_E(); };
        }
        else if (mnemonic == "CP H") {
            opcodeTable[opcode] = [this]() { CP_H(); };
        }
        else if (mnemonic == "CP L") {
            opcodeTable[opcode] = [this]() { CP_L(); };
        }
        else if (mnemonic == "CP (HL)") {
            opcodeTable[opcode] = [this]() { CP_HL(); };
        }
        else if (mnemonic == "CP n8") {
            opcodeTable[opcode] = [this]() { CP_d8(); };
        }
        else if (mnemonic == "INC A") {
            opcodeTable[opcode] = [this]() { INC_A(); };
        }
        else if (mnemonic == "INC B") {
            opcodeTable[opcode] = [this]() { INC_B(); };
        }
        else if (mnemonic == "INC C") {
            opcodeTable[opcode] = [this]() { INC_C(); };
        }
        else if (mnemonic == "INC D") {
            opcodeTable[opcode] = [this]() { INC_D(); };
        }
        else if (mnemonic == "INC E") {
            opcodeTable[opcode] = [this]() { INC_E(); };
        }
        else if (mnemonic == "INC H") {
            opcodeTable[opcode] = [this]() { INC_H(); };
        }
        else if (mnemonic == "INC L") {
            opcodeTable[opcode] = [this]() { INC_L(); };
        }
        else if (mnemonic == "INC (HL)") {
            opcodeTable[opcode] = [this]() { INC_HL(); };
        }
        else if (mnemonic == "DEC A") {
            opcodeTable[opcode] = [this]() { DEC_A(); };
        }
        else if (mnemonic == "DEC B") {
            opcodeTable[opcode] = [this]() { DEC_B(); };
        }
        else if (mnemonic == "DEC C") {
            opcodeTable[opcode] = [this]() { DEC_C(); };
        }
        else if (mnemonic == "DEC D") {
            opcodeTable[opcode] = [this]() { DEC_D(); };
        }
        else if (mnemonic == "DEC E") {
            opcodeTable[opcode] = [this]() { DEC_E(); };
        }
        else if (mnemonic == "DEC H") {
            opcodeTable[opcode] = [this]() { DEC_H(); };
        }
        else if (mnemonic == "DEC L") {
            opcodeTable[opcode] = [this]() { DEC_L(); };
        }
        else if (mnemonic == "DEC (HL)") {
            opcodeTable[opcode] = [this]() { DEC_HL(); };
        }
        else if (mnemonic == "INC BC") {
            opcodeTable[opcode] = [this]() { INC_BC(); };
        }
        else if (mnemonic == "INC DE") {
            opcodeTable[opcode] = [this]() { INC_DE(); };
        }
        else if (mnemonic == "INC HL") {
            opcodeTable[opcode] = [this]() { INC_HL16(); };
        }
        else if (mnemonic == "INC SP") {
            opcodeTable[opcode] = [this]() { INC_SP(); };
        }
        else if (mnemonic == "DEC BC") {
            opcodeTable[opcode] = [this]() { DEC_BC(); };
        }
        else if (mnemonic == "DEC DE") {
            opcodeTable[opcode] = [this]() { DEC_DE(); };
        }
        else if (mnemonic == "DEC HL") {
            opcodeTable[opcode] = [this]() { DEC_HL16(); };
        }
        else if (mnemonic == "DEC SP") {
            opcodeTable[opcode] = [this]() { DEC_SP(); };
        }
        else if (mnemonic == "ADD HL,BC") {
            opcodeTable[opcode] = [this]() { ADD_HL_BC(); };
        }
        else if (mnemonic == "ADD HL,DE") {
            opcodeTable[opcode] = [this]() { ADD_HL_DE(); };
        }
        else if (mnemonic == "ADD HL,HL") {
            opcodeTable[opcode] = [this]() { ADD_HL_HL(); };
        }
        else if (mnemonic == "ADD HL,SP") {
            opcodeTable[opcode] = [this]() { ADD_HL_SP(); };
        }
        else if (mnemonic == "LD BC,n16") {
            opcodeTable[opcode] = [this]() { LD_BC_d16(); };
        }
        else if (mnemonic == "LD DE,n16") {
            opcodeTable[opcode] = [this]() { LD_DE_d16(); };
        }
        else if (mnemonic == "LD HL,n16") {
            opcodeTable[opcode] = [this]() { LD_HL_d16(); };
        }
        else if (mnemonic == "LD SP,n16") {
            opcodeTable[opcode] = [this]() { LD_SP_d16(); };
        }
        else if (mnemonic == "LD (BC),A") {
            opcodeTable[opcode] = [this]() { LD_BC_A(); };
        }
        else if (mnemonic == "LD (DE),A") {
            opcodeTable[opcode] = [this]() { LD_DE_A(); };
        }
        else if (mnemonic == "LD (HL+),A") {
            opcodeTable[opcode] = [this]() { LD_HLp_A(); };
        }
        else if (mnemonic == "LD (HL-),A") {
            opcodeTable[opcode] = [this]() { LD_HL_A(); };
        }
        else if (mnemonic == "LD A,(BC)") {
            opcodeTable[opcode] = [this]() { LD_A_BC(); };
        }
        else if (mnemonic == "LD A,(DE)") {
            opcodeTable[opcode] = [this]() { LD_A_DE(); };
        }
        else if (mnemonic == "LD A,(HL+)") {
            opcodeTable[opcode] = [this]() { LD_A_HLp(); };
        }
        else if (mnemonic == "LD A,(HL-)") {
            opcodeTable[opcode] = [this]() { LD_A_HLm(); };
        }
        else if (mnemonic == "LD (n16),A") {
            opcodeTable[opcode] = [this]() { LD_nn_A(); };
        }
        else if (mnemonic == "LD A,(n16)") {
            opcodeTable[opcode] = [this]() { LD_A_nn(); };
        }
        else if (mnemonic == "LD (C),A") {
            opcodeTable[opcode] = [this]() { LDH_C_A(); };
        }
        else if (mnemonic == "LD A,(C)") {
            opcodeTable[opcode] = [this]() { LDH_A_C(); };
        }
        else if (mnemonic == "JP n16") {
            opcodeTable[opcode] = [this]() { JP_a16(); };
        }
        else if (mnemonic == "JP NZ,n16") {
            opcodeTable[opcode] = [this]() { JP_NZ_a16(); };
        }
        else if (mnemonic == "JP Z,n16") {
            opcodeTable[opcode] = [this]() { JP_Z_a16(); };
        }
        else if (mnemonic == "JP NC,n16") {
            opcodeTable[opcode] = [this]() { JP_NC_a16(); };
        }
        else if (mnemonic == "JP C,n16") {
            opcodeTable[opcode] = [this]() { JP_C_a16(); };
        }
        else if (mnemonic == "JP HL") {
            opcodeTable[opcode] = [this]() { JP_HL(); };
        }
        else if (mnemonic == "JR n8") {
            opcodeTable[opcode] = [this]() { JR_r8(); };
        }
        else if (mnemonic == "JR NZ,n8") {
            opcodeTable[opcode] = [this]() { JR_NZ_r8(); };
        }
        else if (mnemonic == "JR Z,n8") {
            opcodeTable[opcode] = [this]() { JR_Z_r8(); };
        }
        else if (mnemonic == "JR NC,n8") {
            opcodeTable[opcode] = [this]() { JR_NC_r8(); };
        }
        else if (mnemonic == "JR C,n8") {
            opcodeTable[opcode] = [this]() { JR_C_r8(); };
        }
        else if (mnemonic == "CALL n16") {
            opcodeTable[opcode] = [this]() { CALL_a16(); };
        }
        else if (mnemonic == "CALL NZ,n16") {
            opcodeTable[opcode] = [this]() { CALL_NZ_a16(); };
        }
        else if (mnemonic == "CALL Z,n16") {
            opcodeTable[opcode] = [this]() { CALL_Z_a16(); };
        }
        else if (mnemonic == "CALL NC,n16") {
            opcodeTable[opcode] = [this]() { CALL_NC_a16(); };
        }
        else if (mnemonic == "CALL C,n16") {
            opcodeTable[opcode] = [this]() { CALL_C_a16(); };
        }
        else if (mnemonic == "RET") {
            opcodeTable[opcode] = [this]() { RET(); };
        }
        else if (mnemonic == "RET NZ") {
            opcodeTable[opcode] = [this]() { RET_NZ(); };
        }
        else if (mnemonic == "RET Z") {
            opcodeTable[opcode] = [this]() { RET_Z(); };
        }
        else if (mnemonic == "RET NC") {
            opcodeTable[opcode] = [this]() { RET_NC(); };
        }
        else if (mnemonic == "RET C") {
            opcodeTable[opcode] = [this]() { RET_C(); };
        }
        else if (mnemonic == "RETI") {
            opcodeTable[opcode] = [this]() { RETI(); };
        }
        else if (mnemonic == "RST 00H") {
            opcodeTable[opcode] = [this]() { RST_00H(); };
        }
        else if (mnemonic == "RST 08H") {
            opcodeTable[opcode] = [this]() { RST_08H(); };
        }
        else if (mnemonic == "RST 10H") {
            opcodeTable[opcode] = [this]() { RST_10H(); };
        }
        else if (mnemonic == "RST 18H") {
            opcodeTable[opcode] = [this]() { RST_18H(); };
        }
        else if (mnemonic == "RST 20H") {
            opcodeTable[opcode] = [this]() { RST_20H(); };
        }
        else if (mnemonic == "RST 28H") {
            opcodeTable[opcode] = [this]() { RST_28H(); };
        }
        else if (mnemonic == "RST 30H") {
            opcodeTable[opcode] = [this]() { RST_30H(); };
        }
        else if (mnemonic == "RST 38H") {
            opcodeTable[opcode] = [this]() { RST_38H(); };
        }
        else if (mnemonic == "PUSH BC") {
            opcodeTable[opcode] = [this]() { PUSH_BC(); };
        }
        else if (mnemonic == "PUSH DE") {
            opcodeTable[opcode] = [this]() { PUSH_DE(); };
        }
        else if (mnemonic == "PUSH HL") {
            opcodeTable[opcode] = [this]() { PUSH_HL(); };
        }
        else if (mnemonic == "PUSH AF") {
            opcodeTable[opcode] = [this]() { PUSH_AF(); };
        }
        else if (mnemonic == "POP BC") {
            opcodeTable[opcode] = [this]() { POP_BC(); };
        }
        else if (mnemonic == "POP DE") {
            opcodeTable[opcode] = [this]() { POP_DE(); };
        }
        else if (mnemonic == "POP HL") {
            opcodeTable[opcode] = [this]() { POP_HL(); };
        }
        else if (mnemonic == "POP AF") {
            opcodeTable[opcode] = [this]() { POP_AF(); };
        }
        // ... more mappings will be added
        else {
            std::cerr << "Unknown mnemonic: " << mnemonic << std::endl;
        }
    }
    // CB-prefixed opcodes
    else {
        // BIT operations
        if (mnemonic == "BIT 0,A") {
            opcodeTable[opcode] = [this]() { BIT_0_A(); };
        }
        else if (mnemonic == "BIT 0,B") {
            opcodeTable[opcode] = [this]() { BIT_0_B(); };
        }
        else if (mnemonic == "BIT 0,C") {
            opcodeTable[opcode] = [this]() { BIT_0_C(); };
        }
        else if (mnemonic == "BIT 0,D") {
            opcodeTable[opcode] = [this]() { BIT_0_D(); };
        }
        else if (mnemonic == "BIT 0,E") {
            opcodeTable[opcode] = [this]() { BIT_0_E(); };
        }
        else if (mnemonic == "BIT 0,H") {
            opcodeTable[opcode] = [this]() { BIT_0_H(); };
        }
        else if (mnemonic == "BIT 0,L") {
            opcodeTable[opcode] = [this]() { BIT_0_L(); };
        }
        else if (mnemonic == "BIT 0,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_0_HL(); };
        }
        else if (mnemonic == "BIT 1,A") {
            opcodeTable[opcode] = [this]() { BIT_1_A(); };
        }
        else if (mnemonic == "BIT 1,B") {
            opcodeTable[opcode] = [this]() { BIT_1_B(); };
        }
        else if (mnemonic == "BIT 1,C") {
            opcodeTable[opcode] = [this]() { BIT_1_C(); };
        }
        else if (mnemonic == "BIT 1,D") {
            opcodeTable[opcode] = [this]() { BIT_1_D(); };
        }
        else if (mnemonic == "BIT 1,E") {
            opcodeTable[opcode] = [this]() { BIT_1_E(); };
        }
        else if (mnemonic == "BIT 1,H") {
            opcodeTable[opcode] = [this]() { BIT_1_H(); };
        }
        else if (mnemonic == "BIT 1,L") {
            opcodeTable[opcode] = [this]() { BIT_1_L(); };
        }
        else if (mnemonic == "BIT 1,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_1_HL(); };
        }
        else if (mnemonic == "BIT 2,A") {
            opcodeTable[opcode] = [this]() { BIT_2_A(); };
        }
        else if (mnemonic == "BIT 2,B") {
            opcodeTable[opcode] = [this]() { BIT_2_B(); };
        }
        else if (mnemonic == "BIT 2,C") {
            opcodeTable[opcode] = [this]() { BIT_2_C(); };
        }
        else if (mnemonic == "BIT 2,D") {
            opcodeTable[opcode] = [this]() { BIT_2_D(); };
        }
        else if (mnemonic == "BIT 2,E") {
            opcodeTable[opcode] = [this]() { BIT_2_E(); };
        }
        else if (mnemonic == "BIT 2,H") {
            opcodeTable[opcode] = [this]() { BIT_2_H(); };
        }
        else if (mnemonic == "BIT 2,L") {
            opcodeTable[opcode] = [this]() { BIT_2_L(); };
        }
        else if (mnemonic == "BIT 2,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_2_HL(); };
        }
        else if (mnemonic == "BIT 3,A") {
            opcodeTable[opcode] = [this]() { BIT_3_A(); };
        }
        else if (mnemonic == "BIT 3,B") {
            opcodeTable[opcode] = [this]() { BIT_3_B(); };
        }
        else if (mnemonic == "BIT 3,C") {
            opcodeTable[opcode] = [this]() { BIT_3_C(); };
        }
        else if (mnemonic == "BIT 3,D") {
            opcodeTable[opcode] = [this]() { BIT_3_D(); };
        }
        else if (mnemonic == "BIT 3,E") {
            opcodeTable[opcode] = [this]() { BIT_3_E(); };
        }
        else if (mnemonic == "BIT 3,H") {
            opcodeTable[opcode] = [this]() { BIT_3_H(); };
        }
        else if (mnemonic == "BIT 3,L") {
            opcodeTable[opcode] = [this]() { BIT_3_L(); };
        }
        else if (mnemonic == "BIT 3,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_3_HL(); };
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
    
    // Set up opcode functions
    // This is a minimal set of opcodes needed for the boot ROM
    
    // 0x00: NOP
    m_opcodeTable[0x00] = [this]() { NOP(); };
    
    // Add new instruction mappings
    // 0x07: RLCA
    m_opcodeTable[0x07] = [this]() { RLCA(); };
    
    // 0x17: RLA
    m_opcodeTable[0x17] = [this]() { RLA(); };
    
    // 0x0F: RRCA
    m_opcodeTable[0x0F] = [this]() { RRCA(); };
    
    // 0x1F: RRA
    m_opcodeTable[0x1F] = [this]() { RRA(); };
    
    // 0x27: DAA
    m_opcodeTable[0x27] = [this]() { DAA(); };
    
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

// Register-to-register load instructions

// LD B,B
void CPU::LD_B_B() {
    m_registers.b = m_registers.b;
    m_cycles += 4;
}

// LD B,C
void CPU::LD_B_C() {
    m_registers.b = m_registers.c;
    m_cycles += 4;
}

// LD B,D
void CPU::LD_B_D() {
    m_registers.b = m_registers.d;
    m_cycles += 4;
}

// LD B,E
void CPU::LD_B_E() {
    m_registers.b = m_registers.e;
    m_cycles += 4;
}

// LD B,H
void CPU::LD_B_H() {
    m_registers.b = m_registers.h;
    m_cycles += 4;
}

// LD B,L
void CPU::LD_B_L() {
    m_registers.b = m_registers.l;
    m_cycles += 4;
}

// LD B,(HL)
void CPU::LD_B_HL() {
    m_registers.b = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD B,A
void CPU::LD_B_A() {
    m_registers.b = m_registers.a;
    m_cycles += 4;
}

// LD C,B
void CPU::LD_C_B() {
    m_registers.c = m_registers.b;
    m_cycles += 4;
}

// LD C,C
void CPU::LD_C_C() {
    m_registers.c = m_registers.c;
    m_cycles += 4;
}

// LD C,D
void CPU::LD_C_D() {
    m_registers.c = m_registers.d;
    m_cycles += 4;
}

// LD C,E
void CPU::LD_C_E() {
    m_registers.c = m_registers.e;
    m_cycles += 4;
}

// LD C,H
void CPU::LD_C_H() {
    m_registers.c = m_registers.h;
    m_cycles += 4;
}

// LD C,L
void CPU::LD_C_L() {
    m_registers.c = m_registers.l;
    m_cycles += 4;
}

// LD C,(HL)
void CPU::LD_C_HL() {
    m_registers.c = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD C,A
void CPU::LD_C_A() {
    m_registers.c = m_registers.a;
    m_cycles += 4;
}

// LD D,B
void CPU::LD_D_B() {
    m_registers.d = m_registers.b;
    m_cycles += 4;
}

// LD D,C
void CPU::LD_D_C() {
    m_registers.d = m_registers.c;
    m_cycles += 4;
}

// LD D,D
void CPU::LD_D_D() {
    m_registers.d = m_registers.d;
    m_cycles += 4;
}

// LD D,E
void CPU::LD_D_E() {
    m_registers.d = m_registers.e;
    m_cycles += 4;
}

// LD D,H
void CPU::LD_D_H() {
    m_registers.d = m_registers.h;
    m_cycles += 4;
}

// LD D,L
void CPU::LD_D_L() {
    m_registers.d = m_registers.l;
    m_cycles += 4;
}

// LD D,(HL)
void CPU::LD_D_HL() {
    m_registers.d = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD D,A
void CPU::LD_D_A() {
    m_registers.d = m_registers.a;
    m_cycles += 4;
}

// LD E,B
void CPU::LD_E_B() {
    m_registers.e = m_registers.b;
    m_cycles += 4;
}

// LD E,C
void CPU::LD_E_C() {
    m_registers.e = m_registers.c;
    m_cycles += 4;
}

// LD E,D
void CPU::LD_E_D() {
    m_registers.e = m_registers.d;
    m_cycles += 4;
}

// LD E,E
void CPU::LD_E_E() {
    m_registers.e = m_registers.e;
    m_cycles += 4;
}

// LD E,H
void CPU::LD_E_H() {
    m_registers.e = m_registers.h;
    m_cycles += 4;
}

// LD E,L
void CPU::LD_E_L() {
    m_registers.e = m_registers.l;
    m_cycles += 4;
}

// LD E,(HL)
void CPU::LD_E_HL() {
    m_registers.e = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD E,A
void CPU::LD_E_A() {
    m_registers.e = m_registers.a;
    m_cycles += 4;
}

// LD H,B
void CPU::LD_H_B() {
    m_registers.h = m_registers.b;
    m_cycles += 4;
}

// LD H,C
void CPU::LD_H_C() {
    m_registers.h = m_registers.c;
    m_cycles += 4;
}

// LD H,D
void CPU::LD_H_D() {
    m_registers.h = m_registers.d;
    m_cycles += 4;
}

// LD H,E
void CPU::LD_H_E() {
    m_registers.h = m_registers.e;
    m_cycles += 4;
}

// LD H,H
void CPU::LD_H_H() {
    m_registers.h = m_registers.h;
    m_cycles += 4;
}

// LD H,L
void CPU::LD_H_L() {
    m_registers.h = m_registers.l;
    m_cycles += 4;
}

// LD H,(HL)
void CPU::LD_H_HL() {
    m_registers.h = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD H,A
void CPU::LD_H_A() {
    m_registers.h = m_registers.a;
    m_cycles += 4;
}

// LD L,B
void CPU::LD_L_B() {
    m_registers.l = m_registers.b;
    m_cycles += 4;
}

// LD L,C
void CPU::LD_L_C() {
    m_registers.l = m_registers.c;
    m_cycles += 4;
}

// LD L,D
void CPU::LD_L_D() {
    m_registers.l = m_registers.d;
    m_cycles += 4;
}

// LD L,E
void CPU::LD_L_E() {
    m_registers.l = m_registers.e;
    m_cycles += 4;
}

// LD L,H
void CPU::LD_L_H() {
    m_registers.l = m_registers.h;
    m_cycles += 4;
}

// LD L,L
void CPU::LD_L_L() {
    m_registers.l = m_registers.l;
    m_cycles += 4;
}

// LD L,(HL)
void CPU::LD_L_HL() {
    m_registers.l = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD L,A
void CPU::LD_L_A() {
    m_registers.l = m_registers.a;
    m_cycles += 4;
}

// LD (HL),B
void CPU::LD_HL_B() {
    m_memory.write(m_registers.hl, m_registers.b);
    m_cycles += 8;
}

// LD (HL),C
void CPU::LD_HL_C() {
    m_memory.write(m_registers.hl, m_registers.c);
    m_cycles += 8;
}

// LD (HL),D
void CPU::LD_HL_D() {
    m_memory.write(m_registers.hl, m_registers.d);
    m_cycles += 8;
}

// LD (HL),E
void CPU::LD_HL_E() {
    m_memory.write(m_registers.hl, m_registers.e);
    m_cycles += 8;
}

// LD (HL),H
void CPU::LD_HL_H() {
    m_memory.write(m_registers.hl, m_registers.h);
    m_cycles += 8;
}

// LD (HL),L
void CPU::LD_HL_L() {
    m_memory.write(m_registers.hl, m_registers.l);
    m_cycles += 8;
}

// LD A,B
void CPU::LD_A_B() {
    m_registers.a = m_registers.b;
    m_cycles += 4;
}

// LD A,C
void CPU::LD_A_C() {
    m_registers.a = m_registers.c;
    m_cycles += 4;
}

// LD A,D
void CPU::LD_A_D() {
    m_registers.a = m_registers.d;
    m_cycles += 4;
}

// LD A,E
void CPU::LD_A_E() {
    m_registers.a = m_registers.e;
    m_cycles += 4;
}

// LD A,H
void CPU::LD_A_H() {
    m_registers.a = m_registers.h;
    m_cycles += 4;
}

// LD A,L
void CPU::LD_A_L() {
    m_registers.a = m_registers.l;
    m_cycles += 4;
}

// LD A,(HL)
void CPU::LD_A_HL() {
    m_registers.a = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

// LD A,A
void CPU::LD_A_A() {
    m_registers.a = m_registers.a;
    m_cycles += 4;
}

// LD A,(BC)
void CPU::LD_A_BC() {
    m_registers.a = m_memory.read(m_registers.bc);
    m_cycles += 8;
}

// LD A,(DE)
void CPU::LD_A_DE() {
    m_registers.a = m_memory.read(m_registers.de);
    m_cycles += 8;
}

// LD (BC),A
void CPU::LD_BC_A() {
    m_memory.write(m_registers.bc, m_registers.a);
    m_cycles += 8;
}

// LD (DE),A
void CPU::LD_DE_A() {
    m_memory.write(m_registers.de, m_registers.a);
    m_cycles += 8;
}

// LD (HL+),A
void CPU::LD_HLp_A() {
    m_memory.write(m_registers.hl++, m_registers.a);
    m_cycles += 8;
}

// LD (HL-),A
void CPU::LD_HL_A() {
    m_memory.write(m_registers.hl--, m_registers.a);
    m_cycles += 8;
}

// LD A,(BC)
void CPU::LD_A_BC() {
    m_registers.a = m_memory.read(m_registers.bc);
    m_cycles += 8;
}

// LD A,(DE)
void CPU::LD_A_DE() {
    m_registers.a = m_memory.read(m_registers.de);
    m_cycles += 8;
}

// LD A,(HL+)
void CPU::LD_A_HLp() {
    m_registers.a = m_memory.read(m_registers.hl++);
    m_cycles += 8;
}

// LD A,(HL-)
void CPU::LD_A_HLm() {
    m_registers.a = m_memory.read(m_registers.hl--);
    m_cycles += 8;
}

// LD (n16),A
void CPU::LD_nn_A() {
    u16 address = readPC16();
    m_memory.write(address, m_registers.a);
    m_cycles += 16;
}

// LD A,(n16)
void CPU::LD_A_nn() {
    u16 address = readPC16();
    m_registers.a = m_memory.read(address);
    m_cycles += 16;
}

// LD (C),A
void CPU::LDH_C_A() {
    m_memory.write(0xFF00 | m_registers.c, m_registers.a);
    m_cycles += 8;
}

// LD A,(C)
void CPU::LDH_A_C() {
    m_registers.a = m_memory.read(0xFF00 | m_registers.c);
    m_cycles += 8;
}

// JP NZ,n16
void CPU::JP_NZ_a16() {
    if (!getFlag(FLAG_Z)) {
        m_registers.pc = readPC16();
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

// JP Z,n16
void CPU::JP_Z_a16() {
    if (getFlag(FLAG_Z)) {
        m_registers.pc = readPC16();
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

// JP NC,n16
void CPU::JP_NC_a16() {
    if (!getFlag(FLAG_C)) {
        m_registers.pc = readPC16();
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

// JP C,n16
void CPU::JP_C_a16() {
    if (getFlag(FLAG_C)) {
        m_registers.pc = readPC16();
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

// JP HL
void CPU::JP_HL() {
    m_registers.pc = m_registers.hl;
    m_cycles += 8;
}

// JR n8
void CPU::JR_r8() {
    m_registers.pc = m_registers.pc + readPC();
    m_cycles += 12;
}

// JR NZ,n8
void CPU::JR_NZ_r8() {
    if (!getFlag(FLAG_Z)) {
        m_registers.pc = m_registers.pc + readPC();
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

// JR Z,n8
void CPU::JR_Z_r8() {
    if (getFlag(FLAG_Z)) {
        m_registers.pc = m_registers.pc + readPC();
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

// JR NC,n8
void CPU::JR_NC_r8() {
    if (!getFlag(FLAG_C)) {
        m_registers.pc = m_registers.pc + readPC();
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

// JR C,n8
void CPU::JR_C_r8() {
    if (getFlag(FLAG_C)) {
        m_registers.pc = m_registers.pc + readPC();
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

// CALL n16
void CPU::CALL_a16() {
    push(m_registers.pc);
    m_registers.pc = readPC16();
    m_cycles += 24;
}

// CALL NZ,n16
void CPU::CALL_NZ_a16() {
    if (!getFlag(FLAG_Z)) {
        push(m_registers.pc);
        m_registers.pc = readPC16();
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

// CALL Z,n16
void CPU::CALL_Z_a16() {
    if (getFlag(FLAG_Z)) {
        push(m_registers.pc);
        m_registers.pc = readPC16();
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

// CALL NC,n16
void CPU::CALL_NC_a16() {
    if (!getFlag(FLAG_C)) {
        push(m_registers.pc);
        m_registers.pc = readPC16();
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

// CALL C,n16
void CPU::CALL_C_a16() {
    if (getFlag(FLAG_C)) {
        push(m_registers.pc);
        m_registers.pc = readPC16();
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

// RET
void CPU::RET() {
    m_registers.pc = pop();
    m_cycles += 16;
}

// RET NZ
void CPU::RET_NZ() {
    if (!getFlag(FLAG_Z)) {
        m_registers.pc = pop();
        m_cycles += 16;
    } else {
        m_cycles += 8;
    }
}

// RET Z
void CPU::RET_Z() {
    if (getFlag(FLAG_Z)) {
        m_registers.pc = pop();
        m_cycles += 16;
    } else {
        m_cycles += 8;
    }
}

// RET NC
void CPU::RET_NC() {
    if (!getFlag(FLAG_C)) {
        m_registers.pc = pop();
        m_cycles += 16;
    } else {
        m_cycles += 8;
    }
}

// RET C
void CPU::RET_C() {
    if (getFlag(FLAG_C)) {
        m_registers.pc = pop();
        m_cycles += 16;
    } else {
        m_cycles += 8;
    }
}

// RETI
void CPU::RETI() {
    m_registers.pc = pop();
    m_interruptsEnabled = true;
    m_cycles += 16;
}

// RST 00H
void CPU::RST_00H() {
    push(m_registers.pc);
    m_registers.pc = 0x0000;
    m_cycles += 16;
}

// RST 08H
void CPU::RST_08H() {
    push(m_registers.pc);
    m_registers.pc = 0x0008;
    m_cycles += 16;
}

// RST 10H
void CPU::RST_10H() {
    push(m_registers.pc);
    m_registers.pc = 0x0010;
    m_cycles += 16;
}

// RST 18H
void CPU::RST_18H() {
    push(m_registers.pc);
    m_registers.pc = 0x0018;
    m_cycles += 16;
}

// RST 20H
void CPU::RST_20H() {
    push(m_registers.pc);
    m_registers.pc = 0x0020;
    m_cycles += 16;
}

// RST 28H
void CPU::RST_28H() {
    push(m_registers.pc);
    m_registers.pc = 0x0028;
    m_cycles += 16;
}

// RST 30H
void CPU::RST_30H() {
    push(m_registers.pc);
    m_registers.pc = 0x0030;
    m_cycles += 16;
}

// RST 38H
void CPU::RST_38H() {
    push(m_registers.pc);
    m_registers.pc = 0x0038;
    m_cycles += 16;
}

// PUSH BC
void CPU::PUSH_BC() {
    push(m_registers.bc);
    m_cycles += 16;
}

// PUSH DE
void CPU::PUSH_DE() {
    push(m_registers.de);
    m_cycles += 16;
}

// PUSH HL
void CPU::PUSH_HL() {
    push(m_registers.hl);
    m_cycles += 16;
}

// PUSH AF
void CPU::PUSH_AF() {
    push(m_registers.af);
    m_cycles += 16;
}

// POP BC
void CPU::POP_BC() {
    m_registers.bc = pop();
    m_cycles += 12;
}

// POP DE
void CPU::POP_DE() {
    m_registers.de = pop();
    m_cycles += 12;
}

// POP HL
void CPU::POP_HL() {
    m_registers.hl = pop();
    m_cycles += 12;
}

// POP AF
void CPU::POP_AF() {
    m_registers.af = pop();
    m_cycles += 12;
}

// ADD A,A
void CPU::ADD_A_A() {
    u8 value = m_registers.a;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,B
void CPU::ADD_A_B() {
    u8 value = m_registers.b;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,C
void CPU::ADD_A_C() {
    u8 value = m_registers.c;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,D
void CPU::ADD_A_D() {
    u8 value = m_registers.d;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,E
void CPU::ADD_A_E() {
    u8 value = m_registers.e;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,H
void CPU::ADD_A_H() {
    u8 value = m_registers.h;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,L
void CPU::ADD_A_L() {
    u8 value = m_registers.l;
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADD A,(HL)
void CPU::ADD_A_HL() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// ADD A,n8
void CPU::ADD_A_n() {
    u8 value = readPC();
    u16 result = m_registers.a + value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// ADC A,A
void CPU::ADC_A_A() {
    u8 value = m_registers.a;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,B
void CPU::ADC_A_B() {
    u8 value = m_registers.b;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,C
void CPU::ADC_A_C() {
    u8 value = m_registers.c;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,D
void CPU::ADC_A_D() {
    u8 value = m_registers.d;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,E
void CPU::ADC_A_E() {
    u8 value = m_registers.e;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,H
void CPU::ADC_A_H() {
    u8 value = m_registers.h;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,L
void CPU::ADC_A_L() {
    u8 value = m_registers.l;
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// ADC A,(HL)
void CPU::ADC_A_HL() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// ADC A,n8
void CPU::ADC_A_n() {
    u8 value = readPC();
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, ((m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0)) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// SUB A
void CPU::SUB_A() {
    u8 value = m_registers.a;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB B
void CPU::SUB_B() {
    u8 value = m_registers.b;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB C
void CPU::SUB_C() {
    u8 value = m_registers.c;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB D
void CPU::SUB_D() {
    u8 value = m_registers.d;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB E
void CPU::SUB_E() {
    u8 value = m_registers.e;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB H
void CPU::SUB_H() {
    u8 value = m_registers.h;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB L
void CPU::SUB_L() {
    u8 value = m_registers.l;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SUB (HL)
void CPU::SUB_HL() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// SUB n8
void CPU::SUB_n() {
    u8 value = readPC();
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// SBC A,A
void CPU::SBC_A_A() {
    u8 value = m_registers.a;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,B
void CPU::SBC_A_B() {
    u8 value = m_registers.b;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,C
void CPU::SBC_A_C() {
    u8 value = m_registers.c;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,D
void CPU::SBC_A_D() {
    u8 value = m_registers.d;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,E
void CPU::SBC_A_E() {
    u8 value = m_registers.e;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,H
void CPU::SBC_A_H() {
    u8 value = m_registers.h;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,L
void CPU::SBC_A_L() {
    u8 value = m_registers.l;
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

// SBC A,(HL)
void CPU::SBC_A_HL() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// SBC A,n8
void CPU::SBC_A_n() {
    u8 value = readPC();
    u16 result = m_registers.a - value - (getFlag(FLAG_C) ? 1 : 0);
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0));
    setFlag(FLAG_C, result < 0);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

// AND A
void CPU::AND_A() {
    m_registers.a &= m_registers.a;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND B
void CPU::AND_B() {
    m_registers.a &= m_registers.b;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND C
void CPU::AND_C() {
    m_registers.a &= m_registers.c;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND D
void CPU::AND_D() {
    m_registers.a &= m_registers.d;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND E
void CPU::AND_E() {
    m_registers.a &= m_registers.e;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND H
void CPU::AND_H() {
    m_registers.a &= m_registers.h;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND L
void CPU::AND_L() {
    m_registers.a &= m_registers.l;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// AND (HL)
void CPU::AND_HL() {
    m_registers.a &= m_memory.read(m_registers.hl);
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

// AND n8
void CPU::AND_n() {
    u8 value = readPC();
    m_registers.a &= value;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

// OR A
void CPU::OR_A() {
    m_registers.a |= m_registers.a;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR B
void CPU::OR_B() {
    m_registers.a |= m_registers.b;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR C
void CPU::OR_C() {
    m_registers.a |= m_registers.c;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR D
void CPU::OR_D() {
    m_registers.a |= m_registers.d;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR E
void CPU::OR_E() {
    m_registers.a |= m_registers.e;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR H
void CPU::OR_H() {
    m_registers.a |= m_registers.h;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR L
void CPU::OR_L() {
    m_registers.a |= m_registers.l;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// OR (HL)
void CPU::OR_HL() {
    m_registers.a |= m_memory.read(m_registers.hl);
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

// OR n8
void CPU::OR_n() {
    u8 value = readPC();
    m_registers.a |= value;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
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

// XOR B
void CPU::XOR_B() {
    m_registers.a ^= m_registers.b;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR C
void CPU::XOR_C() {
    m_registers.a ^= m_registers.c;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR D
void CPU::XOR_D() {
    m_registers.a ^= m_registers.d;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR E
void CPU::XOR_E() {
    m_registers.a ^= m_registers.e;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR H
void CPU::XOR_H() {
    m_registers.a ^= m_registers.h;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR L
void CPU::XOR_L() {
    m_registers.a ^= m_registers.l;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

// XOR (HL)
void CPU::XOR_HL() {
    m_registers.a ^= m_memory.read(m_registers.hl);
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

// XOR n8
void CPU::XOR_d8() {
    u8 value = readPC();
    m_registers.a ^= value;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

// CP A
void CPU::CP_A() {
    u8 value = m_registers.a;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP B
void CPU::CP_B() {
    u8 value = m_registers.b;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP C
void CPU::CP_C() {
    u8 value = m_registers.c;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP D
void CPU::CP_D() {
    u8 value = m_registers.d;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP E
void CPU::CP_E() {
    u8 value = m_registers.e;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP H
void CPU::CP_H() {
    u8 value = m_registers.h;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP L
void CPU::CP_L() {
    u8 value = m_registers.l;
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 4;
}

// CP (HL)
void CPU::CP_HL() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 8;
}

// CP n8
void CPU::CP_d8() {
    u8 value = readPC();
    u16 result = m_registers.a - value;
    
    // Set flags
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, result < 0);
    
    m_cycles += 8;
}

// INC A
void CPU::INC_A() {
    u8 value = m_registers.a + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + 1 > 0x0F);
    
    m_registers.a = value;
    m_cycles += 4;
}

// INC B
void CPU::INC_B() {
    u8 value = m_registers.b + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.b & 0x0F) + 1 > 0x0F);
    
    m_registers.b = value;
    m_cycles += 4;
}

// INC C
void CPU::INC_C() {
    u8 value = m_registers.c + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.c & 0x0F) + 1 > 0x0F);
    
    m_registers.c = value;
    m_cycles += 4;
}

// INC D
void CPU::INC_D() {
    u8 value = m_registers.d + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.d & 0x0F) + 1 > 0x0F);
    
    m_registers.d = value;
    m_cycles += 4;
}

// INC E
void CPU::INC_E() {
    u8 value = m_registers.e + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.e & 0x0F) + 1 > 0x0F);
    
    m_registers.e = value;
    m_cycles += 4;
}

// INC H
void CPU::INC_H() {
    u8 value = m_registers.h + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.h & 0x0F) + 1 > 0x0F);
    
    m_registers.h = value;
    m_cycles += 4;
}

// INC L
void CPU::INC_L() {
    u8 value = m_registers.l + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.l & 0x0F) + 1 > 0x0F);
    
    m_registers.l = value;
    m_cycles += 4;
}

// INC (HL)
void CPU::INC_HL() {
    u8 value = m_memory.read(m_registers.hl) + 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_memory.read(m_registers.hl) & 0x0F) + 1 > 0x0F);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 8;
}

// DEC A
void CPU::DEC_A() {
    u8 value = m_registers.a - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) == 0x0F);
    
    m_registers.a = value;
    m_cycles += 4;
}

// DEC B
void CPU::DEC_B() {
    u8 value = m_registers.b - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.b & 0x0F) == 0x0F);
    
    m_registers.b = value;
    m_cycles += 4;
}

// DEC C
void CPU::DEC_C() {
    u8 value = m_registers.c - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.c & 0x0F) == 0x0F);
    
    m_registers.c = value;
    m_cycles += 4;
}

// DEC D
void CPU::DEC_D() {
    u8 value = m_registers.d - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.d & 0x0F) == 0x0F);
    
    m_registers.d = value;
    m_cycles += 4;
}

// DEC E
void CPU::DEC_E() {
    u8 value = m_registers.e - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.e & 0x0F) == 0x0F);
    
    m_registers.e = value;
    m_cycles += 4;
}

// DEC H
void CPU::DEC_H() {
    u8 value = m_registers.h - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.h & 0x0F) == 0x0F);
    
    m_registers.h = value;
    m_cycles += 4;
}

// DEC L
void CPU::DEC_L() {
    u8 value = m_registers.l - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.l & 0x0F) == 0x0F);
    
    m_registers.l = value;
    m_cycles += 4;
}

// DEC (HL)
void CPU::DEC_HL() {
    u8 value = m_memory.read(m_registers.hl) - 1;
    
    // Set flags
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_memory.read(m_registers.hl) & 0x0F) == 0x0F);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 8;
}

// INC BC
void CPU::INC_BC() {
    m_registers.bc++;
    m_cycles += 8;
}

// INC DE
void CPU::INC_DE() {
    m_registers.de++;
    m_cycles += 8;
}

// INC HL
void CPU::INC_HL16() {
    m_registers.hl++;
    m_cycles += 8;
}

// INC SP
void CPU::INC_SP() {
    m_registers.sp++;
    m_cycles += 8;
}

// DEC BC
void CPU::DEC_BC() {
    m_registers.bc--;
    m_cycles += 8;
}

// DEC DE
void CPU::DEC_DE() {
    m_registers.de--;
    m_cycles += 8;
}

// DEC HL
void CPU::DEC_HL16() {
    m_registers.hl--;
    m_cycles += 8;
}

// DEC SP
void CPU::DEC_SP() {
    m_registers.sp--;
    m_cycles += 8;
}

// ADD HL,BC
void CPU::ADD_HL_BC() {
    u16 hl = m_registers.hl + m_registers.bc;
    m_registers.hl = hl & 0xFFFF;
    setFlag(FLAG_C, hl > 0xFFFF);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.bc & 0x0FFF) > 0x0FFF);
    m_cycles += 8;
}

// ADD HL,DE
void CPU::ADD_HL_DE() {
    u16 hl = m_registers.hl + m_registers.de;
    m_registers.hl = hl & 0xFFFF;
    setFlag(FLAG_C, hl > 0xFFFF);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.de & 0x0FFF) > 0x0FFF);
    m_cycles += 8;
}

// ADD HL,HL
void CPU::ADD_HL_HL() {
    u16 hl = m_registers.hl + m_registers.hl;
    m_registers.hl = hl & 0xFFFF;
    setFlag(FLAG_C, hl > 0xFFFF);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.hl & 0x0FFF) > 0x0FFF);
    m_cycles += 8;
}

// ADD HL,SP
void CPU::ADD_HL_SP() {
    u16 hl = m_registers.hl + m_registers.sp;
    m_registers.hl = hl & 0xFFFF;
    setFlag(FLAG_C, hl > 0xFFFF);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.sp & 0x0FFF) > 0x0FFF);
    m_cycles += 8;
}

// RLCA
void CPU::RLCA() {
    bool bit7 = bit_test(m_registers.a, 7);
    m_registers.a = (m_registers.a << 1) | bit7;
    
    // Set flags
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, bit7);
    
    m_cycles += 4;
}

// RLA
void CPU::RLA() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = bit_test(m_registers.a, 7);
    m_registers.a = (m_registers.a << 1) | oldCarry;
    
    // Set flags
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 4;
}

// RRCA
void CPU::RRCA() {
    bool bit0 = bit_test(m_registers.a, 0);
    m_registers.a = (m_registers.a >> 1) | (bit0 << 7);
    
    // Set flags
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, bit0);
    
    m_cycles += 4;
}

// RRA
void CPU::RRA() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = bit_test(m_registers.a, 0);
    m_registers.a = (m_registers.a >> 1) | (oldCarry << 7);
    
    // Set flags
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 4;
}

// DAA
void CPU::DAA() {
    u8 correction = 0;
    bool carry = false;
    
    if (getFlag(FLAG_H) || (!getFlag(FLAG_N) && (m_registers.a & 0xF) > 9)) {
        correction |= 0x06;
    }
    
    if (getFlag(FLAG_C) || (!getFlag(FLAG_N) && m_registers.a > 0x99)) {
        correction |= 0x60;
        carry = true;
    }
    
    m_registers.a += getFlag(FLAG_N) ? -correction : correction;
    
    // Set flags
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 4;
}
