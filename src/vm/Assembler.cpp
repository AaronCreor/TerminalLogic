#include "vm/Assembler.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace
{
struct ParsedLine
{
    int line = 0;
    std::string text;
};

std::string trim(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return value.substr(start, end - start);
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> splitArguments(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;
    int bracketDepth = 0;
    for (char ch : text)
    {
        if (ch == '[')
        {
            ++bracketDepth;
        }
        else if (ch == ']')
        {
            --bracketDepth;
        }

        if (ch == ',' && bracketDepth == 0)
        {
            tokens.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty())
    {
        tokens.push_back(trim(current));
    }
    return tokens;
}

bool parseRegister(const std::string& token, RegisterName& reg)
{
    const std::string lower = toLower(token);
    if (lower == "a")
    {
        reg = RegisterName::A;
        return true;
    }
    if (lower == "b")
    {
        reg = RegisterName::B;
        return true;
    }
    if (lower == "c")
    {
        reg = RegisterName::C;
        return true;
    }
    if (lower == "d")
    {
        reg = RegisterName::D;
        return true;
    }
    if (lower == "sp")
    {
        reg = RegisterName::SP;
        return true;
    }
    return false;
}

bool parseNumber(const std::string& token, std::uint16_t& value)
{
    try
    {
        const unsigned long parsed = std::stoul(token, nullptr, 0);
        value = static_cast<std::uint16_t>(parsed & 0xFFFFu);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool isJumpOpcode(Opcode opcode)
{
    return opcode == Opcode::Jmp || opcode == Opcode::Jz || opcode == Opcode::Jnz;
}

bool isAllowed(const std::vector<std::string>& allowedInstructions, const std::string& opcode)
{
    if (allowedInstructions.empty())
    {
        return true;
    }
    return std::find(allowedInstructions.begin(), allowedInstructions.end(), opcode) != allowedInstructions.end();
}

Operand parseOperand(const std::string& token, bool allowJumpAddress, const std::unordered_map<std::string, std::uint16_t>& labels, std::string& error)
{
    Operand operand;
    if (token.empty())
    {
        error = "missing operand";
        return operand;
    }

    RegisterName reg = RegisterName::A;
    if (parseRegister(token, reg))
    {
        operand.type = OperandType::Register;
        operand.reg = reg;
        return operand;
    }

    if (token.size() >= 3 && token.front() == '[' && token.back() == ']')
    {
        const std::string inside = trim(token.substr(1, token.size() - 2));
        if (parseRegister(inside, reg))
        {
            operand.type = OperandType::MemoryRegister;
            operand.reg = reg;
            return operand;
        }

        std::uint16_t address = 0;
        if (parseNumber(inside, address))
        {
            operand.type = OperandType::MemoryImmediate;
            operand.value = address;
            return operand;
        }

        error = "invalid memory operand '" + token + "'";
        return operand;
    }

    std::uint16_t number = 0;
    if (parseNumber(token, number))
    {
        operand.type = allowJumpAddress ? OperandType::Address : OperandType::Immediate;
        operand.value = number;
        return operand;
    }

    auto labelIt = labels.find(toLower(token));
    if (allowJumpAddress && labelIt != labels.end())
    {
        operand.type = OperandType::Address;
        operand.value = labelIt->second;
        return operand;
    }

    error = "invalid operand '" + token + "'";
    return operand;
}

bool validateInstructionOperands(Opcode opcode, const Instruction& instruction, std::string& error)
{
    const Operand& left = instruction.left;
    const Operand& right = instruction.right;
    switch (opcode)
    {
    case Opcode::Mov:
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Cmp:
        if (left.type != OperandType::Register)
        {
            error = "left operand must be a register";
            return false;
        }
        if (right.type == OperandType::None)
        {
            error = "missing right operand";
            return false;
        }
        return true;
    case Opcode::Load:
        if (left.type != OperandType::Register || (right.type != OperandType::MemoryImmediate && right.type != OperandType::MemoryRegister))
        {
            error = "LOAD expects 'LOAD reg, [addr]'";
            return false;
        }
        return true;
    case Opcode::Store:
        if ((left.type != OperandType::MemoryImmediate && left.type != OperandType::MemoryRegister) || right.type == OperandType::None)
        {
            error = "STORE expects 'STORE [addr], value'";
            return false;
        }
        return true;
    case Opcode::Jmp:
    case Opcode::Jz:
    case Opcode::Jnz:
        if (left.type != OperandType::Address)
        {
            error = "jump requires a label or numeric address";
            return false;
        }
        return true;
    case Opcode::In:
        if (left.type != OperandType::Register || right.type != OperandType::Immediate)
        {
            error = "IN expects 'IN reg, port'";
            return false;
        }
        return true;
    case Opcode::Out:
        if (left.type != OperandType::Immediate || right.type == OperandType::None)
        {
            error = "OUT expects 'OUT port, value'";
            return false;
        }
        return true;
    case Opcode::Nop:
    case Opcode::Halt:
        return true;
    }
    error = "unknown instruction";
    return false;
}
}

AssembleResult Assembler::assemble(const std::string& source, const std::vector<std::string>& allowedInstructions) const
{
    AssembleResult result;
    std::vector<ParsedLine> lines;
    std::unordered_map<std::string, std::uint16_t> labels;

    std::stringstream stream(source);
    std::string rawLine;
    int lineNumber = 0;
    std::uint16_t instructionIndex = 0;

    while (std::getline(stream, rawLine))
    {
        ++lineNumber;
        std::string withoutComment = rawLine;
        const std::size_t commentIndex = withoutComment.find(';');
        if (commentIndex != std::string::npos)
        {
            withoutComment = withoutComment.substr(0, commentIndex);
        }
        std::string text = trim(withoutComment);
        if (text.empty())
        {
            continue;
        }

        const std::size_t colon = text.find(':');
        if (colon != std::string::npos)
        {
            const std::string label = toLower(trim(text.substr(0, colon)));
            if (label.empty())
            {
                result.errors.push_back({ lineNumber, "empty label" });
                continue;
            }
            if (labels.contains(label))
            {
                result.errors.push_back({ lineNumber, "duplicate label '" + label + "'" });
                continue;
            }
            labels[label] = instructionIndex;
            text = trim(text.substr(colon + 1));
            if (text.empty())
            {
                continue;
            }
        }

        lines.push_back({ lineNumber, text });
        ++instructionIndex;
    }

    if (!result.errors.empty())
    {
        return result;
    }

    const std::unordered_map<std::string, Opcode> opcodeMap = {
        { "mov", Opcode::Mov },
        { "load", Opcode::Load },
        { "store", Opcode::Store },
        { "add", Opcode::Add },
        { "sub", Opcode::Sub },
        { "cmp", Opcode::Cmp },
        { "jmp", Opcode::Jmp },
        { "jz", Opcode::Jz },
        { "jnz", Opcode::Jnz },
        { "in", Opcode::In },
        { "out", Opcode::Out },
        { "nop", Opcode::Nop },
        { "halt", Opcode::Halt },
    };

    for (const ParsedLine& parsed : lines)
    {
        std::stringstream lineStream(parsed.text);
        std::string opcodeToken;
        lineStream >> opcodeToken;
        const std::string opcodeLower = toLower(opcodeToken);

        auto opcodeIt = opcodeMap.find(opcodeLower);
        if (opcodeIt == opcodeMap.end())
        {
            result.errors.push_back({ parsed.line, "unknown opcode '" + opcodeToken + "'" });
            continue;
        }
        if (!isAllowed(allowedInstructions, opcodeLower))
        {
            result.errors.push_back({ parsed.line, "opcode '" + opcodeLower + "' is locked for this level" });
            continue;
        }

        std::string remainder;
        std::getline(lineStream, remainder);
        remainder = trim(remainder);
        std::vector<std::string> operands = splitArguments(remainder);

        Instruction instruction;
        instruction.opcode = opcodeIt->second;
        instruction.sourceLine = parsed.line;
        instruction.sourceText = parsed.text;

        std::string operandError;
        if (!operands.empty())
        {
            instruction.left = parseOperand(operands[0], isJumpOpcode(instruction.opcode), labels, operandError);
            if (!operandError.empty())
            {
                result.errors.push_back({ parsed.line, operandError });
                continue;
            }
        }
        if (operands.size() > 1)
        {
            instruction.right = parseOperand(operands[1], false, labels, operandError);
            if (!operandError.empty())
            {
                result.errors.push_back({ parsed.line, operandError });
                continue;
            }
        }
        if (operands.size() > 2)
        {
            result.errors.push_back({ parsed.line, "too many operands" });
            continue;
        }

        if ((instruction.opcode == Opcode::In || instruction.opcode == Opcode::Out) && instruction.left.type == OperandType::Immediate)
        {
            instruction.left.type = OperandType::Immediate;
        }

        std::string validationError;
        if (!validateInstructionOperands(instruction.opcode, instruction, validationError))
        {
            result.errors.push_back({ parsed.line, validationError });
            continue;
        }

        result.program.instructions.push_back(instruction);
    }

    result.success = result.errors.empty();
    return result;
}

std::string Assembler::opcodeName(Opcode opcode)
{
    switch (opcode)
    {
    case Opcode::Mov: return "mov";
    case Opcode::Load: return "load";
    case Opcode::Store: return "store";
    case Opcode::Add: return "add";
    case Opcode::Sub: return "sub";
    case Opcode::Cmp: return "cmp";
    case Opcode::Jmp: return "jmp";
    case Opcode::Jz: return "jz";
    case Opcode::Jnz: return "jnz";
    case Opcode::In: return "in";
    case Opcode::Out: return "out";
    case Opcode::Nop: return "nop";
    case Opcode::Halt: return "halt";
    }
    return "?";
}

std::string Assembler::registerName(RegisterName reg)
{
    switch (reg)
    {
    case RegisterName::A: return "A";
    case RegisterName::B: return "B";
    case RegisterName::C: return "C";
    case RegisterName::D: return "D";
    case RegisterName::SP: return "SP";
    case RegisterName::Count: break;
    }
    return "?";
}
