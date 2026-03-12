#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct LevelDefinition
{
    std::string id;
    std::string title;
    std::string description;
    int maxCycles = 256;
    int memoryWords = 64;
    std::vector<std::string> allowedInstructions;
    std::vector<std::uint16_t> inputValues;
    std::vector<std::uint16_t> expectedValues;
    std::string starterSource;
};

class LevelLoader
{
public:
    static std::vector<LevelDefinition> loadDirectory(const std::filesystem::path& directory, std::string& errorMessage);
};
