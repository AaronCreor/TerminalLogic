#pragma once

#include "vm/Assembler.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class StepResult
{
    Running,
    Halted,
    Error,
};

struct RunSummary
{
    bool success = false;
    bool halted = false;
    bool cycleLimitHit = false;
    std::string message;
};

class VirtualMachine
{
public:
    void loadProgram(const Program& program, int memoryWords, const std::vector<std::uint16_t>& inputValues, int cycleLimit);
    void reset(const std::vector<std::uint16_t>& inputValues);

    StepResult step();
    RunSummary runToEnd();

    std::uint16_t reg(RegisterName reg) const;
    std::uint16_t ip() const { return instructionPointer_; }
    bool zeroFlag() const { return zeroFlag_; }
    bool halted() const { return halted_; }
    bool hasError() const { return !lastError_.empty(); }
    const std::string& lastError() const { return lastError_; }
    int cycles() const { return cycles_; }
    int cycleLimit() const { return cycleLimit_; }
    int memoryHighWater() const { return memoryHighWater_; }
    const std::vector<std::uint16_t>& memory() const { return memory_; }
    const std::vector<std::uint16_t>& inputQueue() const { return inputQueue_; }
    const std::vector<std::uint16_t>& outputQueue() const { return outputQueue_; }
    const std::vector<std::string>& trace() const { return trace_; }

private:
    std::uint16_t resolveValue(const Operand& operand, bool& ok);
    std::uint16_t readMemory(const Operand& operand, bool& ok);
    std::uint16_t resolveAddress(const Operand& operand, bool& ok);
    void pushTrace(const std::string& line);
    void setError(const std::string& message);

    Program program_;
    std::vector<std::uint16_t> memory_;
    std::array<std::uint16_t, static_cast<std::size_t>(RegisterName::Count)> registers_{};
    std::vector<std::uint16_t> initialInput_;
    std::vector<std::uint16_t> inputQueue_;
    std::vector<std::uint16_t> outputQueue_;
    std::vector<std::string> trace_;

    std::uint16_t instructionPointer_ = 0;
    bool zeroFlag_ = false;
    bool halted_ = false;
    std::string lastError_;
    int cycles_ = 0;
    int cycleLimit_ = 0;
    int memoryHighWater_ = 0;
};
