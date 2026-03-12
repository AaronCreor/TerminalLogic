#include "app/LevelLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
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

std::vector<std::string> splitCsv(const std::string& value)
{
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        const std::string trimmed = trim(token);
        if (!trimmed.empty())
        {
            parts.push_back(trimmed);
        }
    }
    return parts;
}

std::vector<std::uint16_t> parseUInt16List(const std::string& value)
{
    std::vector<std::uint16_t> result;
    for (const std::string& token : splitCsv(value))
    {
        const unsigned long parsed = std::stoul(token, nullptr, 0);
        result.push_back(static_cast<std::uint16_t>(parsed & 0xFFFFu));
    }
    return result;
}

std::optional<LevelDefinition> parseLevelFile(const std::filesystem::path& path, std::string& errorMessage)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        errorMessage = "Failed to open level file: " + path.string();
        return std::nullopt;
    }

    LevelDefinition level;
    std::ostringstream starter;
    std::ostringstream tutorial;
    std::ostringstream hint;
    std::ostringstream briefing;
    std::ostringstream success;
    enum class Section
    {
        Metadata,
        Source,
        Tutorial,
        Hint,
        Briefing,
        Success,
    };
    Section section = Section::Metadata;
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line))
    {
        ++lineNumber;
        const std::string trimmed = trim(line);
        if (trimmed == "---")
        {
            section = Section::Source;
            continue;
        }
        if (trimmed == "---tutorial---")
        {
            section = Section::Tutorial;
            continue;
        }
        if (trimmed == "---hint---")
        {
            section = Section::Hint;
            continue;
        }
        if (trimmed == "---briefing---")
        {
            section = Section::Briefing;
            continue;
        }
        if (trimmed == "---success---")
        {
            section = Section::Success;
            continue;
        }

        if (section != Section::Metadata)
        {
            std::ostringstream* destination = &starter;
            if (section == Section::Tutorial)
            {
                destination = &tutorial;
            }
            else if (section == Section::Hint)
            {
                destination = &hint;
            }
            else if (section == Section::Briefing)
            {
                destination = &briefing;
            }
            else if (section == Section::Success)
            {
                destination = &success;
            }

            *destination << line;
            if (!input.eof())
            {
                *destination << '\n';
            }
            continue;
        }
        if (trimmed.empty())
        {
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos)
        {
            errorMessage = "Invalid level metadata in " + path.string() + " on line " + std::to_string(lineNumber);
            return std::nullopt;
        }

        const std::string key = toLower(trim(trimmed.substr(0, separator)));
        const std::string value = trim(trimmed.substr(separator + 1));

        if (key == "id")
        {
            level.id = value;
        }
        else if (key == "title")
        {
            level.title = value;
        }
        else if (key == "description")
        {
            level.description = value;
        }
        else if (key == "max_cycles")
        {
            level.maxCycles = std::stoi(value);
        }
        else if (key == "memory_words")
        {
            level.memoryWords = std::stoi(value);
        }
        else if (key == "allowed")
        {
            level.allowedInstructions = splitCsv(toLower(value));
        }
        else if (key == "input")
        {
            level.inputValues = parseUInt16List(value);
        }
        else if (key == "expected")
        {
            level.expectedValues = parseUInt16List(value);
        }
        else
        {
            errorMessage = "Unknown level key '" + key + "' in " + path.string();
            return std::nullopt;
        }
    }

    level.starterSource = starter.str();
    level.tutorialText = tutorial.str();
    level.hintText = hint.str();
    level.briefing = briefing.str();
    level.successText = success.str();

    if (level.id.empty() || level.title.empty())
    {
        errorMessage = "Level file missing required id/title: " + path.string();
        return std::nullopt;
    }

    return level;
}
}

std::vector<LevelDefinition> LevelLoader::loadDirectory(const std::filesystem::path& directory, std::string& errorMessage)
{
    std::vector<LevelDefinition> levels;
    if (!std::filesystem::exists(directory))
    {
        errorMessage = "Level directory not found: " + directory.string();
        return levels;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".level")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files)
    {
        std::string fileError;
        std::optional<LevelDefinition> level = parseLevelFile(file, fileError);
        if (!level.has_value())
        {
            errorMessage = fileError;
            levels.clear();
            return levels;
        }
        levels.push_back(*level);
    }

    if (levels.empty())
    {
        errorMessage = "No .level files found in " + directory.string();
    }

    return levels;
}
