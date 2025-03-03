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
        std::cerr << "Unknown CB-prefixed mnemonic: " << mnemonic << std::endl;
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


