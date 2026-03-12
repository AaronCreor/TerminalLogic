#pragma once

#include "audio/AudioSystem.h"
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
    enum class AppMode
    {
        MainMenu,
        Playing,
    };

    enum class FocusTarget
    {
        Editor,
        Command,
    };

    enum class TutorialStage
    {
        Welcome,
        Comments,
        Help,
        Step,
        Run,
        Complete,
    };

    bool initializeWindow();
    bool initializeAudio();
    bool loadLevels();
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update();
    void render();
    void renderMainMenu();
    void renderGameplay();
    void renderTutorialOverlay(const Rect& bounds);

    void handleTextInput(const char* text);
    void handleKeyDown(const SDL_KeyboardEvent& event);
    void handleMouseDown(int x, int y);
    void handleMouseUp();
    void handleMouseMotion(int x, int y);
    void handleMenuTextInput(const char* text);
    void handleGameplayTextInput(const char* text);
    void handleMenuKeyDown(const SDL_KeyboardEvent& event);
    void handleGameplayKeyDown(const SDL_KeyboardEvent& event);
    void handleEditorKeyDown(const SDL_KeyboardEvent& event);
    void handleCommandKeyDown(const SDL_KeyboardEvent& event);
    void submitCommandBuffer();
    void submitMenuCommand();
    void moveCommandHistory(int delta);

    void executeCommand(const std::string& commandLine, bool fromMenu);
    bool assembleSource(bool logSuccess);
    void stepMachine(int steps = 1);
    void runMachine();
    void resetMachine(bool logAction);
    bool loadLevelById(const std::string& id, bool logAction);
    void validateOutputs(bool fromRun);
    void openMainMenu();
    void startCurrentLevel();
    void startTutorialLevel();
    void toggleTutorial(bool enabled);
    void updateTutorialProgress();
    void toggleFullscreen(bool enabled);
    std::vector<std::string> parseArguments(const std::string& text) const;
    std::string currentCommandBuffer() const;
    std::size_t currentCommandCaret() const;
    void setCurrentCommandCaret(std::size_t caret);
    std::string& activeCommandBuffer();
    std::size_t& activeCommandCaret();
    int& activeHistoryIndex();
    const std::vector<std::string>& activeHistory() const;
    std::vector<std::string>& activeHistory();
    Rect menuDesktopRect() const;
    Rect menuSidebarRect() const;
    Rect menuTerminalRect() const;
    Rect menuLevelsRect() const;
    Rect tutorialRect() const;

    void pushLog(const std::string& line);
    std::vector<std::string> wrapText(const std::string& text, float maxWidth, float scale) const;
    std::string formatWord(std::uint16_t value) const;
    std::string joinWords(const std::vector<std::uint16_t>& values) const;
    std::vector<std::string> splitParagraphs(const std::string& text) const;
    std::vector<std::string> helpLinesForTopic(const std::string& topic) const;

    Rect editorRect() const;
    Rect consoleRect() const;
    Rect commandRect() const;
    Rect inspectorRect() const;
    float dividerX() const;

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool running_ = false;
    AppMode mode_ = AppMode::MainMenu;

    Renderer renderer_;
    AudioSystem audioSystem_;
    Theme theme_ = Theme::amber();
    bool crtEnabled_ = true;
    bool scanlinesEnabled_ = true;
    bool fullscreenEnabled_ = false;
    float inspectorWidthRatio_ = 0.33f;
    bool draggingDivider_ = false;
    bool draggingSelection_ = false;
    int windowedWidth_ = 1440;
    int windowedHeight_ = 900;

    std::filesystem::path assetRoot_;
    std::vector<LevelDefinition> levels_;
    const LevelDefinition* currentLevel_ = nullptr;

    TextBuffer editorBuffer_;
    FocusTarget focus_ = FocusTarget::Editor;
    std::string commandBuffer_;
    std::size_t commandCaret_ = 0;
    std::vector<std::string> commandHistory_;
    int commandHistoryIndex_ = -1;
    std::string menuCommandBuffer_;
    std::size_t menuCommandCaret_ = 0;
    std::vector<std::string> menuCommandHistory_;
    int menuCommandHistoryIndex_ = -1;
    std::vector<std::string> logLines_;
    int hoveredLevelIndex_ = -1;

    Assembler assembler_;
    Program program_;
    VirtualMachine machine_;
    bool programLoaded_ = false;
    std::string lastValidationMessage_;
    bool levelSolved_ = false;
    bool tutorialVisible_ = false;
    bool tutorialCommentSeen_ = false;
    bool tutorialHelpSeen_ = false;
    bool tutorialStepSeen_ = false;
    bool tutorialRunSeen_ = false;
    TutorialStage tutorialStage_ = TutorialStage::Welcome;
};
