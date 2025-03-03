#pragma once

#include "Common.h"
#include "Memory.h"
#include "json.hpp"
#include <functional>

// CPU class
class CPU {
public:
    // Meyer's Singleton pattern
    static CPU& getInstance() {
        static CPU instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;

    // CPU registers
    struct Registers {
        union {
            struct {
                u8 f;  // Flags
                u8 a;  // Accumulator
            };
            u16 af;
        };

        union {
            struct {
                u8 c;
                u8 b;
            };
            u16 bc;
        };

        union {
            struct {
                u8 e;
                u8 d;
            };
            u16 de;
        };

        union {
            struct {
                u8 l;
                u8 h;
            };
            u16 hl;
        };

        u16 sp;  // Stack pointer
        u16 pc;  // Program counter
    };

    // Flag bit positions
    enum Flags {
        FLAG_Z = 7,  // Zero flag
        FLAG_N = 6,  // Subtract flag
        FLAG_H = 5,  // Half carry flag
        FLAG_C = 4   // Carry flag
    };

    // CPU control
    void reset();
    void step();
    u32 getCycles() const { return m_cycles; }

    // Interrupt handling
    void requestInterrupt(u8 interrupt);
    void handleInterrupts();

    // Load opcodes from JSON
    bool loadOpcodes(const std::string& filename);

    // Debug
    const Registers& getRegisters() const { return m_registers; }

    // Load opcodes from JSON
    void parseOpcodeJson(const nlohmann::json& json);
    void mapOpcodeToFunction(u8 opcode, const std::string& mnemonic, bool isCB);

    // Register-to-register loads
    void LD_B_B();
    void LD_B_C();
    void LD_B_D();
    void LD_B_E();
    void LD_B_H();
    void LD_B_L();
    void LD_B_HL();
    
    void LD_C_B();
    void LD_C_C();
    void LD_C_D();
    void LD_C_E();
    void LD_C_H();
    void LD_C_L();
    void LD_C_HL();
    
    void LD_D_B();
    void LD_D_C();
    void LD_D_D();
    void LD_D_E();
    void LD_D_H();
    void LD_D_L();
    void LD_D_HL();
    
    void LD_E_B();
    void LD_E_C();
    void LD_E_D();
    void LD_E_E();
    void LD_E_H();
    void LD_E_L();
    void LD_E_HL();
    
    void LD_H_B();
    void LD_H_C();
    void LD_H_D();
    void LD_H_E();
    void LD_H_H();
    void LD_H_L();
    void LD_H_HL();
    
    void LD_L_B();
    void LD_L_C();
    void LD_L_D();
    void LD_L_E();
    void LD_L_H();
    void LD_L_L();
    void LD_L_HL();
    
    void LD_HL_B();
    void LD_HL_C();
    void LD_HL_D();
    void LD_HL_E();
    void LD_HL_H();
    void LD_HL_L();
    
    void LD_A_A();
    
    // 16-bit loads
    void LD_BC_A();
    void LD_DE_A();
    void LD_HL_n();
    void LD_nn_A();
    void LD_A_nn();
    void LD_A_BC();
    void LD_A_DE();
    void LD_SP_HL();
    void PUSH_AF();
    void PUSH_BC();
    void PUSH_DE();
    void PUSH_HL();
    void POP_AF();
    void POP_BC();
    void POP_DE();
    void POP_HL();
    
    // 8-bit ALU
    void ADD_A_A();
    void ADD_A_B();
    void ADD_A_C();
    void ADD_A_D();
    void ADD_A_E();
    void ADD_A_H();
    void ADD_A_L();
    void ADD_A_HL();
    void ADD_A_n();
    
    void ADC_A_A();
    void ADC_A_B();
    void ADC_A_C();
    void ADC_A_D();
    void ADC_A_E();
    void ADC_A_H();
    void ADC_A_L();
    void ADC_A_HL();
    void ADC_A_n();
    
    void SUB_A();
    void SUB_B();
    void SUB_C();
    void SUB_D();
    void SUB_E();
    void SUB_H();
    void SUB_L();
    void SUB_HL();
    void SUB_n();
    
    void SBC_A_A();
    void SBC_A_B();
    void SBC_A_C();
    void SBC_A_D();
    void SBC_A_E();
    void SBC_A_H();
    void SBC_A_L();
    void SBC_A_HL();
    void SBC_A_n();
    
    void AND_A();
    void AND_B();
    void AND_C();
    void AND_D();
    void AND_E();
    void AND_H();
    void AND_L();
    void AND_HL();
    void AND_n();
    
    void OR_A();
    void OR_B();
    void OR_C();
    void OR_D();
    void OR_E();
    void OR_H();
    void OR_L();
    void OR_HL();
    void OR_n();
    
    // 16-bit arithmetic
    void ADD_HL_BC();
    void ADD_HL_DE();
    void ADD_HL_HL();
    void ADD_HL_SP();
    void ADD_SP_n();
    
    // Jumps
    void JP_nn();
    void JP_NZ_nn();
    void JP_Z_nn();
    void JP_NC_nn();
    void JP_C_nn();
    void JP_HL();
    
    // Calls
    void CALL_nn();
    void CALL_NZ_nn();
    void CALL_Z_nn();
    void CALL_NC_nn();
    void CALL_C_nn();
    
    // Returns
    void RET_NZ();
    void RET_Z();
    void RET_NC();
    void RET_C();
    void RETI();
    void RST_00H();
    void RST_08H();
    void RST_10H();
    void RST_18H();
    void RST_20H();
    void RST_28H();
    void RST_30H();
    void RST_38H();
    
    // CB-prefixed instructions
    void RLC_A();
    void RLC_B();
    void RLC_C();
    void RLC_D();
    void RLC_E();
    void RLC_H();
    void RLC_L();
    void RLC_HL();
    
    void RRC_A();
    void RRC_B();
    void RRC_C();
    void RRC_D();
    void RRC_E();
    void RRC_H();
    void RRC_L();
    void RRC_HL();
    
    void RL_A();
    void RL_B();
    void RL_C();
    void RL_D();
    void RL_E();
    void RL_H();
    void RL_L();
    void RL_HL();
    
    void RR_A();
    void RR_B();
    void RR_C();
    void RR_D();
    void RR_E();
    void RR_H();
    void RR_L();
    void RR_HL();
    
    void SLA_A();
    void SLA_B();
    void SLA_C();
    void SLA_D();
    void SLA_E();
    void SLA_H();
    void SLA_L();
    void SLA_HL();
    
    void SRA_A();
    void SRA_B();
    void SRA_C();
    void SRA_D();
    void SRA_E();
    void SRA_H();
    void SRA_L();
    void SRA_HL();
    
    void SWAP_A();
    void SWAP_B();
    void SWAP_C();
    void SWAP_D();
    void SWAP_E();
    void SWAP_H();
    void SWAP_L();
    void SWAP_HL();
    
    void SRL_A();
    void SRL_B();
    void SRL_C();
    void SRL_D();
    void SRL_E();
    void SRL_H();
    void SRL_L();
    void SRL_HL();
    
    // BIT instructions
    void BIT_0_B();
    void BIT_0_C();
    void BIT_0_D();
    void BIT_0_E();
    void BIT_0_H();
    void BIT_0_L();
    void BIT_0_HL();
    
    void BIT_1_B();
    void BIT_1_C();
    void BIT_1_D();
    void BIT_1_E();
    void BIT_1_H();
    void BIT_1_L();
    void BIT_1_HL();
    
    void BIT_2_B();
    void BIT_2_C();
    void BIT_2_D();
    void BIT_2_E();
    void BIT_2_H();
    void BIT_2_L();
    void BIT_2_HL();
    
    void BIT_3_B();
    void BIT_3_C();
    void BIT_3_D();
    void BIT_3_E();
    void BIT_3_H();
    void BIT_3_L();
    void BIT_3_HL();
    
    void BIT_4_B();
    void BIT_4_C();
    void BIT_4_D();
    void BIT_4_E();
    void BIT_4_H();
    void BIT_4_L();
    void BIT_4_HL();
    
    void BIT_5_B();
    void BIT_5_C();
    void BIT_5_D();
    void BIT_5_E();
    void BIT_5_H();
    void BIT_5_L();
    void BIT_5_HL();
    
    void BIT_6_B();
    void BIT_6_C();
    void BIT_6_D();
    void BIT_6_E();
    void BIT_6_H();
    void BIT_6_L();
    void BIT_6_HL();
    
    void BIT_7_B();
    void BIT_7_C();
    void BIT_7_D();
    void BIT_7_E();
    void BIT_7_L();
    void BIT_7_HL();
    
    // SET instructions
    void SET_0_A();
    void SET_0_B();
    void SET_0_C();
    void SET_0_D();
    void SET_0_E();
    void SET_0_H();
    void SET_0_L();
    void SET_0_HL();
    
    // ... more SET instructions for bits 1-7 ...
    
    // RES instructions
    void RES_0_A();
    void RES_0_B();
    void RES_0_C();
    void RES_0_D();
    void RES_0_E();
    void RES_0_H();
    void RES_0_L();
    void RES_0_HL();
    
    // ... more RES instructions for bits 1-7 ...

    // 16-bit register operations
    void INC_BC();
    void INC_DE();
    void INC_HL16();
    void INC_SP();
    void DEC_BC();
    void DEC_DE();
    void DEC_HL16();
    void DEC_SP();
    
    // Memory operations
    void LD_HLp_A();  // LD (HL+),A
    void LD_A_HLp();  // LD A,(HL+)
    void LD_A_HLm();  // LD A,(HL-)
    void LDH_C_A();   // LD (C),A
    void LDH_A_C();   // LD A,(C)
    
    // Jump operations
    void JP_NZ_a16();
    void JP_Z_a16();
    void JP_NC_a16();
    void JP_C_a16();
    
    // Call operations
    void CALL_NZ_a16();
    void CALL_Z_a16();
    void CALL_NC_a16();
    void CALL_C_a16();

    // Instruction implementations
    void NOP();
    
    // Add new instruction declarations
    void RLCA();
    void RLA();
    void RRCA();
    void RRA();
    void DAA();
    
    // Load instructions
    void LD_BC_d16();

private:
    // Private constructor for singleton
    CPU();

    // CPU state
    Registers m_registers;
    bool m_halted;
    bool m_stopped;
    bool m_interruptsEnabled;
    bool m_pendingInterruptEnable;
    u32 m_cycles;

    // Opcode tables
    using OpcodeFunction = std::function<void()>;
    std::array<OpcodeFunction, 256> m_opcodeTable;
    std::array<OpcodeFunction, 256> m_cbOpcodeTable;

    // Memory reference
    Memory& m_memory;

    // Opcode implementation
    void initializeOpcodes();
    void executeOpcode(u8 opcode);
    void executeCBOpcode(u8 opcode);

    // Helper methods for opcode implementation
    u8 readPC();
    u16 readPC16();
    void push(u16 value);
    u16 pop();

    // Flag operations
    bool getFlag(Flags flag) const;
    void setFlag(Flags flag, bool value);

    // Instruction implementations
    // These will be implemented based on the JSON opcode file
    // Here are just a few examples needed for the boot ROM
    void LD_BC_d16();
    void LD_DE_d16();
    void LD_HL_d16();
    void LD_SP_d16();
    void LD_A_d8();
    void LD_B_d8();
    void LD_C_d8();
    void LD_D_d8();
    void LD_E_d8();
    void LD_H_d8();
    void LD_L_d8();
    void LD_A_B();
    void LD_A_C();
    void LD_A_D();
    void LD_A_E();
    void LD_A_H();
    void LD_A_L();
    void LD_A_HL();
    void LD_B_A();
    void LD_C_A();
    void LD_D_A();
    void LD_E_A();
    void LD_H_A();
    void LD_L_A();
    void LD_HL_A();
    void LD_A_BC();
    void LD_A_DE();
    void LD_BC_A();
    void LD_DE_A();
    void XOR_A();
    void XOR_B();
    void XOR_C();
    void XOR_D();
    void XOR_E();
    void XOR_H();
    void XOR_L();
    void XOR_HL();
    void XOR_d8();
    void JP_a16();
    void JR_r8();
    void JR_NZ_r8();
    void JR_Z_r8();
    void JR_NC_r8();
    void JR_C_r8();
    void CALL_a16();
    void RET();
    void DI();
    void EI();
    void LDH_a8_A();
    void LDH_A_a8();
    void LD_C_A_MEM();
    void LD_A_C_MEM();
    void INC_A();
    void INC_B();
    void INC_C();
    void INC_D();
    void INC_E();
    void INC_H();
    void INC_L();
    void INC_HL_MEM();
    void INC_BC();
    void INC_DE();
    void INC_HL();
    void INC_SP();
    void DEC_A();
    void DEC_B();
    void DEC_C();
    void DEC_D();
    void DEC_E();
    void DEC_H();
    void DEC_L();
    void DEC_HL_MEM();
    void DEC_BC();
    void DEC_DE();
    void DEC_HL();
    void DEC_SP();
    void CP_d8();
    void CP_A();
    void CP_B();
    void CP_C();
    void CP_D();
    void CP_E();
    void CP_H();
    void CP_L();
    void CP_HL();
    void BIT_0_A();
    void BIT_1_A();
    void BIT_2_A();
    void BIT_3_A();
    void BIT_4_A();
    void BIT_5_A();
    void BIT_6_A();
    void BIT_7_A();
    void BIT_7_H();
}; 