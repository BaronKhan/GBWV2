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

    const auto currentPC = m_registers.pc;
    
    // If CPU is halted or stopped, don't execute instructions
    if (m_halted || m_stopped) {
        m_cycles += 4;
        return;
    }
    
    // Fetch opcode
    u8 opcode = m_memory.read(m_registers.pc++);
    
    // Debug output
    if (currentPC >= 0x69 && currentPC <= 0x6E) {
        const auto LY = m_memory.read(0xFF44);
        std::cout << "PC: 0x" << std::hex << (m_registers.pc - 1) 
                  << ", Opcode: 0x" << std::hex << static_cast<int>(opcode)
                  << ", A: 0x" << std::hex << static_cast<int>(m_registers.a)
                  << ", C: 0x" << std::hex << static_cast<int>(m_registers.c)
                  << ", LY: 0x" << std::hex << static_cast<int>(LY)
                  << ", " << m_opcodeTable[opcode].mnemonic
                  << std::endl;
    }

    if (currentPC >= 0x2700 && currentPC <= 0x27FF) {
        std::cout << "PC: 0x" << std::hex << (currentPC) 
                  << ", Opcode: 0x" << std::hex << static_cast<int>(opcode)
                  << ", A: 0x" << std::hex << static_cast<int>(m_registers.a)
                  << ", B: 0x" << std::hex << static_cast<int>(m_registers.b)
                  << ", " << m_opcodeTable[opcode].mnemonic
                  << std::endl;
    }
    
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
                auto name = operand["name"].get<std::string>();
                if (operand.contains("increment") && operand["increment"].get<bool>()) {
                    name += "+";
                }
                else if (operand.contains("decrement") && operand["decrement"].get<bool>()) {
                    name += "-";
                }
                if (name == "HL" && operand.contains("immediate") && !operand["immediate"].get<bool>()) {
                    name += "m";
                }
                operands.push_back(name);
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
                auto name = operand["name"].get<std::string>();
                if (operand.contains("increment") && operand["increment"].get<bool>()) {
                    name += "+";
                }
                else if (operand.contains("decrement") && operand["decrement"].get<bool>()) {
                    name += "-";
                }
                if (name == "HL" && operand.contains("immediate") && !operand["immediate"].get<bool>()) {
                    name += "m";
                }
                operands.push_back(name);
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

    static const std::unordered_map<std::string, OpcodeFunction> mnemonicToFunctionMapping = {
        // unprefixed
        {"NOP", [this]() { NOP(); }},
        {"LD BC,n16", [this]() { LD_BC_n16(); }},
        {"LD BC,A", [this]() { LD_BC_A(); }},
        {"INC BC", [this]() { INC_BC(); }},
        {"INC B", [this]() { INC_B(); }},
        {"DEC B", [this]() { DEC_B(); }},
        {"LD B,n8", [this]() { LD_B_n8(); }},
        {"RLCA", [this]() { RLCA(); }},
        {"LD a16,SP", [this]() { LD_a16_SP(); }},
        {"ADD HL,BC", [this]() { ADD_HL_BC(); }},
        {"LD A,BC", [this]() { LD_A_BC(); }},
        {"DEC BC", [this]() { DEC_BC(); }},
        {"INC C", [this]() { INC_C(); }},
        {"DEC C", [this]() { DEC_C(); }},
        {"LD C,n8", [this]() { LD_C_n8(); }},
        {"RRCA", [this]() { RRCA(); }},
        {"STOP n8", [this]() { STOP_n8(); }},
        {"LD DE,n16", [this]() { LD_DE_n16(); }},
        {"LD DE,A", [this]() { LD_DE_A(); }},
        {"INC DE", [this]() { INC_DE(); }},
        {"INC D", [this]() { INC_D(); }},
        {"DEC D", [this]() { DEC_D(); }},
        {"LD D,n8", [this]() { LD_D_n8(); }},
        {"RLA", [this]() { RLA(); }},
        {"JR e8", [this]() { JR_e8(); }},
        {"ADD HL,DE", [this]() { ADD_HL_DE(); }},
        {"LD A,DE", [this]() { LD_A_DE(); }},
        {"DEC DE", [this]() { DEC_DE(); }},
        {"INC E", [this]() { INC_E(); }},
        {"DEC E", [this]() { DEC_E(); }},
        {"LD E,n8", [this]() { LD_E_n8(); }},
        {"RRA", [this]() { RRA(); }},
        {"JR NZ,e8", [this]() { JR_NZ_e8(); }},
        {"LD HL,n16", [this]() { LD_HL_n16(); }},
        {"LD HL+,A", [this]() {LD_HLI_A(); }},
        {"INC HL", [this]() { INC_HL(); }},
        {"INC H", [this]() { INC_H(); }},
        {"DEC H", [this]() { DEC_H(); }},
        {"LD H,n8", [this]() { LD_H_n8(); }},
        {"DAA", [this]() { DAA(); }},
        {"JR Z,e8", [this]() { JR_Z_e8(); }},
        {"ADD HL,HL", [this]() { ADD_HL_HL(); }},
        {"LD A,HL+", [this]() { LD_A_HLI(); }},
        {"DEC HL", [this]() { DEC_HL(); }},
        {"INC L", [this]() { INC_L(); }},
        {"DEC L", [this]() { DEC_L(); }},
        {"LD L,n8", [this]() { LD_L_n8(); }},
        {"CPL", [this]() { CPL(); }},
        {"JR NC,e8", [this]() { JR_NC_e8(); }},
        {"LD SP,n16", [this]() { LD_SP_n16(); }},
        {"LD HL-,A", [this]() { LD_HLD_A(); }},
        {"INC SP", [this]() { INC_SP(); }},
        {"INC HLm", [this]() { INC_HLm(); }},
        {"DEC HLm", [this]() { DEC_HLm(); }},
        {"LD HLm,n8", [this]() { LD_HLm_n8(); }},
        {"SCF", [this]() { SCF(); }},
        {"JR C,e8", [this]() { JR_C_e8(); }},
        {"ADD HL,SP", [this]() { ADD_HL_SP(); }},
        {"LD A,HL-", [this]() { LD_A_HLD(); }},
        {"DEC SP", [this]() { DEC_SP(); }},
        {"INC A", [this]() { INC_A(); }},
        {"DEC A", [this]() { DEC_A(); }},
        {"LD A,n8", [this]() { LD_A_n8(); }},
        {"CCF", [this]() { CCF(); }},
        {"LD B,B", [this]() { LD_B_B(); }},
        {"LD B,C", [this]() { LD_B_C(); }},
        {"LD B,D", [this]() { LD_B_D(); }},
        {"LD B,E", [this]() { LD_B_E(); }},
        {"LD B,H", [this]() { LD_B_H(); }},
        {"LD B,L", [this]() { LD_B_L(); }},
        {"LD B,HLm", [this]() { LD_B_HLm(); }},
        {"LD B,A", [this]() { LD_B_A(); }},
        {"LD C,B", [this]() { LD_C_B(); }},
        {"LD C,C", [this]() { LD_C_C(); }},
        {"LD C,D", [this]() { LD_C_D(); }},
        {"LD C,E", [this]() { LD_C_E(); }},
        {"LD C,H", [this]() { LD_C_H(); }},
        {"LD C,L", [this]() { LD_C_L(); }},
        {"LD C,HLm", [this]() { LD_C_HLm(); }},
        {"LD C,A", [this]() { LD_C_A(); }},
        {"LD D,B", [this]() { LD_D_B(); }},
        {"LD D,C", [this]() { LD_D_C(); }},
        {"LD D,D", [this]() { LD_D_D(); }},
        {"LD D,E", [this]() { LD_D_E(); }},
        {"LD D,H", [this]() { LD_D_H(); }},
        {"LD D,L", [this]() { LD_D_L(); }},
        {"LD D,HLm", [this]() { LD_D_HLm(); }},
        {"LD D,A", [this]() { LD_D_A(); }},
        {"LD E,B", [this]() { LD_E_B(); }},
        {"LD E,C", [this]() { LD_E_C(); }},
        {"LD E,D", [this]() { LD_E_D(); }},
        {"LD E,E", [this]() { LD_E_E(); }},
        {"LD E,H", [this]() { LD_E_H(); }},
        {"LD E,L", [this]() { LD_E_L(); }},
        {"LD E,HLm", [this]() { LD_E_HLm(); }},
        {"LD E,A", [this]() { LD_E_A(); }},
        {"LD H,B", [this]() { LD_H_B(); }},
        {"LD H,C", [this]() { LD_H_C(); }},
        {"LD H,D", [this]() { LD_H_D(); }},
        {"LD H,E", [this]() { LD_H_E(); }},
        {"LD H,H", [this]() { LD_H_H(); }},
        {"LD H,L", [this]() { LD_H_L(); }},
        {"LD H,HLm", [this]() { LD_H_HLm(); }},
        {"LD H,A", [this]() { LD_H_A(); }},
        {"LD L,B", [this]() { LD_L_B(); }},
        {"LD L,C", [this]() { LD_L_C(); }},
        {"LD L,D", [this]() { LD_L_D(); }},
        {"LD L,E", [this]() { LD_L_E(); }},
        {"LD L,H", [this]() { LD_L_H(); }},
        {"LD L,L", [this]() { LD_L_L(); }},
        {"LD L,HLm", [this]() { LD_L_HLm(); }},
        {"LD L,A", [this]() { LD_L_A(); }},
        {"LD HLm,B", [this]() { LD_HLm_B(); }},
        {"LD HLm,C", [this]() { LD_HLm_C(); }},
        {"LD HLm,D", [this]() { LD_HLm_D(); }},
        {"LD HLm,E", [this]() { LD_HLm_E(); }},
        {"LD HLm,H", [this]() { LD_HLm_H(); }},
        {"LD HLm,L", [this]() { LD_HLm_L(); }},
        {"HALT", [this]() { HALT(); }},
        {"LD HLm,A", [this]() { LD_HLm_A(); }},
        {"LD A,B", [this]() { LD_A_B(); }},
        {"LD A,C", [this]() { LD_A_C(); }},
        {"LD A,D", [this]() { LD_A_D(); }},
        {"LD A,E", [this]() { LD_A_E(); }},
        {"LD A,H", [this]() { LD_A_H(); }},
        {"LD A,L", [this]() { LD_A_L(); }},
        {"LD A,HLm", [this]() { LD_A_HLm(); }},
        {"LD A,A", [this]() { LD_A_A(); }},
        {"ADD A,B", [this]() { ADD_A_B(); }},
        {"ADD A,C", [this]() { ADD_A_C(); }},
        {"ADD A,D", [this]() { ADD_A_D(); }},
        {"ADD A,E", [this]() { ADD_A_E(); }},
        {"ADD A,H", [this]() { ADD_A_H(); }},
        {"ADD A,L", [this]() { ADD_A_L(); }},
        {"ADD A,HLm", [this]() { ADD_A_HLm(); }},
        {"ADD A,A", [this]() { ADD_A_A(); }},
        {"ADC A,B", [this]() { ADC_A_B(); }},
        {"ADC A,C", [this]() { ADC_A_C(); }},
        {"ADC A,D", [this]() { ADC_A_D(); }},
        {"ADC A,E", [this]() { ADC_A_E(); }},
        {"ADC A,H", [this]() { ADC_A_H(); }},
        {"ADC A,L", [this]() { ADC_A_L(); }},
        {"ADC A,HLm", [this]() { ADC_A_HLm(); }},
        {"ADC A,A", [this]() { ADC_A_A(); }},
        {"SUB A,B", [this]() { SUB_A_B(); }},
        {"SUB A,C", [this]() { SUB_A_C(); }},
        {"SUB A,D", [this]() { SUB_A_D(); }},
        {"SUB A,E", [this]() { SUB_A_E(); }},
        {"SUB A,H", [this]() { SUB_A_H(); }},
        {"SUB A,L", [this]() { SUB_A_L(); }},
        {"SUB A,HLm", [this]() { SUB_A_HLm(); }},
        {"SUB A,A", [this]() { SUB_A_A(); }},
        {"SBC A,B", [this]() { SBC_A_B(); }},
        {"SBC A,C", [this]() { SBC_A_C(); }},
        {"SBC A,D", [this]() { SBC_A_D(); }},
        {"SBC A,E", [this]() { SBC_A_E(); }},
        {"SBC A,H", [this]() { SBC_A_H(); }},
        {"SBC A,L", [this]() { SBC_A_L(); }},
        {"SBC A,HLm", [this]() { SBC_A_HLm(); }},
        {"SBC A,A", [this]() { SBC_A_A(); }},
        {"AND A,B", [this]() { AND_B(); }},
        {"AND A,C", [this]() { AND_C(); }},
        {"AND A,D", [this]() { AND_D(); }},
        {"AND A,E", [this]() { AND_E(); }},
        {"AND A,H", [this]() { AND_H(); }},
        {"AND A,L", [this]() { AND_L(); }},
        {"AND A,HLm", [this]() { AND_HLm(); }},
        {"AND A,A", [this]() { AND_A(); }},
        {"XOR A,B", [this]() { XOR_B(); }},
        {"XOR A,C", [this]() { XOR_C(); }},
        {"XOR A,D", [this]() { XOR_D(); }},
        {"XOR A,E", [this]() { XOR_E(); }},
        {"XOR A,H", [this]() { XOR_H(); }},
        {"XOR A,L", [this]() { XOR_L(); }},
        {"XOR A,HLm", [this]() { XOR_HLm(); }},
        {"XOR A,A", [this]() { XOR_A(); }},
        {"OR A,B", [this]() { OR_B(); }},
        {"OR A,C", [this]() { OR_C(); }},
        {"OR A,D", [this]() { OR_D(); }},
        {"OR A,E", [this]() { OR_E(); }},
        {"OR A,H", [this]() { OR_H(); }},
        {"OR A,L", [this]() { OR_L(); }},
        {"OR A,HLm", [this]() { OR_HLm(); }},
        {"OR A,A", [this]() { OR_A(); }},
        {"CP A,B", [this]() { CP_B(); }},
        {"CP A,C", [this]() { CP_C(); }},
        {"CP A,D", [this]() { CP_D(); }},
        {"CP A,E", [this]() { CP_E(); }},
        {"CP A,H", [this]() { CP_H(); }},
        {"CP A,L", [this]() { CP_L(); }},
        {"CP A,HLm", [this]() { CP_HLm(); }},
        {"CP A,A", [this]() { CP_A(); }},
        {"RET NZ", [this]() { RET_NZ(); }},
        {"POP BC", [this]() { POP_BC(); }},
        {"JP NZ,a16", [this]() { JP_NZ_a16(); }},
        {"JP a16", [this]() { JP_a16(); }},
        {"CALL NZ,a16", [this]() { CALL_NZ_a16(); }},
        {"PUSH BC", [this]() { PUSH_BC(); }},
        {"ADD A,n8", [this]() { ADD_A_n8(); }},
        {"RST $00", [this]() { RST_00H(); }},
        {"RET Z", [this]() { RET_Z(); }},
        {"RET", [this]() { RET(); }},
        {"JP Z,a16", [this]() { JP_Z_a16(); }},
        {"PREFIX", [this]() { PREFIX_CB(); }},
        {"CALL Z,a16", [this]() { CALL_Z_a16(); }},
        {"CALL a16", [this]() { CALL_a16(); }},
        {"ADC A,n8", [this]() { ADC_A_n8(); }},
        {"RST $08", [this]() { RST_08H(); }},
        {"RET NC", [this]() { RET_NC(); }},
        {"POP DE", [this]() { POP_DE(); }},
        {"JP NC,a16", [this]() { JP_NC_a16(); }},
        {"CALL NC,a16", [this]() { CALL_NC_a16(); }},
        {"PUSH DE", [this]() { PUSH_DE(); }},
        {"SUB A,n8", [this]() { SUB_n8(); }},
        {"RST $10", [this]() { RST_10H(); }},
        {"RET C", [this]() { RET_C(); }},
        {"RETI", [this]() { RETI(); }},
        {"JP C,a16", [this]() { JP_C_a16(); }},
        {"CALL C,a16", [this]() { CALL_C_a16(); }},
        {"SBC A,n8", [this]() { SBC_A_n8(); }},
        {"RST $18", [this]() { RST_18H(); }},
        {"LDH a8,A", [this]() { LDH_a8_A(); }},
        {"POP HL", [this]() { POP_HL(); }},
        {"LDH C,A", [this]() { LDH_C_A(); }},
        {"PUSH HL", [this]() { PUSH_HL(); }},
        {"AND A,n8", [this]() { AND_n8(); }},
        {"RST $20", [this]() { RST_20H(); }},
        {"ADD SP,e8", [this]() { ADD_SP_e8();}},
        {"JP HL", [this]() { JP_HL(); }},
        {"LD a16,A", [this]() { LD_a16_A(); }},
        {"XOR A,n8", [this]() { XOR_n8(); }},
        {"RST $28", [this]() { RST_28H(); }},
        {"LDH A,a8", [this]() { LDH_A_a8(); }},
        {"POP AF", [this]() { POP_AF(); }},
        {"LDH A,C", [this]() { LDH_A_C(); }},
        {"DI", [this]() { DI(); }},
        {"PUSH AF", [this]() { PUSH_AF(); }},
        {"OR A,n8", [this]() { OR_n8(); }},
        {"RST $30", [this]() { RST_30H(); }},
        {"LD HL,SP+,e8", [this]() { LD_HL_SP_e8(); }},
        {"LD SP,HL", [this]() { LD_SP_HL(); }},
        {"LD A,a16", [this]() { LD_A_a16(); }},
        {"EI", [this]() { EI(); }},
        {"CP A,n8", [this]() { CP_n8(); }},
        {"RST $38", [this]() { RST_38H(); }},

        // CB-prefixed
        {"RLC B", [this]() { RLC_B(); }},
        {"RLC C", [this]() { RLC_C(); }},
        {"RLC D", [this]() { RLC_D(); }},
        {"RLC E", [this]() { RLC_E(); }},
        {"RLC H", [this]() { RLC_H(); }},
        {"RLC L", [this]() { RLC_L(); }},
        {"RLC HLm", [this]() { RLC_HLm(); }},
        {"RLC A", [this]() { RLC_A(); }},
        {"RRC B", [this]() { RRC_B(); }},
        {"RRC C", [this]() { RRC_C(); }},
        {"RRC D", [this]() { RRC_D(); }},
        {"RRC E", [this]() { RRC_E(); }},
        {"RRC H", [this]() { RRC_H(); }},
        {"RRC L", [this]() { RRC_L(); }},
        {"RRC HLm", [this]() { RRC_HLm(); }},
        {"RRC A", [this]() { RRC_A(); }},
        {"RL B", [this]() { RL_B(); }},
        {"RL C", [this]() { RL_C(); }},
        {"RL D", [this]() { RL_D(); }},
        {"RL E", [this]() { RL_E(); }},
        {"RL H", [this]() { RL_H(); }},
        {"RL L", [this]() { RL_L(); }},
        {"RL HLm", [this]() { RL_HLm(); }},
        {"RL A", [this]() { RL_A(); }},
        {"RR B", [this]() { RR_B(); }},
        {"RR C", [this]() { RR_C(); }},
        {"RR D", [this]() { RR_D(); }},
        {"RR E", [this]() { RR_E(); }},
        {"RR H", [this]() { RR_H(); }},
        {"RR L", [this]() { RR_L(); }},
        {"RR HLm", [this]() { RR_HLm(); }},
        {"RR A", [this]() { RR_A(); }},
        {"SLA B", [this]() { SLA_B(); }},
        {"SLA C", [this]() { SLA_C(); }},
        {"SLA D", [this]() { SLA_D();}},
        {"SLA E", [this]() { SLA_E(); }},
        {"SLA H", [this]() { SLA_H(); }},
        {"SLA L", [this]() { SLA_L(); }},
        {"SLA HLm", [this]() { SLA_HLm(); }},
        {"SLA A", [this]() { SLA_A(); }},
        {"SRA B", [this]() { SRA_B(); }},
        {"SRA C", [this]() { SRA_C(); }},
        {"SRA D", [this]() { SRA_D(); }},
        {"SRA E", [this]() { SRA_E(); }},
        {"SRA H", [this]() { SRA_H(); }},
        {"SRA L", [this]() { SRA_L(); }},
        {"SRA HLm", [this]() { SRA_HLm(); }},
        {"SRA A", [this]() { SRA_A(); }},
        {"SWAP B", [this]() { SWAP_B(); }},
        {"SWAP C", [this]() { SWAP_C(); }},
        {"SWAP D", [this]() { SWAP_D(); }},
        {"SWAP E", [this]() { SWAP_E(); }},
        {"SWAP H", [this]() { SWAP_H(); }},
        {"SWAP L", [this]() { SWAP_L(); }},
        {"SWAP HLm", [this]() { SWAP_HLm(); }},
        {"SWAP A", [this]() { SWAP_A(); }},
        {"SRL B", [this]() { SRL_B(); }},
        {"SRL C", [this]() { SRL_C(); }},
        {"SRL D", [this]() { SRL_D(); }},
        {"SRL E", [this]() { SRL_E(); }},
        {"SRL H", [this]() { SRL_H(); }},
        {"SRL L", [this]() { SRL_L(); }},
        {"SRL HLm", [this]() { SRL_HLm(); }},
        {"SRL A", [this]() { SRL_A(); }},
        {"BIT 0,B", [this]() { BIT_0_B(); }},
        {"BIT 0,C", [this]() { BIT_0_C(); }},
        {"BIT 0,D", [this]() { BIT_0_D(); }},
        {"BIT 0,E", [this]() { BIT_0_E(); }},
        {"BIT 0,H", [this]() { BIT_0_H(); }},
        {"BIT 0,L", [this]() { BIT_0_L(); }},
        {"BIT 0,HLm", [this]() { BIT_0_HLm(); }},
        {"BIT 0,A", [this]() { BIT_0_A(); }},
        {"BIT 1,B", [this]() { BIT_1_B(); }},
        {"BIT 1,C", [this]() { BIT_1_C(); }},
        {"BIT 1,D", [this]() { BIT_1_D(); }},
        {"BIT 1,E", [this]() { BIT_1_E(); }},
        {"BIT 1,H", [this]() { BIT_1_H(); }},
        {"BIT 1,L", [this]() { BIT_1_L(); }},
        {"BIT 1,HLm", [this]() { BIT_1_HLm(); }},
        {"BIT 1,A", [this]() { BIT_1_A(); }},
        {"BIT 2,B", [this]() { BIT_2_B(); }},
        {"BIT 2,C", [this]() { BIT_2_C(); }},
        {"BIT 2,D", [this]() { BIT_2_D(); }},
        {"BIT 2,E", [this]() { BIT_2_E(); }},
        {"BIT 2,H", [this]() { BIT_2_H(); }},
        {"BIT 2,L", [this]() { BIT_2_L(); }},
        {"BIT 2,HLm", [this]() { BIT_2_HLm(); }},
        {"BIT 2,A", [this]() { BIT_2_A(); }},
        {"BIT 3,B", [this]() { BIT_3_B(); }},
        {"BIT 3,C", [this]() { BIT_3_C(); }},
        {"BIT 3,D", [this]() { BIT_3_D(); }},
        {"BIT 3,E", [this]() { BIT_3_E(); }},
        {"BIT 3,H", [this]() { BIT_3_H(); }},
        {"BIT 3,L", [this]() { BIT_3_L(); }},
        {"BIT 3,HLm", [this]() { BIT_3_HLm(); }},
        {"BIT 3,A", [this]() { BIT_3_A(); }},
        {"BIT 4,B", [this]() { BIT_4_B(); }},
        {"BIT 4,C", [this]() { BIT_4_C(); }},
        {"BIT 4,D", [this]() { BIT_4_D(); }},
        {"BIT 4,E", [this]() { BIT_4_E(); }},
        {"BIT 4,H", [this]() { BIT_4_H(); }},
        {"BIT 4,L", [this]() { BIT_4_L(); }},
        {"BIT 4,HLm", [this]() { BIT_4_HLm(); }},
        {"BIT 4,A", [this]() { BIT_4_A(); }},
        {"BIT 5,B", [this]() { BIT_5_B(); }},
        {"BIT 5,C", [this]() { BIT_5_C(); }},
        {"BIT 5,D", [this]() { BIT_5_D(); }},
        {"BIT 5,E", [this]() { BIT_5_E(); }},
        {"BIT 5,H", [this]() { BIT_5_H(); }},
        {"BIT 5,L", [this]() { BIT_5_L(); }},
        {"BIT 5,HLm", [this]() { BIT_5_HLm(); }},
        {"BIT 5,A", [this]() { BIT_5_A(); }},
        {"BIT 6,B", [this]() { BIT_6_B(); }},
        {"BIT 6,C", [this]() { BIT_6_C(); }},
        {"BIT 6,D", [this]() { BIT_6_D(); }},
        {"BIT 6,E", [this]() { BIT_6_E(); }},
        {"BIT 6,H", [this]() { BIT_6_H(); }},
        {"BIT 6,L", [this]() { BIT_6_L(); }},
        {"BIT 6,HLm", [this]() { BIT_6_HLm(); }},
        {"BIT 6,A", [this]() { BIT_6_A(); }},
        {"BIT 7,B", [this]() { BIT_7_B(); }},
        {"BIT 7,C", [this]() { BIT_7_C(); }},
        {"BIT 7,D", [this]() { BIT_7_D(); }},
        {"BIT 7,E", [this]() { BIT_7_E(); }},
        {"BIT 7,H", [this]() { BIT_7_H(); }},
        {"BIT 7,L", [this]() { BIT_7_L(); }},
        {"BIT 7,HLm", [this]() { BIT_7_HLm(); }},
        {"BIT 7,A", [this]() { BIT_7_A(); }},
        {"RES 0,B", [this]() { RES_0_B(); }},
        {"RES 0,C", [this]() { RES_0_C(); }},
        {"RES 0,D", [this]() { RES_0_D(); }},
        {"RES 0,E", [this]() { RES_0_E(); }},
        {"RES 0,H", [this]() { RES_0_H(); }},
        {"RES 0,L", [this]() { RES_0_L(); }},
        {"RES 0,HLm", [this]() { RES_0_HLm(); }},
        {"RES 0,A", [this]() { RES_0_A(); }},
        {"RES 1,B", [this]() { RES_1_B(); }},
        {"RES 1,C", [this]() { RES_1_C(); }},
        {"RES 1,D", [this]() { RES_1_D(); }},
        {"RES 1,E", [this]() { RES_1_E(); }},
        {"RES 1,H", [this]() { RES_1_H(); }},
        {"RES 1,L", [this]() { RES_1_L(); }},
        {"RES 1,HLm", [this]() { RES_1_HLm(); }},
        {"RES 1,A", [this]() { RES_1_A(); }},
        {"RES 2,B", [this]() { RES_2_B(); }},
        {"RES 2,C", [this]() { RES_2_C(); }},
        {"RES 2,D", [this]() { RES_2_D(); }},
        {"RES 2,E", [this]() { RES_2_E(); }},
        {"RES 2,H", [this]() { RES_2_H(); }},
        {"RES 2,L", [this]() { RES_2_L(); }},
        {"RES 2,HLm", [this]() { RES_2_HLm(); }},
        {"RES 2,A", [this]() { RES_2_A(); }},
        {"RES 3,B", [this]() { RES_3_B(); }},
        {"RES 3,C", [this]() { RES_3_C(); }},
        {"RES 3,D", [this]() { RES_3_D(); }},
        {"RES 3,E", [this]() { RES_3_E(); }},
        {"RES 3,H", [this]() { RES_3_H(); }},
        {"RES 3,L", [this]() { RES_3_L(); }},
        {"RES 3,HLm", [this]() { RES_3_HLm(); }},
        {"RES 3,A", [this]() { RES_3_A(); }},
        {"RES 4,B", [this]() { RES_4_B(); }},
        {"RES 4,C", [this]() { RES_4_C(); }},
        {"RES 4,D", [this]() { RES_4_D(); }},
        {"RES 4,E", [this]() { RES_4_E(); }},
        {"RES 4,H", [this]() { RES_4_H(); }},
        {"RES 4,L", [this]() { RES_4_L(); }},
        {"RES 4,HLm", [this]() { RES_4_HLm(); }},
        {"RES 4,A", [this]() { RES_4_A(); }},
        {"RES 5,B", [this]() { RES_5_B(); }},
        {"RES 5,C", [this]() { RES_5_C(); }},
        {"RES 5,D", [this]() { RES_5_D(); }},
        {"RES 5,E", [this]() { RES_5_E(); }},
        {"RES 5,H", [this]() { RES_5_H(); }},
        {"RES 5,L", [this]() { RES_5_L(); }},
        {"RES 5,HLm", [this]() { RES_5_HLm(); }},
        {"RES 5,A", [this]() { RES_5_A(); }},
        {"RES 6,B", [this]() { RES_6_B(); }},
        {"RES 6,C", [this]() { RES_6_C(); }},
        {"RES 6,D", [this]() { RES_6_D(); }},
        {"RES 6,E", [this]() { RES_6_E(); }},
        {"RES 6,H", [this]() { RES_6_H(); }},
        {"RES 6,L", [this]() { RES_6_L(); }},
        {"RES 6,HLm", [this]() { RES_6_HLm(); }},
        {"RES 6,A", [this]() { RES_6_A(); }},
        {"RES 7,B", [this]() { RES_7_B(); }},
        {"RES 7,C", [this]() { RES_7_C(); }},
        {"RES 7,D", [this]() { RES_7_D(); }},
        {"RES 7,E", [this]() { RES_7_E(); }},
        {"RES 7,H", [this]() { RES_7_H(); }},
        {"RES 7,L", [this]() { RES_7_L(); }},
        {"RES 7,HLm", [this]() { RES_7_HLm(); }},
        {"RES 7,A", [this]() { RES_7_A(); }},
        {"SET 0,B", [this]() { SET_0_B(); }},
        {"SET 0,C", [this]() { SET_0_C(); }},
        {"SET 0,D", [this]() { SET_0_D(); }},
        {"SET 0,E", [this]() { SET_0_E(); }},
        {"SET 0,H", [this]() { SET_0_H(); }},
        {"SET 0,L", [this]() { SET_0_L(); }},
        {"SET 0,HLm", [this]() { SET_0_HLm(); }},
        {"SET 0,A", [this]() { SET_0_A(); }},
        {"SET 1,B", [this]() { SET_1_B(); }},
        {"SET 1,C", [this]() { SET_1_C(); }},
        {"SET 1,D", [this]() { SET_1_D(); }},
        {"SET 1,E", [this]() { SET_1_E(); }},
        {"SET 1,H", [this]() { SET_1_H(); }},
        {"SET 1,L", [this]() { SET_1_L(); }},
        {"SET 1,HLm", [this]() { SET_1_HLm(); }},
        {"SET 1,A", [this]() { SET_1_A(); }},
        {"SET 2,B", [this]() { SET_2_B(); }},
        {"SET 2,C", [this]() { SET_2_C(); }},
        {"SET 2,D", [this]() { SET_2_D(); }},
        {"SET 2,E", [this]() { SET_2_E(); }},
        {"SET 2,H", [this]() { SET_2_H(); }},
        {"SET 2,L", [this]() { SET_2_L(); }},
        {"SET 2,HLm", [this]() { SET_2_HLm(); }},
        {"SET 2,A", [this]() { SET_2_A(); }},
        {"SET 3,B", [this]() { SET_3_B(); }},
        {"SET 3,C", [this]() { SET_3_C(); }},
        {"SET 3,D", [this]() { SET_3_D(); }},
        {"SET 3,E", [this]() { SET_3_E(); }},
        {"SET 3,H", [this]() { SET_3_H(); }},
        {"SET 3,L", [this]() { SET_3_L(); }},
        {"SET 3,HLm", [this]() { SET_3_HLm(); }},
        {"SET 3,A", [this]() { SET_3_A(); }},
        {"SET 4,B", [this]() { SET_4_B(); }},
        {"SET 4,C", [this]() { SET_4_C(); }},
        {"SET 4,D", [this]() { SET_4_D(); }},
        {"SET 4,E", [this]() { SET_4_E(); }},
        {"SET 4,H", [this]() { SET_4_H(); }},
        {"SET 4,L", [this]() { SET_4_L(); }},
        {"SET 4,HLm", [this]() { SET_4_HLm(); }},
        {"SET 4,A", [this]() { SET_4_A(); }},
        {"SET 5,B", [this]() { SET_5_B(); }},
        {"SET 5,C", [this]() { SET_5_C(); }},
        {"SET 5,D", [this]() { SET_5_D(); }},
        {"SET 5,E", [this]() { SET_5_E(); }},
        {"SET 5,H", [this]() { SET_5_H(); }},
        {"SET 5,L", [this]() { SET_5_L(); }},
        {"SET 5,HLm", [this]() { SET_5_HLm(); }},
        {"SET 5,A", [this]() { SET_5_A(); }},
        {"SET 6,B", [this]() { SET_6_B(); }},
        {"SET 6,C", [this]() { SET_6_C(); }},
        {"SET 6,D", [this]() { SET_6_D(); }},
        {"SET 6,E", [this]() { SET_6_E(); }},
        {"SET 6,H", [this]() { SET_6_H(); }},
        {"SET 6,L", [this]() { SET_6_L(); }},
        {"SET 6,HLm", [this]() { SET_6_HLm(); }},
        {"SET 6,A", [this]() { SET_6_A(); }},
        {"SET 7,B", [this]() { SET_7_B(); }},
        {"SET 7,C", [this]() { SET_7_C(); }},
        {"SET 7,D", [this]() { SET_7_D(); }},
        {"SET 7,E", [this]() { SET_7_E(); }},
        {"SET 7,H", [this]() { SET_7_H(); }},
        {"SET 7,L", [this]() { SET_7_L(); }},
        {"SET 7,HLm", [this]() { SET_7_HLm(); }},
        {"SET 7,A", [this]() { SET_7_A(); }},
    };

    
    auto it = mnemonicToFunctionMapping.find(mnemonic);
    if (it != mnemonicToFunctionMapping.end()) {
        opcodeTable[opcode] = {it->second, mnemonic};
    } else {
        if (mnemonic.find("ILLEGAL") == std::string::npos) {
            std::cerr << "Error: Unknown mnemonic " << mnemonic << std::endl;
        }
        opcodeTable[opcode] = {[this]() { NOP(); }, "NOP"};
    }
}

// Initialize opcodes
void CPU::initializeOpcodes() {
    // Initialize opcode tables with NOP
    for (u16 i = 0; i < 256; i++) {
        m_opcodeTable[i] = {[this]() { NOP(); }, "NOP"};
        m_cbOpcodeTable[i] = {[this]() { NOP(); }, "NOP"};
    }
}

// Execute opcode
void CPU::executeOpcode(u8 opcode) {
    // Call opcode function
    if (m_registers.pc > 0x80) {
        std::cout << "Executing opcode 0x" << std::hex << (int)opcode << " at PC 0x" << m_registers.pc - 1 
                << " (" << m_opcodeTable[opcode].mnemonic << ")" << std::endl;
    }
    m_opcodeTable[opcode].function();
}

// Execute CB opcode
void CPU::executeCBOpcode(u8 opcode) {
    // Call CB opcode function
    if (m_registers.pc > 0x80) {
        std::cout << "Executing CB opcode 0x" << std::hex << (int)opcode << " at PC 0x" << m_registers.pc - 1 
                << " (" << m_cbOpcodeTable[opcode].mnemonic << ")" << std::endl;
    }
    m_cbOpcodeTable[opcode].function();
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
    
    std::cout << "DEC B: B=" << (int)m_registers.b << "->" << (int)result 
              << " Z=" << getFlag(FLAG_Z) << std::endl;
    
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
    std::cout << "DEC_BC: Before BC=0x" << std::hex << m_registers.bc 
              << " (B=0x" << (int)m_registers.b << ", C=0x" << (int)m_registers.c << ")" << std::endl;
    m_registers.bc--;
    std::cout << "DEC_BC: After  BC=0x" << std::hex << m_registers.bc 
              << " (B=0x" << (int)m_registers.b << ", C=0x" << (int)m_registers.c << ")" << std::endl;
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

void CPU::STOP_n8() {
    m_stopped = true;
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
    std::cout << "LD (HL-),A: HL=" << std::hex << m_registers.hl 
              << " A=" << (int)m_registers.a << std::endl;
    
    m_memory.write(m_registers.hl, m_registers.a);
    m_registers.hl--;
    
    std::cout << "After: HL=" << std::hex << m_registers.hl << std::endl;
    
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

void CPU::LD_HLm_n8() {
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

void CPU::CCF() {
    setFlag(FLAG_N, false);
    setFlag(FLAG_H, false);
    setFlag(FLAG_C, !getFlag(FLAG_C));
    
    m_cycles += 4;
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

void CPU::SUB_A_B() {
    u8 result = m_registers.a - m_registers.b;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.b & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.b);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_C() {
    u8 result = m_registers.a - m_registers.c;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.c & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.c);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_D() {
    u8 result = m_registers.a - m_registers.d;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.d & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.d);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_E() {
    u8 result = m_registers.a - m_registers.e;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.e & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.e);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_H() {
    u8 result = m_registers.a - m_registers.h;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.h & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.h);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_L() {
    u8 result = m_registers.a - m_registers.l;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (m_registers.l & 0x0F));
    setFlag(FLAG_C, m_registers.a < m_registers.l);
    
    m_registers.a = result;
    m_cycles += 4;
}

void CPU::SUB_A_HLm() {
    u8 value = m_memory.read(m_registers.hl);
    u8 result = m_registers.a - value;
    
    setFlag(FLAG_Z, result == 0);
    setFlag(FLAG_N, true);
    setFlag(FLAG_H, (m_registers.a & 0x0F) < (value & 0x0F));
    setFlag(FLAG_C, m_registers.a < value);
    
    m_registers.a = result;
    m_cycles += 8;
}

void CPU::SUB_A_A() {
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
    m_cycles += 4;
    executeCBOpcode(opcode);
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

void CPU::LDH_C_A() {
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