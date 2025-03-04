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
        // LD instructions
        else if (mnemonic == "LD BC,n16") {
            opcodeTable[opcode] = [this]() { LD_BC_n16(); };
        }
        else if (mnemonic == "LD (BC),A") {
            opcodeTable[opcode] = [this]() { LD_BC_A(); };
        }
        else if (mnemonic == "INC BC") {
            opcodeTable[opcode] = [this]() { INC_BC(); };
        }
        else if (mnemonic == "INC B") {
            opcodeTable[opcode] = [this]() { INC_B(); };
        }
        else if (mnemonic == "DEC B") {
            opcodeTable[opcode] = [this]() { DEC_B(); };
        }
        else if (mnemonic == "LD B,n8") {
            opcodeTable[opcode] = [this]() { LD_B_n8(); };
        }
        else if (mnemonic == "RLCA") {
            opcodeTable[opcode] = [this]() { RLCA(); };
        }
        else if (mnemonic == "LD (a16),SP") {
            opcodeTable[opcode] = [this]() { LD_a16_SP(); };
        }
        else if (mnemonic == "ADD HL,BC") {
            opcodeTable[opcode] = [this]() { ADD_HL_BC(); };
        }
        else if (mnemonic == "LD A,(BC)") {
            opcodeTable[opcode] = [this]() { LD_A_BC(); };
        }
        else if (mnemonic == "DEC BC") {
            opcodeTable[opcode] = [this]() { DEC_BC(); };
        }
        else if (mnemonic == "INC C") {
            opcodeTable[opcode] = [this]() { INC_C(); };
        }
        else if (mnemonic == "DEC C") {
            opcodeTable[opcode] = [this]() { DEC_C(); };
        }
        else if (mnemonic == "LD C,n8") {
            opcodeTable[opcode] = [this]() { LD_C_n8(); };
        }
        else if (mnemonic == "RRCA") {
            opcodeTable[opcode] = [this]() { RRCA(); };
        }
        else if (mnemonic == "STOP") {
            opcodeTable[opcode] = [this]() { STOP(); };
        }
        else if (mnemonic == "LD DE,n16") {
            opcodeTable[opcode] = [this]() { LD_DE_n16(); };
        }
        else if (mnemonic == "LD (DE),A") {
            opcodeTable[opcode] = [this]() { LD_DE_A(); };
        }
        else if (mnemonic == "INC DE") {
            opcodeTable[opcode] = [this]() { INC_DE(); };
        }
        else if (mnemonic == "INC D") {
            opcodeTable[opcode] = [this]() { INC_D(); };
        }
        else if (mnemonic == "DEC D") {
            opcodeTable[opcode] = [this]() { DEC_D(); };
        }
        else if (mnemonic == "LD D,n8") {
            opcodeTable[opcode] = [this]() { LD_D_n8(); };
        }
        else if (mnemonic == "RLA") {
            opcodeTable[opcode] = [this]() { RLA(); };
        }
        else if (mnemonic == "JR e8") {
            opcodeTable[opcode] = [this]() { JR_e8(); };
        }
        else if (mnemonic == "ADD HL,DE") {
            opcodeTable[opcode] = [this]() { ADD_HL_DE(); };
        }
        else if (mnemonic == "LD A,(DE)") {
            opcodeTable[opcode] = [this]() { LD_A_DE(); };
        }
        else if (mnemonic == "DEC DE") {
            opcodeTable[opcode] = [this]() { DEC_DE(); };
        }
        else if (mnemonic == "INC E") {
            opcodeTable[opcode] = [this]() { INC_E(); };
        }
        else if (mnemonic == "DEC E") {
            opcodeTable[opcode] = [this]() { DEC_E(); };
        }
        else if (mnemonic == "LD E,n8") {
            opcodeTable[opcode] = [this]() { LD_E_n8(); };
        }
        else if (mnemonic == "RRA") {
            opcodeTable[opcode] = [this]() { RRA(); };
        }
        else if (mnemonic == "JR NZ,e8") {
            opcodeTable[opcode] = [this]() { JR_NZ_e8(); };
        }
        else if (mnemonic == "LD HL,n16") {
            opcodeTable[opcode] = [this]() { LD_HL_n16(); };
        }
        else if (mnemonic == "LD (HL+),A") {
            opcodeTable[opcode] = [this]() { LD_HLI_A(); };
        }
        else if (mnemonic == "INC HL") {
            opcodeTable[opcode] = [this]() { INC_HL(); };
        }
        else if (mnemonic == "INC H") {
            opcodeTable[opcode] = [this]() { INC_H(); };
        }
        else if (mnemonic == "DEC H") {
            opcodeTable[opcode] = [this]() { DEC_H(); };
        }
        else if (mnemonic == "LD H,n8") {
            opcodeTable[opcode] = [this]() { LD_H_n8(); };
        }
        else if (mnemonic == "DAA") {
            opcodeTable[opcode] = [this]() { DAA(); };
        }
        else if (mnemonic == "JR Z,e8") {
            opcodeTable[opcode] = [this]() { JR_Z_e8(); };
        }
        else if (mnemonic == "ADD HL,HL") {
            opcodeTable[opcode] = [this]() { ADD_HL_HL(); };
        }
        else if (mnemonic == "LD A,(HL+)") {
            opcodeTable[opcode] = [this]() { LD_A_HLI(); };
        }
        else if (mnemonic == "DEC HL") {
            opcodeTable[opcode] = [this]() { DEC_HL(); };
        }
        else if (mnemonic == "INC L") {
            opcodeTable[opcode] = [this]() { INC_L(); };
        }
        else if (mnemonic == "DEC L") {
            opcodeTable[opcode] = [this]() { DEC_L(); };
        }
        else if (mnemonic == "LD L,n8") {
            opcodeTable[opcode] = [this]() { LD_L_n8(); };
        }
        else if (mnemonic == "CPL") {
            opcodeTable[opcode] = [this]() { CPL(); };
        }
        else if (mnemonic == "JR NC,e8") {
            opcodeTable[opcode] = [this]() { JR_NC_e8(); };
        }
        else if (mnemonic == "LD SP,n16") {
            opcodeTable[opcode] = [this]() { LD_SP_n16(); };
        }
        else if (mnemonic == "LD (HL-),A") {
            opcodeTable[opcode] = [this]() { LD_HLD_A(); };
        }
        else if (mnemonic == "INC SP") {
            opcodeTable[opcode] = [this]() { INC_SP(); };
        }
        else if (mnemonic == "INC (HL)") {
            opcodeTable[opcode] = [this]() { INC_HLm(); };
        }
        else if (mnemonic == "DEC (HL)") {
            opcodeTable[opcode] = [this]() { DEC_HLm(); };
        }
        else if (mnemonic == "LD (HL),n8") {
            opcodeTable[opcode] = [this]() { LD_HL_n8(); };
        }
        else if (mnemonic == "SCF") {
            opcodeTable[opcode] = [this]() { SCF(); };
        }
        else if (mnemonic == "JR C,e8") {
            opcodeTable[opcode] = [this]() { JR_C_e8(); };
        }
        else if (mnemonic == "ADD HL,SP") {
            opcodeTable[opcode] = [this]() { ADD_HL_SP(); };
        }
        else if (mnemonic == "LD A,(HL-)") {
            opcodeTable[opcode] = [this]() { LD_A_HLD(); };
        }
        else if (mnemonic == "DEC SP") {
            opcodeTable[opcode] = [this]() { DEC_SP(); };
        }
        else if (mnemonic == "INC A") {
            opcodeTable[opcode] = [this]() { INC_A(); };
        }
        else if (mnemonic == "DEC A") {
            opcodeTable[opcode] = [this]() { DEC_A(); };
        }
        else if (mnemonic == "LD A,n8") {
            opcodeTable[opcode] = [this]() { LD_A_n8(); };
        }
        else if (mnemonic == "CCF") {
            opcodeTable[opcode] = [this]() { CCF(); };
        }
        // Register-to-register loads (0x40-0x7F)
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
            opcodeTable[opcode] = [this]() { LD_B_HLm(); };
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
            opcodeTable[opcode] = [this]() { LD_C_HLm(); };
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
            opcodeTable[opcode] = [this]() { LD_D_HLm(); };
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
            opcodeTable[opcode] = [this]() { LD_E_HLm(); };
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
            opcodeTable[opcode] = [this]() { LD_H_HLm(); };
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
            opcodeTable[opcode] = [this]() { LD_L_HLm(); };
        }
        else if (mnemonic == "LD L,A") {
            opcodeTable[opcode] = [this]() { LD_L_A(); };
        }
        else if (mnemonic == "LD (HL),B") {
            opcodeTable[opcode] = [this]() { LD_HLm_B(); };
        }
        else if (mnemonic == "LD (HL),C") {
            opcodeTable[opcode] = [this]() { LD_HLm_C(); };
        }
        else if (mnemonic == "LD (HL),D") {
            opcodeTable[opcode] = [this]() { LD_HLm_D(); };
        }
        else if (mnemonic == "LD (HL),E") {
            opcodeTable[opcode] = [this]() { LD_HLm_E(); };
        }
        else if (mnemonic == "LD (HL),H") {
            opcodeTable[opcode] = [this]() { LD_HLm_H(); };
        }
        else if (mnemonic == "LD (HL),L") {
            opcodeTable[opcode] = [this]() { LD_HLm_L(); };
        }
        else if (mnemonic == "HALT") {
            opcodeTable[opcode] = [this]() { HALT(); };
        }
        else if (mnemonic == "LD (HL),A") {
            opcodeTable[opcode] = [this]() { LD_HLm_A(); };
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
            opcodeTable[opcode] = [this]() { LD_A_HLm(); };
        }
        else if (mnemonic == "LD A,A") {
            opcodeTable[opcode] = [this]() { LD_A_A(); };
        }
        // Arithmetic operations (0x80-0x8F)
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
            opcodeTable[opcode] = [this]() { ADD_A_HLm(); };
        }
        else if (mnemonic == "ADD A,A") {
            opcodeTable[opcode] = [this]() { ADD_A_A(); };
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
            opcodeTable[opcode] = [this]() { ADC_A_HLm(); };
        }
        else if (mnemonic == "ADC A,A") {
            opcodeTable[opcode] = [this]() { ADC_A_A(); };
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
            opcodeTable[opcode] = [this]() { SUB_HLm(); };
        }
        else if (mnemonic == "SUB A") {
            opcodeTable[opcode] = [this]() { SUB_A(); };
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
            opcodeTable[opcode] = [this]() { SBC_A_HLm(); };
        }
        else if (mnemonic == "SBC A,A") {
            opcodeTable[opcode] = [this]() { SBC_A_A(); };
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
            opcodeTable[opcode] = [this]() { AND_HLm(); };
        }
        else if (mnemonic == "AND A") {
            opcodeTable[opcode] = [this]() { AND_A(); };
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
            opcodeTable[opcode] = [this]() { XOR_HLm(); };
        }
        else if (mnemonic == "XOR A") {
            opcodeTable[opcode] = [this]() { XOR_A(); };
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
            opcodeTable[opcode] = [this]() { OR_HLm(); };
        }
        else if (mnemonic == "OR A") {
            opcodeTable[opcode] = [this]() { OR_A(); };
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
            opcodeTable[opcode] = [this]() { CP_HLm(); };
        }
        else if (mnemonic == "CP A") {
            opcodeTable[opcode] = [this]() { CP_A(); };
        }
        else if (mnemonic == "RET NZ") {
            opcodeTable[opcode] = [this]() { RET_NZ(); };
        }
        else if (mnemonic == "POP BC") {
            opcodeTable[opcode] = [this]() { POP_BC(); };
        }
        else if (mnemonic == "JP NZ,a16") {
            opcodeTable[opcode] = [this]() { JP_NZ_a16(); };
        }
        else if (mnemonic == "JP a16") {
            opcodeTable[opcode] = [this]() { JP_a16(); };
        }
        else if (mnemonic == "CALL NZ,a16") {
            opcodeTable[opcode] = [this]() { CALL_NZ_a16(); };
        }
        else if (mnemonic == "PUSH BC") {
            opcodeTable[opcode] = [this]() { PUSH_BC(); };
        }
        else if (mnemonic == "ADD A,n8") {
            opcodeTable[opcode] = [this]() { ADD_A_n8(); };
        }
        else if (mnemonic == "RST 00H") {
            opcodeTable[opcode] = [this]() { RST_00H(); };
        }
        else if (mnemonic == "RET Z") {
            opcodeTable[opcode] = [this]() { RET_Z(); };
        }
        else if (mnemonic == "RET") {
            opcodeTable[opcode] = [this]() { RET(); };
        }
        else if (mnemonic == "JP Z,a16") {
            opcodeTable[opcode] = [this]() { JP_Z_a16(); };
        }
        else if (mnemonic == "PREFIX CB") {
            opcodeTable[opcode] = [this]() { PREFIX_CB(); };
        }
        else if (mnemonic == "CALL Z,a16") {
            opcodeTable[opcode] = [this]() { CALL_Z_a16(); };
        }
        else if (mnemonic == "CALL a16") {
            opcodeTable[opcode] = [this]() { CALL_a16(); };
        }
        else if (mnemonic == "ADC A,n8") {
            opcodeTable[opcode] = [this]() { ADC_A_n8(); };
        }
        else if (mnemonic == "RST 08H") {
            opcodeTable[opcode] = [this]() { RST_08H(); };
        }
        else if (mnemonic == "RET NC") {
            opcodeTable[opcode] = [this]() { RET_NC(); };
        }
        else if (mnemonic == "POP DE") {
            opcodeTable[opcode] = [this]() { POP_DE(); };
        }
        else if (mnemonic == "JP NC,a16") {
            opcodeTable[opcode] = [this]() { JP_NC_a16(); };
        }
        else if (mnemonic == "CALL NC,a16") {
            opcodeTable[opcode] = [this]() { CALL_NC_a16(); };
        }
        else if (mnemonic == "PUSH DE") {
            opcodeTable[opcode] = [this]() { PUSH_DE(); };
        }
        else if (mnemonic == "SUB n8") {
            opcodeTable[opcode] = [this]() { SUB_n8(); };
        }
        else if (mnemonic == "RST 10H") {
            opcodeTable[opcode] = [this]() { RST_10H(); };
        }
        else if (mnemonic == "RET C") {
            opcodeTable[opcode] = [this]() { RET_C(); };
        }
        else if (mnemonic == "RETI") {
            opcodeTable[opcode] = [this]() { RETI(); };
        }
        else if (mnemonic == "JP C,a16") {
            opcodeTable[opcode] = [this]() { JP_C_a16(); };
        }
        else if (mnemonic == "CALL C,a16") {
            opcodeTable[opcode] = [this]() { CALL_C_a16(); };
        }
        else if (mnemonic == "SBC A,n8") {
            opcodeTable[opcode] = [this]() { SBC_A_n8(); };
        }
        else if (mnemonic == "RST 18H") {
            opcodeTable[opcode] = [this]() { RST_18H(); };
        }
        else if (mnemonic == "LDH (a8),A") {
            opcodeTable[opcode] = [this]() { LDH_a8_A(); };
        }
        else if (mnemonic == "POP HL") {
            opcodeTable[opcode] = [this]() { POP_HL(); };
        }
        else if (mnemonic == "LD (C),A") {
            opcodeTable[opcode] = [this]() { LD_C_A(); };
        }
        else if (mnemonic == "PUSH HL") {
            opcodeTable[opcode] = [this]() { PUSH_HL(); };
        }
        else if (mnemonic == "AND n8") {
            opcodeTable[opcode] = [this]() { AND_n8(); };
        }
        else if (mnemonic == "RST 20H") {
            opcodeTable[opcode] = [this]() { RST_20H(); };
        }
        else if (mnemonic == "ADD SP,e8") {
            opcodeTable[opcode] = [this]() { ADD_SP_e8(); };
        }
        else if (mnemonic == "JP (HL)") {
            opcodeTable[opcode] = [this]() { JP_HL(); };
        }
        else if (mnemonic == "LD (a16),A") {
            opcodeTable[opcode] = [this]() { LD_a16_A(); };
        }
        else if (mnemonic == "XOR n8") {
            opcodeTable[opcode] = [this]() { XOR_n8(); };
        }
        else if (mnemonic == "RST 28H") {
            opcodeTable[opcode] = [this]() { RST_28H(); };
        }
        else if (mnemonic == "LDH A,(a8)") {
            opcodeTable[opcode] = [this]() { LDH_A_a8(); };
        }
        else if (mnemonic == "POP AF") {
            opcodeTable[opcode] = [this]() { POP_AF(); };
        }
        else if (mnemonic == "LD A,(C)") {
            opcodeTable[opcode] = [this]() { LDH_A_C(); };
        }
        else if (mnemonic == "DI") {
            opcodeTable[opcode] = [this]() { DI(); };
        }
        else if (mnemonic == "PUSH AF") {
            opcodeTable[opcode] = [this]() { PUSH_AF(); };
        }
        else if (mnemonic == "OR n8") {
            opcodeTable[opcode] = [this]() { OR_n8(); };
        }
        else if (mnemonic == "RST 30H") {
            opcodeTable[opcode] = [this]() { RST_30H(); };
        }
        else if (mnemonic == "LD HL,SP+e8") {
            opcodeTable[opcode] = [this]() { LD_HL_SP_e8(); };
        }
        else if (mnemonic == "LD SP,HL") {
            opcodeTable[opcode] = [this]() { LD_SP_HL(); };
        }
        else if (mnemonic == "LD A,(a16)") {
            opcodeTable[opcode] = [this]() { LD_A_a16(); };
        }
        else if (mnemonic == "EI") {
            opcodeTable[opcode] = [this]() { EI(); };
        }
        else if (mnemonic == "CP n8") {
            opcodeTable[opcode] = [this]() { CP_n8(); };
        }
        else if (mnemonic == "RST 38H") {
            opcodeTable[opcode] = [this]() { RST_38H(); };
        }
        else {
            std::cerr << "Unknown mnemonic: " << mnemonic << std::endl;
        }
    }
    // CB-prefixed opcodes
    else {
        if (mnemonic == "RLC B") {
            opcodeTable[opcode] = [this]() { RLC_B(); };
        }
        else if (mnemonic == "RLC C") {
            opcodeTable[opcode] = [this]() { RLC_C(); };
        }
        else if (mnemonic == "RLC D") {
            opcodeTable[opcode] = [this]() { RLC_D(); };
        }
        else if (mnemonic == "RLC E") {
            opcodeTable[opcode] = [this]() { RLC_E(); };
        }
        else if (mnemonic == "RLC H") {
            opcodeTable[opcode] = [this]() { RLC_H(); };
        }
        else if (mnemonic == "RLC L") {
            opcodeTable[opcode] = [this]() { RLC_L(); };
        }
        else if (mnemonic == "RLC (HL)") {
            opcodeTable[opcode] = [this]() { RLC_HLm(); };
        }
        else if (mnemonic == "RLC A") {
            opcodeTable[opcode] = [this]() { RLC_A(); };
        }
        else if (mnemonic == "RRC B") {
            opcodeTable[opcode] = [this]() { RRC_B(); };
        }
        else if (mnemonic == "RRC C") {
            opcodeTable[opcode] = [this]() { RRC_C(); };
        }
        else if (mnemonic == "RRC D") {
            opcodeTable[opcode] = [this]() { RRC_D(); };
        }
        else if (mnemonic == "RRC E") {
            opcodeTable[opcode] = [this]() { RRC_E(); };
        }
        else if (mnemonic == "RRC H") {
            opcodeTable[opcode] = [this]() { RRC_H(); };
        }
        else if (mnemonic == "RRC L") {
            opcodeTable[opcode] = [this]() { RRC_L(); };
        }
        else if (mnemonic == "RRC (HL)") {
            opcodeTable[opcode] = [this]() { RRC_HLm(); };
        }
        else if (mnemonic == "RRC A") {
            opcodeTable[opcode] = [this]() { RRC_A(); };
        }
        else if (mnemonic == "RL B") {
            opcodeTable[opcode] = [this]() { RL_B(); };
        }
        else if (mnemonic == "RL C") {
            opcodeTable[opcode] = [this]() { RL_C(); };
        }
        else if (mnemonic == "RL D") {
            opcodeTable[opcode] = [this]() { RL_D(); };
        }
        else if (mnemonic == "RL E") {
            opcodeTable[opcode] = [this]() { RL_E(); };
        }
        else if (mnemonic == "RL H") {
            opcodeTable[opcode] = [this]() { RL_H(); };
        }
        else if (mnemonic == "RL L") {
            opcodeTable[opcode] = [this]() { RL_L(); };
        }
        else if (mnemonic == "RL (HL)") {
            opcodeTable[opcode] = [this]() { RL_HLm(); };
        }
        else if (mnemonic == "RL A") {
            opcodeTable[opcode] = [this]() { RL_A(); };
        }
        else if (mnemonic == "RR B") {
            opcodeTable[opcode] = [this]() { RR_B(); };
        }
        else if (mnemonic == "RR C") {
            opcodeTable[opcode] = [this]() { RR_C(); };
        }
        else if (mnemonic == "RR D") {
            opcodeTable[opcode] = [this]() { RR_D(); };
        }
        else if (mnemonic == "RR E") {
            opcodeTable[opcode] = [this]() { RR_E(); };
        }
        else if (mnemonic == "RR H") {
            opcodeTable[opcode] = [this]() { RR_H(); };
        }
        else if (mnemonic == "RR L") {
            opcodeTable[opcode] = [this]() { RR_L(); };
        }
        else if (mnemonic == "RR (HL)") {
            opcodeTable[opcode] = [this]() { RR_HLm(); };
        }
        else if (mnemonic == "RR A") {
            opcodeTable[opcode] = [this]() { RR_A(); };
        }
        else if (mnemonic == "SLA B") {
            opcodeTable[opcode] = [this]() { SLA_B(); };
        }
        else if (mnemonic == "SLA C") {
            opcodeTable[opcode] = [this]() { SLA_C(); };
        }
        else if (mnemonic == "SLA D") {
            opcodeTable[opcode] = [this]() { SLA_D(); };
        }
        else if (mnemonic == "SLA E") {
            opcodeTable[opcode] = [this]() { SLA_E(); };
        }
        else if (mnemonic == "SLA H") {
            opcodeTable[opcode] = [this]() { SLA_H(); };
        }
        else if (mnemonic == "SLA L") {
            opcodeTable[opcode] = [this]() { SLA_L(); };
        }
        else if (mnemonic == "SLA (HL)") {
            opcodeTable[opcode] = [this]() { SLA_HLm(); };
        }
        else if (mnemonic == "SLA A") {
            opcodeTable[opcode] = [this]() { SLA_A(); };
        }
        else if (mnemonic == "SRA B") {
            opcodeTable[opcode] = [this]() { SRA_B(); };
        }
        else if (mnemonic == "SRA C") {
            opcodeTable[opcode] = [this]() { SRA_C(); };
        }
        else if (mnemonic == "SRA D") {
            opcodeTable[opcode] = [this]() { SRA_D(); };
        }
        else if (mnemonic == "SRA E") {
            opcodeTable[opcode] = [this]() { SRA_E(); };
        }
        else if (mnemonic == "SRA H") {
            opcodeTable[opcode] = [this]() { SRA_H(); };
        }
        else if (mnemonic == "SRA L") {
            opcodeTable[opcode] = [this]() { SRA_L(); };
        }
        else if (mnemonic == "SRA (HL)") {
            opcodeTable[opcode] = [this]() { SRA_HLm(); };
        }
        else if (mnemonic == "SRA A") {
            opcodeTable[opcode] = [this]() { SRA_A(); };
        }
        else if (mnemonic == "SWAP B") {
            opcodeTable[opcode] = [this]() { SWAP_B(); };
        }
        else if (mnemonic == "SWAP C") {
            opcodeTable[opcode] = [this]() { SWAP_C(); };
        }
        else if (mnemonic == "SWAP D") {
            opcodeTable[opcode] = [this]() { SWAP_D(); };
        }
        else if (mnemonic == "SWAP E") {
            opcodeTable[opcode] = [this]() { SWAP_E(); };
        }
        else if (mnemonic == "SWAP H") {
            opcodeTable[opcode] = [this]() { SWAP_H(); };
        }
        else if (mnemonic == "SWAP L") {
            opcodeTable[opcode] = [this]() { SWAP_L(); };
        }
        else if (mnemonic == "SWAP (HL)") {
            opcodeTable[opcode] = [this]() { SWAP_HLm(); };
        }
        else if (mnemonic == "SWAP A") {
            opcodeTable[opcode] = [this]() { SWAP_A(); };
        }
        else if (mnemonic == "SRL B") {
            opcodeTable[opcode] = [this]() { SRL_B(); };
        }
        else if (mnemonic == "SRL C") {
            opcodeTable[opcode] = [this]() { SRL_C(); };
        }
        else if (mnemonic == "SRL D") {
            opcodeTable[opcode] = [this]() { SRL_D(); };
        }
        else if (mnemonic == "SRL E") {
            opcodeTable[opcode] = [this]() { SRL_E(); };
        }
        else if (mnemonic == "SRL H") {
            opcodeTable[opcode] = [this]() { SRL_H(); };
        }
        else if (mnemonic == "SRL L") {
            opcodeTable[opcode] = [this]() { SRL_L(); };
        }
        else if (mnemonic == "SRL (HL)") {
            opcodeTable[opcode] = [this]() { SRL_HLm(); };
        }
        else if (mnemonic == "SRL A") {
            opcodeTable[opcode] = [this]() { SRL_A(); };
        }
        // BIT instructions
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
            opcodeTable[opcode] = [this]() { BIT_0_HLm(); };
        }
        else if (mnemonic == "BIT 0,A") {
            opcodeTable[opcode] = [this]() { BIT_0_A(); };
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
            opcodeTable[opcode] = [this]() { BIT_1_HLm(); };
        }
        else if (mnemonic == "BIT 1,A") {
            opcodeTable[opcode] = [this]() { BIT_1_A(); };
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
            opcodeTable[opcode] = [this]() { BIT_2_HLm(); };
        }
        else if (mnemonic == "BIT 2,A") {
            opcodeTable[opcode] = [this]() { BIT_2_A(); };
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
            opcodeTable[opcode] = [this]() { BIT_3_HLm(); };
        }
        else if (mnemonic == "BIT 3,A") {
            opcodeTable[opcode] = [this]() { BIT_3_A(); };
        }
        else if (mnemonic == "BIT 4,B") {
            opcodeTable[opcode] = [this]() { BIT_4_B(); };
        }
        else if (mnemonic == "BIT 4,C") {
            opcodeTable[opcode] = [this]() { BIT_4_C(); };
        }
        else if (mnemonic == "BIT 4,D") {
            opcodeTable[opcode] = [this]() { BIT_4_D(); };
        }
        else if (mnemonic == "BIT 4,E") {
            opcodeTable[opcode] = [this]() { BIT_4_E(); };
        }
        else if (mnemonic == "BIT 4,H") {
            opcodeTable[opcode] = [this]() { BIT_4_H(); };
        }
        else if (mnemonic == "BIT 4,L") {
            opcodeTable[opcode] = [this]() { BIT_4_L(); };
        }
        else if (mnemonic == "BIT 4,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_4_HLm(); };
        }
        else if (mnemonic == "BIT 4,A") {
            opcodeTable[opcode] = [this]() { BIT_4_A(); };
        }
        else if (mnemonic == "BIT 5,B") {
            opcodeTable[opcode] = [this]() { BIT_5_B(); };
        }
        else if (mnemonic == "BIT 5,C") {
            opcodeTable[opcode] = [this]() { BIT_5_C(); };
        }
        else if (mnemonic == "BIT 5,D") {
            opcodeTable[opcode] = [this]() { BIT_5_D(); };
        }
        else if (mnemonic == "BIT 5,E") {
            opcodeTable[opcode] = [this]() { BIT_5_E(); };
        }
        else if (mnemonic == "BIT 5,H") {
            opcodeTable[opcode] = [this]() { BIT_5_H(); };
        }
        else if (mnemonic == "BIT 5,L") {
            opcodeTable[opcode] = [this]() { BIT_5_L(); };
        }
        else if (mnemonic == "BIT 5,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_5_HLm(); };
        }
        else if (mnemonic == "BIT 5,A") {
            opcodeTable[opcode] = [this]() { BIT_5_A(); };
        }
        else if (mnemonic == "BIT 6,B") {
            opcodeTable[opcode] = [this]() { BIT_6_B(); };
        }
        else if (mnemonic == "BIT 6,C") {
            opcodeTable[opcode] = [this]() { BIT_6_C(); };
        }
        else if (mnemonic == "BIT 6,D") {
            opcodeTable[opcode] = [this]() { BIT_6_D(); };
        }
        else if (mnemonic == "BIT 6,E") {
            opcodeTable[opcode] = [this]() { BIT_6_E(); };
        }
        else if (mnemonic == "BIT 6,H") {
            opcodeTable[opcode] = [this]() { BIT_6_H(); };
        }
        else if (mnemonic == "BIT 6,L") {
            opcodeTable[opcode] = [this]() { BIT_6_L(); };
        }
        else if (mnemonic == "BIT 6,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_6_HLm(); };
        }
        else if (mnemonic == "BIT 6,A") {
            opcodeTable[opcode] = [this]() { BIT_6_A(); };
        }
        else if (mnemonic == "BIT 7,B") {
            opcodeTable[opcode] = [this]() { BIT_7_B(); };
        }
        else if (mnemonic == "BIT 7,C") {
            opcodeTable[opcode] = [this]() { BIT_7_C(); };
        }
        else if (mnemonic == "BIT 7,D") {
            opcodeTable[opcode] = [this]() { BIT_7_D(); };
        }
        else if (mnemonic == "BIT 7,E") {
            opcodeTable[opcode] = [this]() { BIT_7_E(); };
        }
        else if (mnemonic == "BIT 7,H") {
            opcodeTable[opcode] = [this]() { BIT_7_H(); };
        }
        else if (mnemonic == "BIT 7,L") {
            opcodeTable[opcode] = [this]() { BIT_7_L(); };
        }
        else if (mnemonic == "BIT 7,(HL)") {
            opcodeTable[opcode] = [this]() { BIT_7_HLm(); };
        }
        else if (mnemonic == "BIT 7,A") {
            opcodeTable[opcode] = [this]() { BIT_7_A(); };
        }
        else if (mnemonic == "RES 0,B") {
            opcodeTable[opcode] = [this]() { RES_0_B(); };
        }
        else if (mnemonic == "RES 0,C") {
            opcodeTable[opcode] = [this]() { RES_0_C(); };
        }
        else if (mnemonic == "RES 0,D") {
            opcodeTable[opcode] = [this]() { RES_0_D(); };
        }
        else if (mnemonic == "RES 0,E") {
            opcodeTable[opcode] = [this]() { RES_0_E(); };
        }
        else if (mnemonic == "RES 0,H") {
            opcodeTable[opcode] = [this]() { RES_0_H(); };
        }
        else if (mnemonic == "RES 0,L") {
            opcodeTable[opcode] = [this]() { RES_0_L(); };
        }
        else if (mnemonic == "RES 0,(HL)") {
            opcodeTable[opcode] = [this]() { RES_0_HLm(); };
        }
        else if (mnemonic == "RES 0,A") {
            opcodeTable[opcode] = [this]() { RES_0_A(); };
        }
        else if (mnemonic == "RES 1,B") {
            opcodeTable[opcode] = [this]() { RES_1_B(); };
        }
        else if (mnemonic == "RES 1,C") {
            opcodeTable[opcode] = [this]() { RES_1_C(); };
        }
        else if (mnemonic == "RES 1,D") {
            opcodeTable[opcode] = [this]() { RES_1_D(); };
        }
        else if (mnemonic == "RES 1,E") {
            opcodeTable[opcode] = [this]() { RES_1_E(); };
        }
        else if (mnemonic == "RES 1,H") {
            opcodeTable[opcode] = [this]() { RES_1_H(); };
        }
        else if (mnemonic == "RES 1,L") {
            opcodeTable[opcode] = [this]() { RES_1_L(); };
        }
        else if (mnemonic == "RES 1,(HL)") {
            opcodeTable[opcode] = [this]() { RES_1_HLm(); };
        }
        else if (mnemonic == "RES 1,A") {
            opcodeTable[opcode] = [this]() { RES_1_A(); };
        }
        else if (mnemonic == "RES 2,B") {
            opcodeTable[opcode] = [this]() { RES_2_B(); };
        }
        else if (mnemonic == "RES 2,C") {
            opcodeTable[opcode] = [this]() { RES_2_C(); };
        }
        else if (mnemonic == "RES 2,D") {
            opcodeTable[opcode] = [this]() { RES_2_D(); };
        }
        else if (mnemonic == "RES 2,E") {
            opcodeTable[opcode] = [this]() { RES_2_E(); };
        }
        else if (mnemonic == "RES 2,H") {
            opcodeTable[opcode] = [this]() { RES_2_H(); };
        }
        else if (mnemonic == "RES 2,L") {
            opcodeTable[opcode] = [this]() { RES_2_L(); };
        }
        else if (mnemonic == "RES 2,(HL)") {
            opcodeTable[opcode] = [this]() { RES_2_HLm(); };
        }
        else if (mnemonic == "RES 2,A") {
            opcodeTable[opcode] = [this]() { RES_2_A(); };
        }
        else if (mnemonic == "RES 3,B") {
            opcodeTable[opcode] = [this]() { RES_3_B(); };
        }
        else if (mnemonic == "RES 3,C") {
            opcodeTable[opcode] = [this]() { RES_3_C(); };
        }
        else if (mnemonic == "RES 3,D") {
            opcodeTable[opcode] = [this]() { RES_3_D(); };
        }
        else if (mnemonic == "RES 3,E") {
            opcodeTable[opcode] = [this]() { RES_3_E(); };
        }
        else if (mnemonic == "RES 3,H") {
            opcodeTable[opcode] = [this]() { RES_3_H(); };
        }
        else if (mnemonic == "RES 3,L") {
            opcodeTable[opcode] = [this]() { RES_3_L(); };
        }
        else if (mnemonic == "RES 3,(HL)") {
            opcodeTable[opcode] = [this]() { RES_3_HLm(); };
        }
        else if (mnemonic == "RES 3,A") {
            opcodeTable[opcode] = [this]() { RES_3_A(); };
        }
        else if (mnemonic == "RES 4,B") {
            opcodeTable[opcode] = [this]() { RES_4_B(); };
        }
        else if (mnemonic == "RES 4,C") {
            opcodeTable[opcode] = [this]() { RES_4_C(); };
        }
        else if (mnemonic == "RES 4,D") {
            opcodeTable[opcode] = [this]() { RES_4_D(); };
        }
        else if (mnemonic == "RES 4,E") {
            opcodeTable[opcode] = [this]() { RES_4_E(); };
        }
        else if (mnemonic == "RES 4,H") {
            opcodeTable[opcode] = [this]() { RES_4_H(); };
        }
        else if (mnemonic == "RES 4,L") {
            opcodeTable[opcode] = [this]() { RES_4_L(); };
        }
        else if (mnemonic == "RES 4,(HL)") {
            opcodeTable[opcode] = [this]() { RES_4_HLm(); };
        }
        else if (mnemonic == "RES 4,A") {
            opcodeTable[opcode] = [this]() { RES_4_A(); };
        }
        else if (mnemonic == "RES 5,B") {
            opcodeTable[opcode] = [this]() { RES_5_B(); };
        }
        else if (mnemonic == "RES 5,C") {
            opcodeTable[opcode] = [this]() { RES_5_C(); };
        }
        else if (mnemonic == "RES 5,D") {
            opcodeTable[opcode] = [this]() { RES_5_D(); };
        }
        else if (mnemonic == "RES 5,E") {
            opcodeTable[opcode] = [this]() { RES_5_E(); };
        }
        else if (mnemonic == "RES 5,H") {
            opcodeTable[opcode] = [this]() { RES_5_H(); };
        }
        else if (mnemonic == "RES 5,L") {
            opcodeTable[opcode] = [this]() { RES_5_L(); };
        }
        else if (mnemonic == "RES 5,(HL)") {
            opcodeTable[opcode] = [this]() { RES_5_HLm(); };
        }
        else if (mnemonic == "RES 5,A") {
            opcodeTable[opcode] = [this]() { RES_5_A(); };
        }
        else if (mnemonic == "RES 6,B") {
            opcodeTable[opcode] = [this]() { RES_6_B(); };
        }
        else if (mnemonic == "RES 6,C") {
            opcodeTable[opcode] = [this]() { RES_6_C(); };
        }
        else if (mnemonic == "RES 6,D") {
            opcodeTable[opcode] = [this]() { RES_6_D(); };
        }
        else if (mnemonic == "RES 6,E") {
            opcodeTable[opcode] = [this]() { RES_6_E(); };
        }
        else if (mnemonic == "RES 6,H") {
            opcodeTable[opcode] = [this]() { RES_6_H(); };
        }
        else if (mnemonic == "RES 6,L") {
            opcodeTable[opcode] = [this]() { RES_6_L(); };
        }
        else if (mnemonic == "RES 6,(HL)") {
            opcodeTable[opcode] = [this]() { RES_6_HLm(); };
        }
        else if (mnemonic == "RES 6,A") {
            opcodeTable[opcode] = [this]() { RES_6_A(); };
        }
        else if (mnemonic == "RES 7,B") {
            opcodeTable[opcode] = [this]() { RES_7_B(); };
        }
        else if (mnemonic == "RES 7,C") {
            opcodeTable[opcode] = [this]() { RES_7_C(); };
        }
        else if (mnemonic == "RES 7,D") {
            opcodeTable[opcode] = [this]() { RES_7_D(); };
        }
        else if (mnemonic == "RES 7,E") {
            opcodeTable[opcode] = [this]() { RES_7_E(); };
        }
        else if (mnemonic == "RES 7,H") {
            opcodeTable[opcode] = [this]() { RES_7_H(); };
        }
        else if (mnemonic == "RES 7,L") {
            opcodeTable[opcode] = [this]() { RES_7_L(); };
        }
        else if (mnemonic == "RES 7,(HL)") {
            opcodeTable[opcode] = [this]() { RES_7_HLm(); };
        }
        else if (mnemonic == "RES 7,A") {
            opcodeTable[opcode] = [this]() { RES_7_A(); };
        }
        else if (mnemonic == "SET 0,B") {
            opcodeTable[opcode] = [this]() { SET_0_B(); };
        }
        else if (mnemonic == "SET 0,C") {
            opcodeTable[opcode] = [this]() { SET_0_C(); };
        }
        else if (mnemonic == "SET 0,D") {
            opcodeTable[opcode] = [this]() { SET_0_D(); };
        }
        else if (mnemonic == "SET 0,E") {
            opcodeTable[opcode] = [this]() { SET_0_E(); };
        }
        else if (mnemonic == "SET 0,H") {
            opcodeTable[opcode] = [this]() { SET_0_H(); };
        }
        else if (mnemonic == "SET 0,L") {
            opcodeTable[opcode] = [this]() { SET_0_L(); };
        }
        else if (mnemonic == "SET 0,(HL)") {
            opcodeTable[opcode] = [this]() { SET_0_HLm(); };
        }
        else if (mnemonic == "SET 0,A") {
            opcodeTable[opcode] = [this]() { SET_0_A(); };
        }
        else if (mnemonic == "SET 1,B") {
            opcodeTable[opcode] = [this]() { SET_1_B(); };
        }
        else if (mnemonic == "SET 1,C") {
            opcodeTable[opcode] = [this]() { SET_1_C(); };
        }
        else if (mnemonic == "SET 1,D") {
            opcodeTable[opcode] = [this]() { SET_1_D(); };
        }
        else if (mnemonic == "SET 1,E") {
            opcodeTable[opcode] = [this]() { SET_1_E(); };
        }
        else if (mnemonic == "SET 1,H") {
            opcodeTable[opcode] = [this]() { SET_1_H(); };
        }
        else if (mnemonic == "SET 1,L") {
            opcodeTable[opcode] = [this]() { SET_1_L(); };
        }
        else if (mnemonic == "SET 1,(HL)") {
            opcodeTable[opcode] = [this]() { SET_1_HLm(); };
        }
        else if (mnemonic == "SET 1,A") {
            opcodeTable[opcode] = [this]() { SET_1_A(); };
        }
        else if (mnemonic == "SET 2,B") {
            opcodeTable[opcode] = [this]() { SET_2_B(); };
        }
        else if (mnemonic == "SET 2,C") {
            opcodeTable[opcode] = [this]() { SET_2_C(); };
        }
        else if (mnemonic == "SET 2,D") {
            opcodeTable[opcode] = [this]() { SET_2_D(); };
        }
        else if (mnemonic == "SET 2,E") {
            opcodeTable[opcode] = [this]() { SET_2_E(); };
        }
        else if (mnemonic == "SET 2,H") {
            opcodeTable[opcode] = [this]() { SET_2_H(); };
        }
        else if (mnemonic == "SET 2,L") {
            opcodeTable[opcode] = [this]() { SET_2_L(); };
        }
        else if (mnemonic == "SET 2,(HL)") {
            opcodeTable[opcode] = [this]() { SET_2_HLm(); };
        }
        else if (mnemonic == "SET 2,A") {
            opcodeTable[opcode] = [this]() { SET_2_A(); };
        }
        else if (mnemonic == "SET 3,B") {
            opcodeTable[opcode] = [this]() { SET_3_B(); };
        }
        else if (mnemonic == "SET 3,C") {
            opcodeTable[opcode] = [this]() { SET_3_C(); };
        }
        else if (mnemonic == "SET 3,D") {
            opcodeTable[opcode] = [this]() { SET_3_D(); };
        }
        else if (mnemonic == "SET 3,E") {
            opcodeTable[opcode] = [this]() { SET_3_E(); };
        }
        else if (mnemonic == "SET 3,H") {
            opcodeTable[opcode] = [this]() { SET_3_H(); };
        }
        else if (mnemonic == "SET 3,L") {
            opcodeTable[opcode] = [this]() { SET_3_L(); };
        }
        else if (mnemonic == "SET 3,(HL)") {
            opcodeTable[opcode] = [this]() { SET_3_HLm(); };
        }
        else if (mnemonic == "SET 3,A") {
            opcodeTable[opcode] = [this]() { SET_3_A(); };
        }
        else if (mnemonic == "SET 4,B") {
            opcodeTable[opcode] = [this]() { SET_4_B(); };
        }
        else if (mnemonic == "SET 4,C") {
            opcodeTable[opcode] = [this]() { SET_4_C(); };
        }
        else if (mnemonic == "SET 4,D") {
            opcodeTable[opcode] = [this]() { SET_4_D(); };
        }
        else if (mnemonic == "SET 4,E") {
            opcodeTable[opcode] = [this]() { SET_4_E(); };
        }
        else if (mnemonic == "SET 4,H") {
            opcodeTable[opcode] = [this]() { SET_4_H(); };
        }
        else if (mnemonic == "SET 4,L") {
            opcodeTable[opcode] = [this]() { SET_4_L(); };
        }
        else if (mnemonic == "SET 4,(HL)") {
            opcodeTable[opcode] = [this]() { SET_4_HLm(); };
        }
        else if (mnemonic == "SET 4,A") {
            opcodeTable[opcode] = [this]() { SET_4_A(); };
        }
        else if (mnemonic == "SET 5,B") {
            opcodeTable[opcode] = [this]() { SET_5_B(); };
        }
        else if (mnemonic == "SET 5,C") {
            opcodeTable[opcode] = [this]() { SET_5_C(); };
        }
        else if (mnemonic == "SET 5,D") {
            opcodeTable[opcode] = [this]() { SET_5_D(); };
        }
        else if (mnemonic == "SET 5,E") {
            opcodeTable[opcode] = [this]() { SET_5_E(); };
        }
        else if (mnemonic == "SET 5,H") {
            opcodeTable[opcode] = [this]() { SET_5_H(); };
        }
        else if (mnemonic == "SET 5,L") {
            opcodeTable[opcode] = [this]() { SET_5_L(); };
        }
        else if (mnemonic == "SET 5,(HL)") {
            opcodeTable[opcode] = [this]() { SET_5_HLm(); };
        }
        else if (mnemonic == "SET 5,A") {
            opcodeTable[opcode] = [this]() { SET_5_A(); };
        }
        else if (mnemonic == "SET 6,B") {
            opcodeTable[opcode] = [this]() { SET_6_B(); };
        }
        else if (mnemonic == "SET 6,C") {
            opcodeTable[opcode] = [this]() { SET_6_C(); };
        }
        else if (mnemonic == "SET 6,D") {
            opcodeTable[opcode] = [this]() { SET_6_D(); };
        }
        else if (mnemonic == "SET 6,E") {
            opcodeTable[opcode] = [this]() { SET_6_E(); };
        }
        else if (mnemonic == "SET 6,H") {
            opcodeTable[opcode] = [this]() { SET_6_H(); };
        }
        else if (mnemonic == "SET 6,L") {
            opcodeTable[opcode] = [this]() { SET_6_L(); };
        }
        else if (mnemonic == "SET 6,(HL)") {
            opcodeTable[opcode] = [this]() { SET_6_HLm(); };
        }
        else if (mnemonic == "SET 6,A") {
            opcodeTable[opcode] = [this]() { SET_6_A(); };
        }
        else if (mnemonic == "SET 7,B") {
            opcodeTable[opcode] = [this]() { SET_7_B(); };
        }
        else if (mnemonic == "SET 7,C") {
            opcodeTable[opcode] = [this]() { SET_7_C(); };
        }
        else if (mnemonic == "SET 7,D") {
            opcodeTable[opcode] = [this]() { SET_7_D(); };
        }
        else if (mnemonic == "SET 7,E") {
            opcodeTable[opcode] = [this]() { SET_7_E(); };
        }
        else if (mnemonic == "SET 7,H") {
            opcodeTable[opcode] = [this]() { SET_7_H(); };
        }
        else if (mnemonic == "SET 7,L") {
            opcodeTable[opcode] = [this]() { SET_7_L(); };
        }
        else if (mnemonic == "SET 7,(HL)") {
            opcodeTable[opcode] = [this]() { SET_7_HLm(); };
        }
        else if (mnemonic == "SET 7,A") {
            opcodeTable[opcode] = [this]() { SET_7_A(); };
        }
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

void CPU::INC_H() {
    u8 result = m_registers.h + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.h & 0x0F) == 0x0F);
    
    m_registers.h = result;
    m_cycles += 4;
}

void CPU::DEC_H() {
    u8 result = m_registers.h - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.h & 0x0F) == 0x00);
    
    m_registers.h = result;
    m_cycles += 4;
}

void CPU::LD_H_n8() {
    m_registers.h = readPC();
    m_cycles += 8;
}

void CPU::DAA() {
    u8 a = m_registers.a;
    bool carry = getFlag(FLAG_C);
    
    if (!getFlag(FLAG_N)) {
        if (carry || a > 0x99) {
            a += 0x60;
            setFlag(FLAG_C, true);
        }
        if (getFlag(FLAG_H) || (a & 0x0F) > 0x09) {
            a += 0x06;
        }
    } else {
        if (carry) {
            a -= 0x60;
        }
        if (getFlag(FLAG_H)) {
            a -= 0x06;
        }
    }
    
    setFlag(FLAG_Z, a == 0);
    setFlag(FLAG_H, false);
    m_registers.a = a;
    m_cycles += 4;
}

void CPU::JR_Z_e8() {
    s8 offset = static_cast<s8>(readPC());
    
    if (getFlag(FLAG_Z)) {
        m_registers.pc += offset;
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

void CPU::ADD_HL_HL() {
    u32 result = m_registers.hl + m_registers.hl;
    
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.hl & 0x0FFF) > 0x0FFF);
    setFlag(FLAG_C, result > 0xFFFF);
    
    m_registers.hl = result & 0xFFFF;
    m_cycles += 8;
}

void CPU::LD_A_HLI() {
    m_registers.a = m_memory.read(m_registers.hl);
    m_registers.hl++;
    m_cycles += 8;
}

void CPU::DEC_HL() {
    m_registers.hl--;
    m_cycles += 8;
}

void CPU::INC_L() {
    u8 result = m_registers.l + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.l & 0x0F) == 0x0F);
    
    m_registers.l = result;
    m_cycles += 4;
}

void CPU::DEC_L() {
    u8 result = m_registers.l - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.l & 0x0F) == 0x00);
    
    m_registers.l = result;
    m_cycles += 4;
}

void CPU::LD_L_n8() {
    m_registers.l = readPC();
    m_cycles += 8;
}

void CPU::CPL() {
    m_registers.a = ~m_registers.a;
    
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, true);
    
    m_cycles += 4;
}

void CPU::JR_NC_e8() {
    s8 offset = static_cast<s8>(readPC());
    
    if (!getFlag(FLAG_C)) {
        m_registers.pc += offset;
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

void CPU::LD_SP_n16() {
    m_registers.sp = readPC16();
    m_cycles += 12;
}

void CPU::LD_HLD_A() {
    m_memory.write(m_registers.hl, m_registers.a);
    m_registers.hl--;
    m_cycles += 8;
}

void CPU::INC_SP() {
    m_registers.sp++;
    m_cycles += 8;
}

void CPU::INC_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 result = value + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (value & 0x0F) == 0x0F);
    
    m_memory.write(m_registers.hl, result);
    m_cycles += 12;
}

void CPU::DEC_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 result = value - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (value & 0x0F) == 0x00);
    
    m_memory.write(m_registers.hl, result);
    m_cycles += 12;
}

void CPU::LD_HL_n8() {
    m_memory.write(m_registers.hl, readPC());
    m_cycles += 12;
}

void CPU::SCF() {
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, true);
    
    m_cycles += 4;
}

void CPU::JR_C_e8() {
    s8 offset = static_cast<s8>(readPC());
    
    if (getFlag(FLAG_C)) {
        m_registers.pc += offset;
        m_cycles += 12;
    } else {
        m_cycles += 8;
    }
}

void CPU::ADD_HL_SP() {
    u32 result = m_registers.hl + m_registers.sp;
    
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.hl & 0x0FFF) + (m_registers.sp & 0x0FFF) > 0x0FFF);
    setFlag(FLAG_C, result > 0xFFFF);
    
    m_registers.hl = result & 0xFFFF;
    m_cycles += 8;
}

void CPU::LD_A_HLD() {
    m_registers.a = m_memory.read(m_registers.hl);
    m_registers.hl--;
    m_cycles += 8;
}

void CPU::DEC_SP() {
    m_registers.sp--;
    m_cycles += 8;
}

void CPU::INC_A() {
    u8 result = m_registers.a + 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) == 0x0F);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::DEC_A() {
    u8 result = m_registers.a - 1;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) == 0x00);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::LD_A_n8() {
    m_registers.a = readPC();
    m_cycles += 8;
}

void CPU::LD_B_B() {
    // No operation needed - loading B into itself
    m_cycles += 4;
}

void CPU::LD_B_C() {
    m_registers.b = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_B_D() {
    m_registers.b = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_B_E() {
    m_registers.b = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_B_H() {
    m_registers.b = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_B_L() {
    m_registers.b = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_B_HLm() {
    m_registers.b = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_B_A() {
    m_registers.b = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_C_B() {
    m_registers.c = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_C_C() {
    // No operation needed - loading C into itself
    m_cycles += 4;
}

void CPU::LD_C_D() {
    m_registers.c = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_C_E() {
    m_registers.c = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_C_H() {
    m_registers.c = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_C_L() {
    m_registers.c = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_C_HLm() {
    m_registers.c = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_C_A() {
    m_registers.c = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_D_B() {
    m_registers.d = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_D_C() {
    m_registers.d = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_D_D() {
    // No operation needed - loading D into itself
    m_cycles += 4;
}

void CPU::LD_D_E() {
    m_registers.d = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_D_H() {
    m_registers.d = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_D_L() {
    m_registers.d = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_D_HLm() {
    m_registers.d = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_D_A() {
    m_registers.d = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_E_B() {
    m_registers.e = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_E_C() {
    m_registers.e = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_E_D() {
    m_registers.e = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_E_E() {
    // No operation needed - loading E into itself
    m_cycles += 4;
}

void CPU::LD_E_H() {
    m_registers.e = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_E_L() {
    m_registers.e = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_E_HLm() {
    m_registers.e = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_E_A() {
    m_registers.e = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_H_B() {
    m_registers.h = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_H_C() {
    m_registers.h = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_H_D() {
    m_registers.h = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_H_E() {
    m_registers.h = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_H_H() {
    // No operation needed - loading H into itself
    m_cycles += 4;
}

void CPU::LD_H_L() {
    m_registers.h = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_H_HLm() {
    m_registers.h = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_H_A() {
    m_registers.h = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_L_B() {
    m_registers.l = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_L_C() {
    m_registers.l = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_L_D() {
    m_registers.l = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_L_E() {
    m_registers.l = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_L_H() {
    m_registers.l = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_L_L() {
    // No operation needed - loading L into itself
    m_cycles += 4;
}

void CPU::LD_L_HLm() {
    m_registers.l = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_L_A() {
    m_registers.l = m_registers.a;
    m_cycles += 4;
}

void CPU::LD_HLm_B() {
    m_memory.write(m_registers.hl, m_registers.b);
    m_cycles += 8;
}

void CPU::LD_HLm_C() {
    m_memory.write(m_registers.hl, m_registers.c);
    m_cycles += 8;
}

void CPU::LD_HLm_D() {
    m_memory.write(m_registers.hl, m_registers.d);
    m_cycles += 8;
}

void CPU::LD_HLm_E() {
    m_memory.write(m_registers.hl, m_registers.e);
    m_cycles += 8;
}

void CPU::LD_HLm_H() {
    m_memory.write(m_registers.hl, m_registers.h);
    m_cycles += 8;
}

void CPU::LD_HLm_L() {
    m_memory.write(m_registers.hl, m_registers.l);
    m_cycles += 8;
}

void CPU::HALT() {
    m_halted = true;
    m_cycles += 4;
}

void CPU::LD_HLm_A() {
    m_memory.write(m_registers.hl, m_registers.a);
    m_cycles += 8;
}

void CPU::LD_A_B() {
    m_registers.a = m_registers.b;
    m_cycles += 4;
}

void CPU::LD_A_C() {
    m_registers.a = m_registers.c;
    m_cycles += 4;
}

void CPU::LD_A_D() {
    m_registers.a = m_registers.d;
    m_cycles += 4;
}

void CPU::LD_A_E() {
    m_registers.a = m_registers.e;
    m_cycles += 4;
}

void CPU::LD_A_H() {
    m_registers.a = m_registers.h;
    m_cycles += 4;
}

void CPU::LD_A_L() {
    m_registers.a = m_registers.l;
    m_cycles += 4;
}

void CPU::LD_A_HLm() {
    m_registers.a = m_memory.read(m_registers.hl);
    m_cycles += 8;
}

void CPU::LD_A_A() {
    // No operation needed - loading A into itself
    m_cycles += 4;
}

void CPU::ADD_A_B() {
    u16 result = m_registers.a + m_registers.b;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.b & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_C() {
    u16 result = m_registers.a + m_registers.c;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.c & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_D() {
    u16 result = m_registers.a + m_registers.d;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.d & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_E() {
    u16 result = m_registers.a + m_registers.e;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.e & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_H() {
    u16 result = m_registers.a + m_registers.h;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.h & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_L() {
    u16 result = m_registers.a + m_registers.l;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.l & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADD_A_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a + value;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

void CPU::ADD_A_A() {
    u16 result = m_registers.a + m_registers.a;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.a & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_B() {
    u16 result = m_registers.a + m_registers.b + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.b & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_C() {
    u16 result = m_registers.a + m_registers.c + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.c & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_D() {
    u16 result = m_registers.a + m_registers.d + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.d & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_E() {
    u16 result = m_registers.a + m_registers.e + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.e & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_H() {
    u16 result = m_registers.a + m_registers.h + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.h & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_L() {
    u16 result = m_registers.a + m_registers.l + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.l & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::ADC_A_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

void CPU::ADC_A_A() {
    u16 result = m_registers.a + m_registers.a + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (m_registers.a & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 4;
}

void CPU::SUB_B() {
    u8 result = m_registers.a - m_registers.b;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.b & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.b);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_C() {
    u8 result = m_registers.a - m_registers.c;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.c & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.c);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_D() {
    u8 result = m_registers.a - m_registers.d;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.d & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.d);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_E() {
    u8 result = m_registers.a - m_registers.e;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.e & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.e);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_H() {
    u8 result = m_registers.a - m_registers.h;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.h & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.h);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_L() {
    u8 result = m_registers.a - m_registers.l;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.l & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.l);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 result = m_registers.a - value;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, m_registers.a < value);
    
    m_registers.a = result;
    m_cycles += 8;
}

void CPU::SUB_A() {
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_registers.a = 0;
    m_cycles += 4;
}

void CPU::SBC_A_B() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.b - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.b & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.b + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_C() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.c - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.c & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.c + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_D() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.d - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.d & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.d + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_E() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.e - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.e & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.e + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_H() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.h - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.h & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.h + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_L() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - m_registers.l - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.l & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < m_registers.l + carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SBC_A_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - value - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < value + carry);
    
    m_registers.a = result;
    m_cycles += 8;
}

void CPU::SBC_A_A() {
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = carry ? 0xFF : 0;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, carry);
    setFlag(FLAG_C, carry);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::AND_B() {
    m_registers.a &= m_registers.b;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_C() {
    m_registers.a &= m_registers.c;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_D() {
    m_registers.a &= m_registers.d;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_E() {
    m_registers.a &= m_registers.e;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_H() {
    m_registers.a &= m_registers.h;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_L() {
    m_registers.a &= m_registers.l;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::AND_HLm() {
    m_registers.a &= m_memory.read(m_registers.hl);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::AND_A() {
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_B() {
    m_registers.a ^= m_registers.b;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_C() {
    m_registers.a ^= m_registers.c;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_D() {
    m_registers.a ^= m_registers.d;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_E() {
    m_registers.a ^= m_registers.e;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_H() {
    m_registers.a ^= m_registers.h;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_L() {
    m_registers.a ^= m_registers.l;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::XOR_HLm() {
    m_registers.a ^= m_memory.read(m_registers.hl);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::XOR_A() {
    m_registers.a = 0;
    
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_B() {
    m_registers.a |= m_registers.b;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_C() {
    m_registers.a |= m_registers.c;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_D() {
    m_registers.a |= m_registers.d;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_E() {
    m_registers.a |= m_registers.e;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_H() {
    m_registers.a |= m_registers.h;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_L() {
    m_registers.a |= m_registers.l;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::OR_HLm() {
    m_registers.a |= m_memory.read(m_registers.hl);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::OR_A() {
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::CP_B() {
    u8 result = m_registers.a - m_registers.b;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.b & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.b);
    
    m_cycles += 4;
}

void CPU::CP_C() {
    u8 result = m_registers.a - m_registers.c;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.c & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.c);
    
    m_cycles += 4;
}

void CPU::CP_D() {
    u8 result = m_registers.a - m_registers.d;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.d & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.d);
    
    m_cycles += 4;
}

void CPU::CP_E() {
    u8 result = m_registers.a - m_registers.e;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.e & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.e);
    
    m_cycles += 4;
}

void CPU::CP_H() {
    u8 result = m_registers.a - m_registers.h;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.h & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.h);
    
    m_cycles += 4;
}

void CPU::CP_L() {
    u8 result = m_registers.a - m_registers.l;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.l & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.l);
    
    m_cycles += 4;
}

void CPU::CP_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 result = m_registers.a - value;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, m_registers.a < value);
    
    m_cycles += 8;
}

void CPU::CP_A() {
    setFlag(FLAG_Z, true);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 4;
}

void CPU::RET_NZ() {
    if (!getFlag(FLAG_Z)) {
        m_registers.pc = pop();
        m_cycles += 20;
    } else {
        m_cycles += 8;
    }
}

void CPU::POP_BC() {
    m_registers.bc = pop();
    m_cycles += 12;
}

void CPU::JP_NZ_a16() {
    u16 address = readPC16();
    
    if (!getFlag(FLAG_Z)) {
        m_registers.pc = address;
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

void CPU::JP_a16() {
    m_registers.pc = readPC16();
    m_cycles += 16;
}

void CPU::CALL_NZ_a16() {
    u16 address = readPC16();
    
    if (!getFlag(FLAG_Z)) {
        push(m_registers.pc);
        m_registers.pc = address;
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

void CPU::PUSH_BC() {
    push(m_registers.bc);
    m_cycles += 16;
}

void CPU::ADD_A_n8() {
    u8 value = readPC();
    u16 result = m_registers.a + value;
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

void CPU::RST_00H() {
    push(m_registers.pc);
    m_registers.pc = 0x0000;
    m_cycles += 16;
}

void CPU::RET_Z() {
    if (getFlag(FLAG_Z)) {
        m_registers.pc = pop();
        m_cycles += 20;
    } else {
        m_cycles += 8;
    }
}

void CPU::RET() {
    m_registers.pc = pop();
    m_cycles += 16;
}

void CPU::JP_Z_a16() {
    u16 address = readPC16();
    
    if (getFlag(FLAG_Z)) {
        m_registers.pc = address;
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

void CPU::PREFIX_CB() {
    u8 opcode = readPC();
    executeCBOpcode(opcode);
    m_cycles += 4;
}

void CPU::CALL_Z_a16() {
    u16 address = readPC16();
    
    if (getFlag(FLAG_Z)) {
        push(m_registers.pc);
        m_registers.pc = address;
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

void CPU::CALL_a16() {
    u16 address = readPC16();
    push(m_registers.pc);
    m_registers.pc = address;
    m_cycles += 24;
}

void CPU::ADC_A_n8() {
    u8 value = readPC();
    u16 result = m_registers.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_Z, (result & 0xFF) == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.a & 0x0F) + (value & 0x0F) + (getFlag(FLAG_C) ? 1 : 0) > 0x0F);
    setFlag(FLAG_C, result > 0xFF);
    
    m_registers.a = result & 0xFF;
    m_cycles += 8;
}

void CPU::RST_08H() {
    push(m_registers.pc);
    m_registers.pc = 0x0008;
    m_cycles += 16;
}

void CPU::RET_NC() {
    if (!getFlag(FLAG_C)) {
        m_registers.pc = pop();
        m_cycles += 20;
    } else {
        m_cycles += 8;
    }
}

void CPU::POP_DE() {
    m_registers.de = pop();
    m_cycles += 12;
}

void CPU::JP_NC_a16() {
    u16 address = readPC16();
    
    if (!getFlag(FLAG_C)) {
        m_registers.pc = address;
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

void CPU::CALL_NC_a16() {
    u16 address = readPC16();
    
    if (!getFlag(FLAG_C)) {
        push(m_registers.pc);
        m_registers.pc = address;
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

void CPU::PUSH_DE() {
    push(m_registers.de);
    m_cycles += 16;
}

void CPU::SUB_n8() {
    u8 value = readPC();
    u8 result = m_registers.a - value;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, m_registers.a < value);
    
    m_registers.a = result;
    m_cycles += 8;
}

void CPU::RST_10H() {
    push(m_registers.pc);
    m_registers.pc = 0x0010;
    m_cycles += 16;
}

void CPU::RET_C() {
    if (getFlag(FLAG_C)) {
        m_registers.pc = pop();
        m_cycles += 20;
    } else {
        m_cycles += 8;
    }
}

void CPU::RETI() {
    m_registers.pc = pop();
    m_interruptsEnabled = true;
    m_cycles += 16;
}

void CPU::JP_C_a16() {
    u16 address = readPC16();
    
    if (getFlag(FLAG_C)) {
        m_registers.pc = address;
        m_cycles += 16;
    } else {
        m_cycles += 12;
    }
}

void CPU::CALL_C_a16() {
    u16 address = readPC16();
    
    if (getFlag(FLAG_C)) {
        push(m_registers.pc);
        m_registers.pc = address;
        m_cycles += 24;
    } else {
        m_cycles += 12;
    }
}

void CPU::SBC_A_n8() {
    u8 value = readPC();
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    u8 result = m_registers.a - value - carry;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F) + carry);
    setFlag(FLAG_C, m_registers.a < value + carry);
    
    m_registers.a = result;
    m_cycles += 8;
}

void CPU::RST_18H() {
    push(m_registers.pc);
    m_registers.pc = 0x0018;
    m_cycles += 16;
}

void CPU::LDH_a8_A() {
    u8 offset = readPC();
    m_memory.write(0xFF00 + offset, m_registers.a);
    m_cycles += 12;
}

void CPU::POP_HL() {
    m_registers.hl = pop();
    m_cycles += 12;
}

void CPU::LD_C_A() {
    m_memory.write(0xFF00 + m_registers.c, m_registers.a);
    m_cycles += 8;
}

void CPU::PUSH_HL() {
    push(m_registers.hl);
    m_cycles += 16;
}

void CPU::AND_n8() {
    m_registers.a &= readPC();
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::RST_20H() {
    push(m_registers.pc);
    m_registers.pc = 0x0020;
    m_cycles += 16;
}

void CPU::ADD_SP_e8() {
    s8 value = static_cast<s8>(readPC());
    u32 result = m_registers.sp + value;
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.sp & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, (m_registers.sp & 0xFF) + (value & 0xFF) > 0xFF);
    
    m_registers.sp = result & 0xFFFF;
    m_cycles += 16;
}

void CPU::JP_HL() {
    m_registers.pc = m_registers.hl;
    m_cycles += 4;
}

void CPU::LD_a16_A() {
    u16 address = readPC16();
    m_memory.write(address, m_registers.a);
    m_cycles += 16;
}

void CPU::XOR_n8() {
    m_registers.a ^= readPC();
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::RST_28H() {
    push(m_registers.pc);
    m_registers.pc = 0x0028;
    m_cycles += 16;
}

void CPU::LDH_A_a8() {
    u8 offset = readPC();
    m_registers.a = m_memory.read(0xFF00 + offset);
    m_cycles += 12;
}

void CPU::POP_AF() {
    m_registers.af = pop() & 0xFFF0;  // Lower 4 bits of F are always 0
    m_cycles += 12;
}

void CPU::LDH_A_C() {
    m_registers.a = m_memory.read(0xFF00 + m_registers.c);
    m_cycles += 8;
}

void CPU::DI() {
    m_interruptsEnabled = false;
    m_cycles += 4;
}

void CPU::PUSH_AF() {
    push(m_registers.af);
    m_cycles += 16;
}

void CPU::OR_n8() {
    m_registers.a |= readPC();
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::RST_30H() {
    push(m_registers.pc);
    m_registers.pc = 0x0030;
    m_cycles += 16;
}

void CPU::LD_HL_SP_e8() {
    s8 value = static_cast<s8>(readPC());
    u32 result = m_registers.sp + value;
    
    setFlag(FLAG_Z, false);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, (m_registers.sp & 0x0F) + (value & 0x0F) > 0x0F);
    setFlag(FLAG_C, (m_registers.sp & 0xFF) + (value & 0xFF) > 0xFF);
    
    m_registers.hl = result & 0xFFFF;
    m_cycles += 12;
}

void CPU::LD_SP_HL() {
    m_registers.sp = m_registers.hl;
    m_cycles += 8;
}

void CPU::LD_A_a16() {
    u16 address = readPC16();
    m_registers.a = m_memory.read(address);
    m_cycles += 16;
}

void CPU::EI() {
    m_pendingInterruptEnable = true;
    m_cycles += 4;
}

void CPU::CP_n8() {
    u8 value = readPC();
    u8 result = m_registers.a - value;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, m_registers.a < value);
    
    m_cycles += 8;
}

void CPU::RST_38H() {
    push(m_registers.pc);
    m_registers.pc = 0x0038;
    m_cycles += 16;
}

// CB Prefixed Instructions

void CPU::RLC_B() {
    bool carry = (m_registers.b & 0x80) != 0;
    m_registers.b = (m_registers.b << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_C() {
    bool carry = (m_registers.c & 0x80) != 0;
    m_registers.c = (m_registers.c << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_D() {
    bool carry = (m_registers.d & 0x80) != 0;
    m_registers.d = (m_registers.d << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_E() {
    bool carry = (m_registers.e & 0x80) != 0;
    m_registers.e = (m_registers.e << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_H() {
    bool carry = (m_registers.h & 0x80) != 0;
    m_registers.h = (m_registers.h << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_L() {
    bool carry = (m_registers.l & 0x80) != 0;
    m_registers.l = (m_registers.l << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RLC_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool carry = (value & 0x80) != 0;
    value = (value << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RLC_A() {
    bool carry = (m_registers.a & 0x80) != 0;
    m_registers.a = (m_registers.a << 1) | (carry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_B() {
    bool carry = (m_registers.b & 0x01) != 0;
    m_registers.b = (m_registers.b >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_C() {
    bool carry = (m_registers.c & 0x01) != 0;
    m_registers.c = (m_registers.c >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_D() {
    bool carry = (m_registers.d & 0x01) != 0;
    m_registers.d = (m_registers.d >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_E() {
    bool carry = (m_registers.e & 0x01) != 0;
    m_registers.e = (m_registers.e >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_H() {
    bool carry = (m_registers.h & 0x01) != 0;
    m_registers.h = (m_registers.h >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_L() {
    bool carry = (m_registers.l & 0x01) != 0;
    m_registers.l = (m_registers.l >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RRC_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool carry = (value & 0x01) != 0;
    value = (value >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RRC_A() {
    bool carry = (m_registers.a & 0x01) != 0;
    m_registers.a = (m_registers.a >> 1) | (carry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::RL_B() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.b & 0x80) != 0;
    m_registers.b = (m_registers.b << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_C() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.c & 0x80) != 0;
    m_registers.c = (m_registers.c << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_D() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.d & 0x80) != 0;
    m_registers.d = (m_registers.d << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_E() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.e & 0x80) != 0;
    m_registers.e = (m_registers.e << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_H() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.h & 0x80) != 0;
    m_registers.h = (m_registers.h << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_L() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.l & 0x80) != 0;
    m_registers.l = (m_registers.l << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RL_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (value & 0x80) != 0;
    value = (value << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RL_A() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.a & 0x80) != 0;
    m_registers.a = (m_registers.a << 1) | (oldCarry ? 1 : 0);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_B() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.b & 0x01) != 0;
    m_registers.b = (m_registers.b >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_C() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.c & 0x01) != 0;
    m_registers.c = (m_registers.c >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_D() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.d & 0x01) != 0;
    m_registers.d = (m_registers.d >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_E() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.e & 0x01) != 0;
    m_registers.e = (m_registers.e >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_H() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.h & 0x01) != 0;
    m_registers.h = (m_registers.h >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_L() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.l & 0x01) != 0;
    m_registers.l = (m_registers.l >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::RR_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (value & 0x01) != 0;
    value = (value >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RR_A() {
    bool oldCarry = getFlag(FLAG_C);
    bool newCarry = (m_registers.a & 0x01) != 0;
    m_registers.a = (m_registers.a >> 1) | (oldCarry ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, newCarry);
    
    m_cycles += 8;
}

void CPU::SLA_B() {
    bool carry = (m_registers.b & 0x80) != 0;
    m_registers.b = m_registers.b << 1;
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_C() {
    bool carry = (m_registers.c & 0x80) != 0;
    m_registers.c = m_registers.c << 1;
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_D() {
    bool carry = (m_registers.d & 0x80) != 0;
    m_registers.d = m_registers.d << 1;
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_E() {
    bool carry = (m_registers.e & 0x80) != 0;
    m_registers.e = m_registers.e << 1;
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_H() {
    bool carry = (m_registers.h & 0x80) != 0;
    m_registers.h = m_registers.h << 1;
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_L() {
    bool carry = (m_registers.l & 0x80) != 0;
    m_registers.l = m_registers.l << 1;
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SLA_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool carry = (value & 0x80) != 0;
    value = value << 1;
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SLA_A() {
    bool carry = (m_registers.a & 0x80) != 0;
    m_registers.a = m_registers.a << 1;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_B() {
    bool carry = (m_registers.b & 0x01) != 0;
    bool msb = (m_registers.b & 0x80) != 0;
    m_registers.b = (m_registers.b >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_C() {
    bool carry = (m_registers.c & 0x01) != 0;
    bool msb = (m_registers.c & 0x80) != 0;
    m_registers.c = (m_registers.c >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_D() {
    bool carry = (m_registers.d & 0x01) != 0;
    bool msb = (m_registers.d & 0x80) != 0;
    m_registers.d = (m_registers.d >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_E() {
    bool carry = (m_registers.e & 0x01) != 0;
    bool msb = (m_registers.e & 0x80) != 0;
    m_registers.e = (m_registers.e >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_H() {
    bool carry = (m_registers.h & 0x01) != 0;
    bool msb = (m_registers.h & 0x80) != 0;
    m_registers.h = (m_registers.h >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_L() {
    bool carry = (m_registers.l & 0x01) != 0;
    bool msb = (m_registers.l & 0x80) != 0;
    m_registers.l = (m_registers.l >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRA_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool carry = (value & 0x01) != 0;
    bool msb = (value & 0x80) != 0;
    value = (value >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SRA_A() {
    bool carry = (m_registers.a & 0x01) != 0;
    bool msb = (m_registers.a & 0x80) != 0;
    m_registers.a = (m_registers.a >> 1) | (msb ? 0x80 : 0);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SWAP_B() {
    m_registers.b = ((m_registers.b & 0x0F) << 4) | ((m_registers.b & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_C() {
    m_registers.c = ((m_registers.c & 0x0F) << 4) | ((m_registers.c & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_D() {
    m_registers.d = ((m_registers.d & 0x0F) << 4) | ((m_registers.d & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_E() {
    m_registers.e = ((m_registers.e & 0x0F) << 4) | ((m_registers.e & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_H() {
    m_registers.h = ((m_registers.h & 0x0F) << 4) | ((m_registers.h & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_L() {
    m_registers.l = ((m_registers.l & 0x0F) << 4) | ((m_registers.l & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SWAP_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SWAP_A() {
    m_registers.a = ((m_registers.a & 0x0F) << 4) | ((m_registers.a & 0xF0) >> 4);
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, false);
    
    m_cycles += 8;
}

void CPU::SRL_B() {
    bool carry = (m_registers.b & 0x01) != 0;
    m_registers.b = m_registers.b >> 1;
    
    setFlag(FLAG_Z, m_registers.b == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_C() {
    bool carry = (m_registers.c & 0x01) != 0;
    m_registers.c = m_registers.c >> 1;
    
    setFlag(FLAG_Z, m_registers.c == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_D() {
    bool carry = (m_registers.d & 0x01) != 0;
    m_registers.d = m_registers.d >> 1;
    
    setFlag(FLAG_Z, m_registers.d == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_E() {
    bool carry = (m_registers.e & 0x01) != 0;
    m_registers.e = m_registers.e >> 1;
    
    setFlag(FLAG_Z, m_registers.e == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_H() {
    bool carry = (m_registers.h & 0x01) != 0;
    m_registers.h = m_registers.h >> 1;
    
    setFlag(FLAG_Z, m_registers.h == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_L() {
    bool carry = (m_registers.l & 0x01) != 0;
    m_registers.l = m_registers.l >> 1;
    
    setFlag(FLAG_Z, m_registers.l == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::SRL_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    bool carry = (value & 0x01) != 0;
    value = value >> 1;
    
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SRL_A() {
    bool carry = (m_registers.a & 0x01) != 0;
    m_registers.a = m_registers.a >> 1;
    
    setFlag(FLAG_Z, m_registers.a == 0);
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, carry);
    
    m_cycles += 8;
}

void CPU::BIT_0_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_0_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_0_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x01));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_1_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_1_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x02));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_2_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_2_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x04));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_3_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_3_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x08));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_4_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_4_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x10));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_5_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_5_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x20));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_6_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_6_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x40));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_B() {
    setFlag(FLAG_Z, !(m_registers.b & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_C() {
    setFlag(FLAG_Z, !(m_registers.c & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_D() {
    setFlag(FLAG_Z, !(m_registers.d & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_E() {
    setFlag(FLAG_Z, !(m_registers.e & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_H() {
    setFlag(FLAG_Z, !(m_registers.h & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_L() {
    setFlag(FLAG_Z, !(m_registers.l & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::BIT_7_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    setFlag(FLAG_Z, !(value & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 12;
}

void CPU::BIT_7_A() {
    setFlag(FLAG_Z, !(m_registers.a & 0x80));
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, true);
    
    m_cycles += 8;
}

void CPU::RES_0_B() {
    m_registers.b &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_C() {
    m_registers.c &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_D() {
    m_registers.d &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_E() {
    m_registers.e &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_H() {
    m_registers.h &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_L() {
    m_registers.l &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_0_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x01;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_0_A() {
    m_registers.a &= ~0x01;
    m_cycles += 8;
}

void CPU::RES_1_B() {
    m_registers.b &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_C() {
    m_registers.c &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_D() {
    m_registers.d &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_E() {
    m_registers.e &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_H() {
    m_registers.h &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_L() {
    m_registers.l &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_1_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x02;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_1_A() {
    m_registers.a &= ~0x02;
    m_cycles += 8;
}

void CPU::RES_2_B() {
    m_registers.b &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_C() {
    m_registers.c &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_D() {
    m_registers.d &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_E() {
    m_registers.e &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_H() {
    m_registers.h &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_L() {
    m_registers.l &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_2_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x04;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_2_A() {
    m_registers.a &= ~0x04;
    m_cycles += 8;
}

void CPU::RES_3_B() {
    m_registers.b &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_C() {
    m_registers.c &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_D() {
    m_registers.d &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_E() {
    m_registers.e &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_H() {
    m_registers.h &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_L() {
    m_registers.l &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_3_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x08;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_3_A() {
    m_registers.a &= ~0x08;
    m_cycles += 8;
}

void CPU::RES_4_B() {
    m_registers.b &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_C() {
    m_registers.c &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_D() {
    m_registers.d &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_E() {
    m_registers.e &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_H() {
    m_registers.h &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_L() {
    m_registers.l &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_4_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x10;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_4_A() {
    m_registers.a &= ~0x10;
    m_cycles += 8;
}

void CPU::RES_5_B() {
    m_registers.b &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_C() {
    m_registers.c &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_D() {
    m_registers.d &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_E() {
    m_registers.e &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_H() {
    m_registers.h &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_L() {
    m_registers.l &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_5_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x20;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_5_A() {
    m_registers.a &= ~0x20;
    m_cycles += 8;
}

void CPU::RES_6_B() {
    m_registers.b &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_C() {
    m_registers.c &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_D() {
    m_registers.d &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_E() {
    m_registers.e &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_H() {
    m_registers.h &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_L() {
    m_registers.l &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_6_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x40;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_6_A() {
    m_registers.a &= ~0x40;
    m_cycles += 8;
}

void CPU::RES_7_B() {
    m_registers.b &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_C() {
    m_registers.c &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_D() {
    m_registers.d &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_E() {
    m_registers.e &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_H() {
    m_registers.h &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_L() {
    m_registers.l &= ~0x80;
    m_cycles += 8;
}

void CPU::RES_7_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value &= ~0x80;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::RES_7_A() {
    m_registers.a &= ~0x80;
    m_cycles += 8;
}

void CPU::SET_0_B() {
    m_registers.b |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_C() {
    m_registers.c |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_D() {
    m_registers.d |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_E() {
    m_registers.e |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_H() {
    m_registers.h |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_L() {
    m_registers.l |= 0x01;
    m_cycles += 8;
}

void CPU::SET_0_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x01;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_0_A() {
    m_registers.a |= 0x01;
    m_cycles += 8;
}

void CPU::SET_1_B() {
    m_registers.b |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_C() {
    m_registers.c |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_D() {
    m_registers.d |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_E() {
    m_registers.e |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_H() {
    m_registers.h |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_L() {
    m_registers.l |= 0x02;
    m_cycles += 8;
}

void CPU::SET_1_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x02;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_1_A() {
    m_registers.a |= 0x02;
    m_cycles += 8;
}

void CPU::SET_2_B() {
    m_registers.b |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_C() {
    m_registers.c |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_D() {
    m_registers.d |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_E() {
    m_registers.e |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_H() {
    m_registers.h |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_L() {
    m_registers.l |= 0x04;
    m_cycles += 8;
}

void CPU::SET_2_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x04;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_2_A() {
    m_registers.a |= 0x04;
    m_cycles += 8;
}

void CPU::SET_3_B() {
    m_registers.b |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_C() {
    m_registers.c |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_D() {
    m_registers.d |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_E() {
    m_registers.e |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_H() {
    m_registers.h |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_L() {
    m_registers.l |= 0x08;
    m_cycles += 8;
}

void CPU::SET_3_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x08;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_3_A() {
    m_registers.a |= 0x08;
    m_cycles += 8;
}

void CPU::SET_4_B() {
    m_registers.b |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_C() {
    m_registers.c |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_D() {
    m_registers.d |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_E() {
    m_registers.e |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_H() {
    m_registers.h |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_L() {
    m_registers.l |= 0x10;
    m_cycles += 8;
}

void CPU::SET_4_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x10;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_4_A() {
    m_registers.a |= 0x10;
    m_cycles += 8;
}

void CPU::SET_5_B() {
    m_registers.b |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_C() {
    m_registers.c |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_D() {
    m_registers.d |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_E() {
    m_registers.e |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_H() {
    m_registers.h |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_L() {
    m_registers.l |= 0x20;
    m_cycles += 8;
}

void CPU::SET_5_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x20;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_5_A() {
    m_registers.a |= 0x20;
    m_cycles += 8;
}

void CPU::SET_6_B() {
    m_registers.b |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_C() {
    m_registers.c |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_D() {
    m_registers.d |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_E() {
    m_registers.e |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_H() {
    m_registers.h |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_L() {
    m_registers.l |= 0x40;
    m_cycles += 8;
}

void CPU::SET_6_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x40;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_6_A() {
    m_registers.a |= 0x40;
    m_cycles += 8;
}

void CPU::SET_7_B() {
    m_registers.b |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_C() {
    m_registers.c |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_D() {
    m_registers.d |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_E() {
    m_registers.e |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_H() {
    m_registers.h |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_L() {
    m_registers.l |= 0x80;
    m_cycles += 8;
}

void CPU::SET_7_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    value |= 0x80;
    m_memory.write(m_registers.hl, value);
    m_cycles += 16;
}

void CPU::SET_7_A() {
    m_registers.a |= 0x80;
    m_cycles += 8;
}