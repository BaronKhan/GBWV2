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

    // Instructions
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
    void LD_HLm_A();   // 0x76
    void LD_A_B();     // 0x77
    void LD_A_C();     // 0x78
    void LD_A_D();     // 0x79
    void LD_A_E();     // 0x7A
    void LD_A_H();     // 0x7B
    void LD_A_L();     // 0x7C
    void LD_A_HLm();   // 0x7D
    void LD_A_A();     // 0x7E
    void ADD_A_B();    // 0x7F
    void ADD_A_C();    // 0x80
    void ADD_A_D();    // 0x81
    void ADD_A_E();    // 0x82
    void ADD_A_H();    // 0x83
    void ADD_A_L();    // 0x84
    void ADD_A_HLm();  // 0x85
    void ADD_A_A();    // 0x86
    void ADC_A_B();    // 0x87
    void ADC_A_C();    // 0x88
    void ADC_A_D();    // 0x89
    void ADC_A_E();    // 0x8A
    void ADC_A_H();    // 0x8B
    void ADC_A_L();    // 0x8C
    void ADC_A_HLm();  // 0x8D
    void ADC_A_A();    // 0x8E
    void SUB_B();      // 0x8F
    void SUB_C();      // 0x90
    void SUB_D();      // 0x91
    void SUB_E();      // 0x92
    void SUB_H();      // 0x93
    void SUB_L();      // 0x94
    void SUB_HLm();    // 0x95
    void SUB_A();      // 0x96
    void SBC_A_B();    // 0x97
    void SBC_A_C();    // 0x98
    void SBC_A_D();    // 0x99
    void SBC_A_E();    // 0x9A
    void SBC_A_H();    // 0x9B
    void SBC_A_L();    // 0x9C
    void SBC_A_HLm();  // 0x9D
    void SBC_A_A();    // 0x9E
    void AND_B();      // 0x9F
    void AND_C();      // 0xA0
    void AND_D();      // 0xA1
    void AND_E();      // 0xA2
    void AND_H();      // 0xA3
    void AND_L();      // 0xA4
    void AND_HLm();    // 0xA5
    void AND_A();      // 0xA6
    void XOR_B();      // 0xA7
    void XOR_C();      // 0xA8
    void XOR_D();      // 0xA9
    void XOR_E();      // 0xAA
    void XOR_H();      // 0xAB
    void XOR_L();      // 0xAC
    void XOR_HLm();    // 0xAD
    void XOR_A();      // 0xAE
    void OR_B();       // 0xAF
    void OR_C();       // 0xB0
    void OR_D();       // 0xB1
    void OR_E();       // 0xB2
    void OR_H();       // 0xB3
    void OR_L();       // 0xB4
    void OR_HLm();     // 0xB5
    void OR_A();       // 0xB6
    void CP_B();       // 0xB7
    void CP_C();       // 0xB8
    void CP_D();       // 0xB9
    void CP_E();       // 0xBA
    void CP_H();       // 0xBB
    void CP_L();       // 0xBC
    void CP_HLm();     // 0xBD
    void CP_A();       // 0xBE
    void RET_NZ();     // 0xBF
    void POP_BC();     // 0xC0
    void JP_NZ_a16();  // 0xC1
    void JP_a16();     // 0xC2
    void CALL_NZ_a16();// 0xC3
    void PUSH_BC();    // 0xC4
    void ADD_A_n8();   // 0xC5
    void RST_00H();    // 0xC6
    void RET_Z();      // 0xC7
    void RET();        // 0xC8
    void JP_Z_a16();   // 0xC9
    void PREFIX_CB();   // 0xCA
    void CALL_Z_a16(); // 0xCB
    void CALL_a16();   // 0xCC
    void ADC_A_n8();   // 0xCD
    void RST_08H();    // 0xCE
    void RET_NC();     // 0xCF
    void POP_DE();     // 0xD0
    void JP_NC_a16();  // 0xD1
    void CALL_NC_a16();// 0xD2
    void PUSH_DE();    // 0xD3
};