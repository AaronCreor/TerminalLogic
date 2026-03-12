#pragma once

#include "app/LevelLoader.h"
#include "render/Renderer.h"
#include "ui/TextBuffer.h"
#include "vm/Assembler.h"
#include "vm/VirtualMachine.h"

#include <SDL2/SDL.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class Application
{
public:
    ~Application();

    bool initialize(int argc, char** argv);
    int run();

private:
    enum class FocusTarget
    {
        Editor,
        Command,
    };

    bool initializeWindow();
    bool loadLevels();
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update();
    void render();

    void handleTextInput(const char* text);
    void handleKeyDown(const SDL_KeyboardEvent& event);
    void handleMouseDown(int x, int y);
    void handleMouseUp();
    void handleMouseMotion(int x, int y);

    void executeCommand(const std::string& commandLine);
    bool assembleSource(bool logSuccess);
    void stepMachine();
    void runMachine();
    void resetMachine(bool logAction);
    bool loadLevelById(const std::string& id, bool logAction);
    void validateOutputs(bool fromRun);

    void pushLog(const std::string& line);
    std::vector<std::string> wrapText(const std::string& text, float maxWidth, float scale) const;
    std::string formatWord(std::uint16_t value) const;
    std::string joinWords(const std::vector<std::uint16_t>& values) const;

    Rect editorRect() const;
    Rect consoleRect() const;
    Rect commandRect() const;
    Rect inspectorRect() const;
    float dividerX() const;

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool running_ = false;

    Renderer renderer_;
    Theme theme_ = Theme::amber();
    bool crtEnabled_ = true;
    bool scanlinesEnabled_ = true;
    bool curvatureEnabled_ = true;
    float inspectorWidthRatio_ = 0.33f;
    bool draggingDivider_ = false;

    std::filesystem::path assetRoot_;
    std::vector<LevelDefinition> levels_;
    const LevelDefinition* currentLevel_ = nullptr;

    TextBuffer editorBuffer_;
    FocusTarget focus_ = FocusTarget::Editor;
    std::string commandBuffer_;
    std::size_t commandCaret_ = 0;
    std::vector<std::string> commandHistory_;
    int commandHistoryIndex_ = -1;
    std::vector<std::string> logLines_;

    Assembler assembler_;
    Program program_;
    VirtualMachine machine_;
    bool programLoaded_ = false;
    std::string lastValidationMessage_;
    bool levelSolved_ = false;
};
