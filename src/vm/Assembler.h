#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RegisterName
{
    A = 0,
    B,
    C,
    D,
    SP,
    Count,
};

enum class Opcode
{
    Mov,
    Load,
    Store,
    Add,
    Sub,
    Cmp,
    Jmp,
    Jz,
    Jnz,
    In,
    Out,
    Nop,
    Halt,
};

enum class OperandType
{
    None,
    Register,
    Immediate,
    MemoryImmediate,
    MemoryRegister,
    Address,
    Port,
};

struct Operand
{
    OperandType type = OperandType::None;
    RegisterName reg = RegisterName::A;
    std::uint16_t value = 0;
};

struct Instruction
{
    Opcode opcode = Opcode::Nop;
    Operand left;
    Operand right;
    int sourceLine = 0;
    std::string sourceText;
};

struct Program
{
    std::vector<Instruction> instructions;
};

struct AssemblerError
{
    int line = 0;
    std::string message;
};

struct AssembleResult
{
    bool success = false;
    Program program;
    std::vector<AssemblerError> errors;
};

class Assembler
{
public:
    AssembleResult assemble(const std::string& source, const std::vector<std::string>& allowedInstructions) const;

    static std::string opcodeName(Opcode opcode);
    static std::string registerName(RegisterName reg);
};
