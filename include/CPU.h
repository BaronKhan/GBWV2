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

    // Unprefixed Instructions
    void NOP();
    void LD_BC_n16();  // 0x01
    void LD_BC_A();    // 0x02
    void INC_BC();     // 0x03
    void INC_B();      // 0x04
    void DEC_B();      // 0x05
    void LD_B_n8();    // 0x06
    void RLCA();       // 0x07
    void LD_a16_SP();  // 0x08
    void ADD_HL_BC();  // 0x09
    void LD_A_BC();    // 0x0A
    void DEC_BC();     // 0x0B
    void INC_C();      // 0x0C
    void DEC_C();      // 0x0D
    void LD_C_n8();    // 0x0E
    void RRCA();       // 0x0F
    void STOP();       // 0x10
    void LD_DE_n16();  // 0x11
    void LD_DE_A();    // 0x12
    void INC_DE();     // 0x13
    void INC_D();      // 0x14
    void DEC_D();      // 0x15
    void LD_D_n8();    // 0x16
    void RLA();        // 0x17
    void JR_e8();      // 0x18
    void ADD_HL_DE();  // 0x19
    void LD_A_DE();    // 0x1A
    void DEC_DE();     // 0x1B
    void INC_E();      // 0x1C
    void DEC_E();      // 0x1D
    void LD_E_n8();    // 0x1E
    void RRA();        // 0x1F
    void JR_NZ_e8();   // 0x20
    void LD_HL_n16();  // 0x21
    void LD_HLI_A();   // 0x22
    void INC_HL();     // 0x23
    void INC_H();      // 0x24
    void DEC_H();      // 0x25
    void LD_H_n8();    // 0x26
    void DAA();        // 0x27
    void JR_Z_e8();    // 0x28
    void ADD_HL_HL();  // 0x29
    void LD_A_HLI();   // 0x2A
    void DEC_HL();     // 0x2B
    void INC_L();      // 0x2C
    void DEC_L();      // 0x2D
    void LD_L_n8();    // 0x2E
    void CPL();        // 0x2F
    void JR_NC_e8();   // 0x30
    void LD_SP_n16();  // 0x31
    void LD_HLD_A();   // 0x32
    void INC_SP();     // 0x33
    void INC_HLm();    // 0x34
    void DEC_HLm();    // 0x35
    void LD_HL_n8();   // 0x36
    void SCF();        // 0x37
    void JR_C_e8();    // 0x38
    void ADD_HL_SP();  // 0x39
    void LD_A_HLD();   // 0x3A
    void DEC_SP();     // 0x3B
    void INC_A();      // 0x3C
    void DEC_A();      // 0x3D
    void LD_A_n8();    // 0x3E
    void CCF();        // 0x3F
    void LD_B_B();     // 0x40
    void LD_B_C();     // 0x41
    void LD_B_D();     // 0x42
    void LD_B_E();     // 0x43
    void LD_B_H();     // 0x44
    void LD_B_L();     // 0x45
    void LD_B_HLm();   // 0x46
    void LD_B_A();     // 0x47
    void LD_C_B();     // 0x48
    void LD_C_C();     // 0x49
    void LD_C_D();     // 0x4A
    void LD_C_E();     // 0x4B
    void LD_C_H();     // 0x4C
    void LD_C_L();     // 0x4D
    void LD_C_HLm();   // 0x4E
    void LD_C_A();     // 0x4F
    void LD_D_B();     // 0x50
    void LD_D_C();     // 0x51
    void LD_D_D();     // 0x52
    void LD_D_E();     // 0x53
    void LD_D_H();     // 0x54
    void LD_D_L();     // 0x55
    void LD_D_HLm();   // 0x56
    void LD_D_A();     // 0x57
    void LD_E_B();     // 0x58
    void LD_E_C();     // 0x59
    void LD_E_D();     // 0x5A
    void LD_E_E();     // 0x5B
    void LD_E_H();     // 0x5C
    void LD_E_L();     // 0x5D
    void LD_E_HLm();   // 0x5E
    void LD_E_A();     // 0x5F
    void LD_H_B();     // 0x60
    void LD_H_C();     // 0x61
    void LD_H_D();     // 0x62
    void LD_H_E();     // 0x63
    void LD_H_H();     // 0x64
    void LD_H_L();     // 0x65
    void LD_H_HLm();   // 0x66
    void LD_H_A();     // 0x67
    void LD_L_B();     // 0x68
    void LD_L_C();     // 0x69
    void LD_L_D();     // 0x6A
    void LD_L_E();     // 0x6B
    void LD_L_H();     // 0x6C
    void LD_L_L();     // 0x6D
    void LD_L_HLm();   // 0x6E
    void LD_L_A();     // 0x6F
    void LD_HLm_B();   // 0x70
    void LD_HLm_C();   // 0x71
    void LD_HLm_D();   // 0x72
    void LD_HLm_E();   // 0x73
    void LD_HLm_H();   // 0x74
    void LD_HLm_L();   // 0x75
    void HALT();       // 0x76
    void LD_HLm_A();    // 0x77
    void LD_A_B();      // 0x78
    void LD_A_C();      // 0x79
    void LD_A_D();      // 0x7A
    void LD_A_E();      // 0x7B
    void LD_A_H();      // 0x7C
    void LD_A_L();      // 0x7D
    void LD_A_HLm();    // 0x7E
    void LD_A_A();      // 0x7F
    void ADD_A_B();     // 0x80
    void ADD_A_C();     // 0x81
    void ADD_A_D();     // 0x82
    void ADD_A_E();     // 0x83
    void ADD_A_H();     // 0x84
    void ADD_A_L();     // 0x85
    void ADD_A_HLm();   // 0x86
    void ADD_A_A();     // 0x87
    void ADC_A_B();     // 0x88
    void ADC_A_C();     // 0x89
    void ADC_A_D();     // 0x8A
    void ADC_A_E();     // 0x8B
    void ADC_A_H();     // 0x8C
    void ADC_A_L();     // 0x8D
    void ADC_A_HLm();   // 0x8E
    void ADC_A_A();     // 0x8F
    void SUB_B();       // 0x90
    void SUB_C();       // 0x91
    void SUB_D();       // 0x92
    void SUB_E();       // 0x93
    void SUB_H();       // 0x94
    void SUB_L();       // 0x95
    void SUB_HLm();     // 0x96
    void SUB_A();       // 0x97
    void SBC_A_B();     // 0x98
    void SBC_A_C();     // 0x99
    void SBC_A_D();     // 0x9A
    void SBC_A_E();     // 0x9B
    void SBC_A_H();     // 0x9C
    void SBC_A_L();     // 0x9D
    void SBC_A_HLm();   // 0x9E
    void SBC_A_A();     // 0x9F
    void AND_B();       // 0xA0
    void AND_C();       // 0xA1
    void AND_D();       // 0xA2
    void AND_E();       // 0xA3
    void AND_H();       // 0xA4
    void AND_L();       // 0xA5
    void AND_HLm();     // 0xA6
    void AND_A();       // 0xA7
    void XOR_B();       // 0xA8
    void XOR_C();       // 0xA9
    void XOR_D();       // 0xAA
    void XOR_E();       // 0xAB
    void XOR_H();       // 0xAC
    void XOR_L();       // 0xAD
    void XOR_HLm();     // 0xAE
    void XOR_A();       // 0xAF
    void OR_B();        // 0xB0
    void OR_C();        // 0xB1
    void OR_D();        // 0xB2
    void OR_E();        // 0xB3
    void OR_H();        // 0xB4
    void OR_L();        // 0xB5
    void OR_HLm();      // 0xB6
    void OR_A();        // 0xB7
    void CP_B();        // 0xB8
    void CP_C();        // 0xB9
    void CP_D();        // 0xBA
    void CP_E();        // 0xBB
    void CP_H();        // 0xBC
    void CP_L();        // 0xBD
    void CP_HLm();      // 0xBE
    void CP_A();        // 0xBF
    void RET_NZ();      // 0xC0
    void POP_BC();      // 0xC1
    void JP_NZ_a16();   // 0xC2
    void JP_a16();      // 0xC3
    void CALL_NZ_a16(); // 0xC4
    void PUSH_BC();     // 0xC5
    void ADD_A_n8();    // 0xC6
    void RST_00H();     // 0xC7
    void RET_Z();       // 0xC8
    void RET();         // 0xC9
    void JP_Z_a16();    // 0xCA
    void PREFIX_CB();   // 0xCB
    void CALL_Z_a16();  // 0xCC
    void CALL_a16();    // 0xCD
    void ADC_A_n8();    // 0xCE
    void RST_08H();     // 0xCF
    void RET_NC();      // 0xD0
    void POP_DE();      // 0xD1
    void JP_NC_a16();   // 0xD2
    void CALL_NC_a16(); // 0xD4
    void PUSH_DE();     // 0xD5
    void SUB_n8();      // 0xD6
    void RST_10H();     // 0xD7
    void RET_C();       // 0xD8
    void RETI();        // 0xD9
    void JP_C_a16();    // 0xDA
    void CALL_C_a16();  // 0xDC
    void SBC_A_n8();    // 0xDE
    void RST_18H();     // 0xDF
    void LDH_a8_A();    // 0xE0
    void POP_HL();      // 0xE1
    void LD_C_A();      // 0xE2
    void PUSH_HL();     // 0xE5
    void AND_n8();      // 0xE6
    void RST_20H();     // 0xE7
    void ADD_SP_e8();   // 0xE8
    void JP_HL();       // 0xE9
    void LD_a16_A();    // 0xEA
    void XOR_n8();      // 0xEE
    void RST_28H();     // 0xEF
    void LDH_A_a8();    // 0xF0
    void POP_AF();      // 0xF1
    void LDH_A_C();      // 0xF2
    void DI();          // 0xF3
    void PUSH_AF();     // 0xF5
    void OR_n8();       // 0xF6
    void RST_30H();     // 0xF7
    void LD_HL_SP_e8(); // 0xF8
    void LD_SP_HL();    // 0xF9
    void LD_A_a16();    // 0xFA
    void EI();          // 0xFB
    void CP_n8();       // 0xFE
    void RST_38H();     // 0xFF

    // CB Prefixed Instructions
    void RLC_B();       // 0x00 (CB)
    void RLC_C();       // 0x01 (CB)
    void RLC_D();       // 0x02 (CB)
    void RLC_E();       // 0x03 (CB)
    void RLC_H();       // 0x04 (CB)
    void RLC_L();       // 0x05 (CB)
    void RLC_HLm();     // 0x06 (CB)
    void RLC_A();       // 0x07 (CB)
    void RRC_B();       // 0x08 (CB)
    void RRC_C();       // 0x09 (CB)
    void RRC_D();       // 0x0A (CB)
    void RRC_E();       // 0x0B (CB)
    void RRC_H();       // 0x0C (CB)
    void RRC_L();       // 0x0D (CB)
    void RRC_HLm();     // 0x0E (CB)
    void RRC_A();       // 0x0F (CB)
    void RL_B();        // 0x10 (CB)
    void RL_C();        // 0x11 (CB)
    void RL_D();        // 0x12 (CB)
    void RL_E();        // 0x13 (CB)
    void RL_H();        // 0x14 (CB)
    void RL_L();        // 0x15 (CB)
    void RL_HLm();      // 0x16 (CB)
    void RL_A();        // 0x17 (CB)
    void RR_B();        // 0x18 (CB)
    void RR_C();        // 0x19 (CB)
    void RR_D();        // 0x1A (CB)
    void RR_E();        // 0x1B (CB)
    void RR_H();        // 0x1C (CB)
    void RR_L();        // 0x1D (CB)
    void RR_HLm();      // 0x1E (CB)
    void RR_A();        // 0x1F (CB)
    void SLA_B();       // 0x20 (CB)
    void SLA_C();       // 0x21 (CB)
    void SLA_D();       // 0x22 (CB)
    void SLA_E();       // 0x23 (CB)
    void SLA_H();       // 0x24 (CB)
    void SLA_L();       // 0x25 (CB)
    void SLA_HLm();     // 0x26 (CB)
    void SLA_A();       // 0x27 (CB)
    void SRA_B();       // 0x28 (CB)
    void SRA_C();       // 0x29 (CB)
    void SRA_D();       // 0x2A (CB)
    void SRA_E();       // 0x2B (CB)
    void SRA_H();       // 0x2C (CB)
    void SRA_L();       // 0x2D (CB)
    void SRA_HLm();     // 0x2E (CB)
    void SRA_A();       // 0x2F (CB)
    void SWAP_B();      // 0x30 (CB)
    void SWAP_C();      // 0x31 (CB)
    void SWAP_D();      // 0x32 (CB)
    void SWAP_E();      // 0x33 (CB)
    void SWAP_H();      // 0x34 (CB)
    void SWAP_L();      // 0x35 (CB)
    void SWAP_HLm();    // 0x36 (CB)
    void SWAP_A();      // 0x37 (CB)
    void SRL_B();       // 0x38 (CB)
    void SRL_C();       // 0x39 (CB)
    void SRL_D();       // 0x3A (CB)
    void SRL_E();       // 0x3B (CB)
    void SRL_H();       // 0x3C (CB)
    void SRL_L();       // 0x3D (CB)
    void SRL_HLm();     // 0x3E (CB)
    void SRL_A();       // 0x3F (CB)
    void BIT_0_B();     // 0x40 (CB)
    void BIT_0_C();     // 0x41 (CB)
    void BIT_0_D();     // 0x42 (CB)
    void BIT_0_E();     // 0x43 (CB)
    void BIT_0_H();     // 0x44 (CB)
    void BIT_0_L();     // 0x45 (CB)
    void BIT_0_HLm();   // 0x46 (CB)
    void BIT_0_A();     // 0x47 (CB)
    void BIT_1_B();     // 0x48 (CB)
    void BIT_1_C();     // 0x49 (CB)
    void BIT_1_D();     // 0x4A (CB)
    void BIT_1_E();     // 0x4B (CB)
    void BIT_1_H();     // 0x4C (CB)
    void BIT_1_L();     // 0x4D (CB)
    void BIT_1_HLm();   // 0x4E (CB)
    void BIT_1_A();     // 0x4F (CB)
    void BIT_2_B();     // 0x50 (CB)
    void BIT_2_C();     // 0x51 (CB)
    void BIT_2_D();     // 0x52 (CB)
    void BIT_2_E();     // 0x53 (CB)
    void BIT_2_H();     // 0x54 (CB)
    void BIT_2_L();     // 0x55 (CB)
    void BIT_2_HLm();   // 0x56 (CB)
    void BIT_2_A();     // 0x57 (CB)
    void BIT_3_B();     // 0x58 (CB)
    void BIT_3_C();     // 0x59 (CB)
    void BIT_3_D();     // 0x5A (CB)
    void BIT_3_E();     // 0x5B (CB)
    void BIT_3_H();     // 0x5C (CB)
    void BIT_3_L();     // 0x5D (CB)
    void BIT_3_HLm();   // 0x5E (CB)
    void BIT_3_A();     // 0x5F (CB)
    void BIT_4_B();     // 0x60 (CB)
    void BIT_4_C();     // 0x61 (CB)
    void BIT_4_D();     // 0x62 (CB)
    void BIT_4_E();     // 0x63 (CB)
    void BIT_4_H();     // 0x64 (CB)
    void BIT_4_L();     // 0x65 (CB)
    void BIT_4_HLm();   // 0x66 (CB)
    void BIT_4_A();     // 0x67 (CB)
};