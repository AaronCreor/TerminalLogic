#include "vm/VirtualMachine.h"

#include <algorithm>
#include <sstream>

namespace
{
std::string joinValues(const std::vector<std::uint16_t>& values)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        output << values[i];
        if (i + 1 < values.size())
        {
            output << ", ";
        }
    }
    return output.str();
}
}

void VirtualMachine::loadProgram(const Program& program, int memoryWords, const std::vector<std::uint16_t>& inputValues, int cycleLimit)
{
    program_ = program;
    memory_.assign(static_cast<std::size_t>(std::max(memoryWords, 1)), 0);
    initialInput_ = inputValues;
    cycleLimit_ = cycleLimit;
    reset(inputValues);
}

void VirtualMachine::reset(const std::vector<std::uint16_t>& inputValues)
{
    std::fill(memory_.begin(), memory_.end(), 0);
    registers_.fill(0);
    if (!memory_.empty())
    {
        registers_[static_cast<std::size_t>(RegisterName::SP)] = static_cast<std::uint16_t>(memory_.size() - 1);
    }
    inputQueue_ = inputValues;
    outputQueue_.clear();
    trace_.clear();
    instructionPointer_ = 0;
    zeroFlag_ = false;
    halted_ = false;
    lastError_.clear();
    cycles_ = 0;
    memoryHighWater_ = 0;
}

std::uint16_t VirtualMachine::reg(RegisterName reg) const
{
    return registers_[static_cast<std::size_t>(reg)];
}

StepResult VirtualMachine::step()
{
    if (!lastError_.empty())
    {
        return StepResult::Error;
    }
    if (halted_)
    {
        return StepResult::Halted;
    }
    if (instructionPointer_ >= program_.instructions.size())
    {
        setError("instruction pointer moved beyond program");
        return StepResult::Error;
    }
    if (cycles_ >= cycleLimit_)
    {
        setError("cycle limit reached");
        return StepResult::Error;
    }

    const Instruction& instruction = program_.instructions[instructionPointer_];
    std::ostringstream traceLine;
    traceLine << instructionPointer_ << ": " << instruction.sourceText;
    pushTrace(traceLine.str());

    bool ok = true;
    auto writeRegister = [&](RegisterName regName, std::uint16_t value) {
        registers_[static_cast<std::size_t>(regName)] = value;
    };

    const std::uint16_t nextIp = static_cast<std::uint16_t>(instructionPointer_ + 1);
    ++cycles_;

    switch (instruction.opcode)
    {
    case Opcode::Mov:
        writeRegister(instruction.left.reg, resolveValue(instruction.right, ok));
        instructionPointer_ = nextIp;
        break;
    case Opcode::Load:
        writeRegister(instruction.left.reg, readMemory(instruction.right, ok));
        instructionPointer_ = nextIp;
        break;
    case Opcode::Store:
    {
        const std::uint16_t address = resolveAddress(instruction.left, ok);
        const std::uint16_t value = resolveValue(instruction.right, ok);
        if (ok && address < memory_.size())
        {
            memory_[address] = value;
            memoryHighWater_ = std::max(memoryHighWater_, static_cast<int>(address + 1));
        }
        instructionPointer_ = nextIp;
        break;
    }
    case Opcode::Add:
        writeRegister(instruction.left.reg, static_cast<std::uint16_t>(reg(instruction.left.reg) + resolveValue(instruction.right, ok)));
        instructionPointer_ = nextIp;
        break;
    case Opcode::Sub:
        writeRegister(instruction.left.reg, static_cast<std::uint16_t>(reg(instruction.left.reg) - resolveValue(instruction.right, ok)));
        instructionPointer_ = nextIp;
        break;
    case Opcode::Cmp:
        zeroFlag_ = reg(instruction.left.reg) == resolveValue(instruction.right, ok);
        instructionPointer_ = nextIp;
        break;
    case Opcode::Jmp:
        instructionPointer_ = instruction.left.value;
        break;
    case Opcode::Jz:
        instructionPointer_ = zeroFlag_ ? instruction.left.value : nextIp;
        break;
    case Opcode::Jnz:
        instructionPointer_ = zeroFlag_ ? nextIp : instruction.left.value;
        break;
    case Opcode::In:
        if (instruction.right.value != 0)
        {
            setError("only input port 0 is available in the current prototype");
            ok = false;
            break;
        }
        if (inputQueue_.empty())
        {
            setError("input queue is empty");
            ok = false;
            break;
        }
        writeRegister(instruction.left.reg, inputQueue_.front());
        inputQueue_.erase(inputQueue_.begin());
        instructionPointer_ = nextIp;
        break;
    case Opcode::Out:
        if (instruction.left.value != 1)
        {
            setError("only output port 1 is available in the current prototype");
            ok = false;
            break;
        }
        outputQueue_.push_back(resolveValue(instruction.right, ok));
        instructionPointer_ = nextIp;
        break;
    case Opcode::Nop:
        instructionPointer_ = nextIp;
        break;
    case Opcode::Halt:
        halted_ = true;
        break;
    }

    if (!ok)
    {
        return StepResult::Error;
    }
    if (!lastError_.empty())
    {
        return StepResult::Error;
    }
    if (halted_)
    {
        return StepResult::Halted;
    }
    return StepResult::Running;
}

RunSummary VirtualMachine::runToEnd()
{
    RunSummary summary;
    while (true)
    {
        StepResult result = step();
        if (result == StepResult::Running)
        {
            continue;
        }
        if (result == StepResult::Halted)
        {
            summary.success = true;
            summary.halted = true;
            summary.message = "Program halted successfully. Output: [" + joinValues(outputQueue_) + "]";
            return summary;
        }

        summary.success = false;
        summary.halted = halted_;
        summary.cycleLimitHit = lastError_ == "cycle limit reached";
        summary.message = lastError_;
        return summary;
    }
}

std::uint16_t VirtualMachine::resolveValue(const Operand& operand, bool& ok)
{
    switch (operand.type)
    {
    case OperandType::Register:
        return reg(operand.reg);
    case OperandType::Immediate:
    case OperandType::Address:
    case OperandType::Port:
        return operand.value;
    case OperandType::MemoryImmediate:
    case OperandType::MemoryRegister:
        return readMemory(operand, ok);
    case OperandType::None:
        break;
    }
    ok = false;
    setError("invalid operand value");
    return 0;
}

std::uint16_t VirtualMachine::readMemory(const Operand& operand, bool& ok)
{
    const std::uint16_t address = resolveAddress(operand, ok);
    if (!ok)
    {
        return 0;
    }
    if (address >= memory_.size())
    {
        ok = false;
        setError("memory access out of bounds");
        return 0;
    }
    memoryHighWater_ = std::max(memoryHighWater_, static_cast<int>(address + 1));
    return memory_[address];
}

std::uint16_t VirtualMachine::resolveAddress(const Operand& operand, bool& ok)
{
    switch (operand.type)
    {
    case OperandType::MemoryImmediate:
        return operand.value;
    case OperandType::MemoryRegister:
        return reg(operand.reg);
    case OperandType::Address:
        return operand.value;
    default:
        ok = false;
        setError("invalid address operand");
        return 0;
    }
}

void VirtualMachine::pushTrace(const std::string& line)
{
    trace_.push_back(line);
    constexpr std::size_t maxTraceLines = 16;
    if (trace_.size() > maxTraceLines)
    {
        trace_.erase(trace_.begin());
    }
}

void VirtualMachine::setError(const std::string& message)
{
    if (lastError_.empty())
    {
        lastError_ = message;
    }
}
