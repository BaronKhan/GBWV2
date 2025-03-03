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
}; 