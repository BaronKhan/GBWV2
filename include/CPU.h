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

    // Load opcodes from JSON
    void parseOpcodeJson(const nlohmann::json& json);

    // Instruction implementations
    // These will be implemented based on the JSON opcode file
    // Here are just a few examples needed for the boot ROM
    void NOP();
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