#include "app/Application.h"

#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>

namespace
{
constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 900;
constexpr float kPanelMargin = 20.0f;
constexpr float kHeaderHeight = 64.0f;
constexpr float kCommandHeight = 40.0f;
constexpr float kConsoleHeight = 220.0f;
constexpr const char* kEmployerName = "Helix Relay Corporation";
constexpr const char* kRoleName = "Junior Systems Programmer";
constexpr const char* kDivisionName = "Signal Recovery Division";

struct CommandSpec
{
    const char* name;
    const char* usage;
    const char* summary;
    const char* details;
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

bool parseBoolToken(const std::string& token, bool currentValue, bool& parsedValue)
{
    const std::string lower = toLower(token);
    if (lower == "on" || lower == "true" || lower == "1")
    {
        parsedValue = true;
        return true;
    }
    if (lower == "off" || lower == "false" || lower == "0")
    {
        parsedValue = false;
        return true;
    }
    if (lower == "toggle")
    {
        parsedValue = !currentValue;
        return true;
    }
    return false;
}

struct TutorialChecklistItem
{
    const char* label;
    bool complete;
};

const std::array<CommandSpec, 14> kCommands = {{
    { "help", "help [topic]", "Show all commands or detailed help for one topic.", "Topics include commands, comments, memory, registers, instructions, and any command name." },
    { "levels", "levels", "List all available levels.", "Use this from the desktop or while playing to see every loadable puzzle." },
    { "load", "load <id>", "Load a level by id.", "Example: load signal_boost. Loading resets the machine and refreshes the source editor." },
    { "assemble", "assemble", "Assemble the current source code.", "Assembly checks comments, labels, and allowed instructions for the current level." },
    { "run", "run", "Assemble if needed and run until halt or error.", "Use this once your program is ready. Solved levels report cycles and memory usage." },
    { "step", "step [count]", "Execute one or more instructions.", "Example: step 4. This is the best way to learn how registers, output, and trace change over time." },
    { "reset", "reset", "Reset the virtual machine.", "The current program stays loaded if assembly already succeeded." },
    { "theme", "theme amber|green", "Switch monitor phosphor theme.", "Amber is the default workstation look. Green gives a colder terminal feel." },
    { "crt", "crt on|off", "Enable or disable the CRT presentation layer.", "CRT toggles scanlines, glow, noise, and edge treatment as one package." },
    { "scanlines", "scanlines on|off", "Toggle scanlines only.", "Useful when tuning readability versus atmosphere." },
    { "audio", "audio on|off", "Toggle ambient hum and UI sounds.", "Audio uses generated tones so it works without external sound assets." },
    { "fullscreen", "fullscreen on|off|toggle", "Switch true fullscreen mode.", "This uses SDL exclusive fullscreen when available." },
    { "tutorial", "tutorial show|hide|replay", "Show, hide, or restart the current tutorial overlay.", "The first puzzle uses a guided tutorial that reacts to your actions." },
    { "menu", "menu", "Return to the in-universe desktop.", "You can resume from the desktop or launch another puzzle there." },
}};

const CommandSpec* findCommandSpec(const std::string& name)
{
    const std::string lower = toLower(name);
    for (const CommandSpec& command : kCommands)
    {
        if (lower == command.name)
        {
            return &command;
        }
    }
    return nullptr;
}
}

Application::~Application()
{
    shutdown();
}

bool Application::initialize(int /*argc*/, char** /*argv*/)
{
    assetRoot_ = std::filesystem::path(ASMLAB_PROJECT_ROOT) / "assets";
    if (!initializeWindow())
    {
        return false;
    }
    initializeAudio();
    if (!loadLevels())
    {
        return false;
    }

    pushLog("Terminal Core workstation online.");
    pushLog(std::string("Employee profile loaded: ") + kRoleName + " // " + kEmployerName + ".");
    pushLog("Shift queue ready. Click a contract or type 'help'.");
    openMainMenu();

    running_ = true;
    SDL_StartTextInput();
    return true;
}

bool Application::initializeWindow()
{
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window_ = SDL_CreateWindow(
        "Terminal Core",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (window_ == nullptr)
    {
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr)
    {
        return false;
    }

    SDL_GL_SetSwapInterval(1);
    renderer_.resize(kWindowWidth, kWindowHeight);
    return true;
}

bool Application::initializeAudio()
{
    return audioSystem_.initialize();
}

bool Application::loadLevels()
{
    std::string errorMessage;
    levels_ = LevelLoader::loadDirectory(assetRoot_ / "levels", errorMessage);
    if (levels_.empty())
    {
        pushLog(errorMessage.empty() ? "No levels were loaded." : errorMessage);
        return false;
    }
    return loadLevelById(levels_.front().id, false);
}

void Application::shutdown()
{
    audioSystem_.shutdown();
    if (glContext_ != nullptr)
    {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

int Application::run()
{
    while (running_)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            handleEvent(event);
        }
        update();
        render();
        SDL_GL_SwapWindow(window_);
    }
    return 0;
}

void Application::handleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_QUIT:
        running_ = false;
        break;
    case SDL_TEXTINPUT:
        handleTextInput(event.text.text);
        break;
    case SDL_KEYDOWN:
        handleKeyDown(event.key);
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            handleMouseDown(event.button.x, event.button.y);
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            handleMouseUp();
        }
        break;
    case SDL_MOUSEMOTION:
        handleMouseMotion(event.motion.x, event.motion.y);
        break;
    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            if (!fullscreenEnabled_)
            {
                windowedWidth_ = event.window.data1;
                windowedHeight_ = event.window.data2;
            }
            renderer_.resize(event.window.data1, event.window.data2);
        }
        break;
    default:
        break;
    }
}

void Application::update()
{
    updateTutorialProgress();
}

void Application::render()
{
    renderer_.beginFrame(theme_);
    if (mode_ == AppMode::MainMenu)
    {
        renderMainMenu();
    }
    else
    {
        renderGameplay();
    }
    renderer_.endFrame(crtEnabled_, scanlinesEnabled_);
}

void Application::renderMainMenu()
{
    const Rect desktop = menuDesktopRect();
    const Rect sidebar = menuSidebarRect();
    const Rect levelsRect = menuLevelsRect();
    const Rect terminal = menuTerminalRect();

    renderer_.drawRect(desktop, theme_.panelFill);
    renderer_.drawOutline(desktop, theme_.border, 2.0f);
    renderer_.drawRect({ desktop.x, desktop.y, desktop.w, 34.0f }, theme_.panelFillAlt);
    renderer_.drawOutline({ desktop.x, desktop.y, desktop.w, 34.0f }, theme_.border, 1.0f);
    renderer_.drawText(desktop.x + 16.0f, desktop.y + 10.0f, "TERMINAL CORE // HELIX RELAY DESKTOP", 1.25f, theme_.accent);
    renderer_.drawRect(sidebar, theme_.panelFillAlt);
    renderer_.drawOutline(sidebar, theme_.border, 2.0f);
    renderer_.drawText(sidebar.x + 14.0f, sidebar.y + 12.0f, "WORKSTATION", 1.25f, theme_.accent);

    const std::array<std::string, 5> menuLabels = {
        "Resume Current Contract",
        "Guided Tutorial",
        fullscreenEnabled_ ? "Disable Fullscreen" : "Enable Fullscreen",
        audioSystem_.enabled() ? "Mute Audio" : "Enable Audio",
        "Shutdown"
    };
    for (std::size_t i = 0; i < menuLabels.size(); ++i)
    {
        const Rect button{ sidebar.x + 14.0f, sidebar.y + 48.0f + static_cast<float>(i) * 58.0f, sidebar.w - 28.0f, 44.0f };
        renderer_.drawRect(button, theme_.panelFill);
        renderer_.drawOutline(button, theme_.border, 2.0f);
        renderer_.drawText(button.x + 12.0f, button.y + 13.0f, menuLabels[i], 1.0f, theme_.text);
    }

    const Rect storyPanel{ sidebar.x + 14.0f, sidebar.y + 346.0f, sidebar.w - 28.0f, sidebar.h - 360.0f };
    renderer_.drawRect(storyPanel, theme_.panelFill);
    renderer_.drawOutline(storyPanel, theme_.border, 2.0f);
    renderer_.drawText(storyPanel.x + 12.0f, storyPanel.y + 12.0f, "PERSONNEL", 1.0f, theme_.accent);
    renderer_.drawText(storyPanel.x + 12.0f, storyPanel.y + 32.0f, kEmployerName, 1.0f, theme_.text);
    renderer_.drawText(storyPanel.x + 12.0f, storyPanel.y + 50.0f, kRoleName, 1.0f, theme_.text);
    renderer_.drawText(storyPanel.x + 12.0f, storyPanel.y + 68.0f, kDivisionName, 1.0f, theme_.text);
    float storyY = storyPanel.y + 96.0f;
    const std::string storyBlurb = "You maintain remote relay infrastructure after automated systems fail. Each contract is a live recovery job routed through Terminal Core.";
    for (const std::string& line : wrapText(storyBlurb, storyPanel.w - 24.0f, 1.0f))
    {
        renderer_.drawText(storyPanel.x + 12.0f, storyY, line, 1.0f, theme_.textMuted);
        storyY += 16.0f;
    }

    renderer_.drawRect(levelsRect, theme_.panelFillAlt);
    renderer_.drawOutline(levelsRect, theme_.border, 2.0f);
    renderer_.drawText(levelsRect.x + 14.0f, levelsRect.y + 12.0f, "JOB QUEUE", 1.25f, theme_.accent);
    if (currentLevel_ != nullptr)
    {
        renderer_.drawText(levelsRect.x + 180.0f, levelsRect.y + 12.0f, "Current: " + currentLevel_->title, 1.0f, theme_.textMuted);
    }

    for (std::size_t i = 0; i < levels_.size(); ++i)
    {
        const Rect row{ levelsRect.x + 14.0f, levelsRect.y + 42.0f + static_cast<float>(i) * 58.0f, levelsRect.w - 28.0f, 46.0f };
        const bool selected = currentLevel_ != nullptr && currentLevel_->id == levels_[i].id;
        renderer_.drawRect(row, selected ? theme_.panelFill : theme_.backgroundTop);
        renderer_.drawOutline(row, selected ? theme_.accent : theme_.border, 2.0f);
        renderer_.drawText(row.x + 12.0f, row.y + 8.0f, levels_[i].title + " [" + levels_[i].id + "]", 1.0f, selected ? theme_.accent : theme_.text);
        renderer_.drawText(row.x + 12.0f, row.y + 24.0f, levels_[i].description, 1.0f, theme_.textMuted);
    }

    renderer_.drawRect(terminal, theme_.panelFill);
    renderer_.drawOutline(terminal, theme_.border, 2.0f);
    renderer_.drawText(terminal.x + 14.0f, terminal.y + 12.0f, "DESKTOP TERMINAL", 1.25f, theme_.accent);

    const float lineHeight = renderer_.glyphHeight(1.0f) + 4.0f;
    const int maxLines = std::max(1, static_cast<int>((terminal.h - 76.0f) / lineHeight));
    const int start = std::max(0, static_cast<int>(logLines_.size()) - maxLines);
    for (int i = start; i < static_cast<int>(logLines_.size()); ++i)
    {
        const float y = terminal.y + 38.0f + static_cast<float>(i - start) * lineHeight;
        renderer_.drawText(terminal.x + 14.0f, y, logLines_[i], 1.0f, theme_.textMuted);
    }

    renderer_.drawText(terminal.x + 14.0f, terminal.y + terminal.h - 26.0f, "> " + menuCommandBuffer_, 1.25f, theme_.text);
    if ((SDL_GetTicks() / 500) % 2 == 0)
    {
        const float caretX = terminal.x + 14.0f + renderer_.glyphWidth(1.25f) * static_cast<float>(2 + menuCommandCaret_);
        renderer_.drawRect({ caretX, terminal.y + terminal.h - 28.0f, 2.0f, renderer_.glyphHeight(1.25f) }, theme_.accent);
    }
}

void Application::renderGameplay()
{
    const Rect header{ kPanelMargin, kPanelMargin, static_cast<float>(renderer_.width()) - kPanelMargin * 2.0f, kHeaderHeight };
    const Rect leftEditor = editorRect();
    const Rect leftConsole = consoleRect();
    const Rect command = commandRect();
    const Rect inspector = inspectorRect();

    renderer_.drawRect(header, theme_.panelFillAlt);
    renderer_.drawOutline(header, theme_.border, 2.0f);
    renderer_.drawText(header.x + 16.0f, header.y + 14.0f, currentLevel_ != nullptr ? currentLevel_->title : "No Level", 2.0f, theme_.accent);
    renderer_.drawText(header.x + 16.0f, header.y + 38.0f,
        currentLevel_ != nullptr ? currentLevel_->description : "Load a level to begin.",
        1.0f, theme_.textMuted);

    const std::string machineState = levelSolved_ ? "SOLVED" : (machine_.hasError() ? "FAULT" : (machine_.halted() ? "HALTED" : "READY"));
    renderer_.drawText(header.x + header.w - 180.0f, header.y + 14.0f, machineState, 2.0f, levelSolved_ ? theme_.success : theme_.text);
    renderer_.drawText(header.x + header.w - 280.0f, header.y + 40.0f,
        "cycles " + std::to_string(machine_.cycles()) + "/" + std::to_string(machine_.cycleLimit()) + "  F11 fullscreen",
        1.0f, theme_.textMuted);

    renderer_.drawRect(leftEditor, theme_.panelFill);
    renderer_.drawOutline(leftEditor, focus_ == FocusTarget::Editor ? theme_.accent : theme_.border, 2.0f);
    renderer_.drawText(leftEditor.x + 14.0f, leftEditor.y + 12.0f, "SOURCE / TERMINAL", 1.25f, theme_.accent);

    renderer_.drawRect(leftConsole, theme_.panelFillAlt);
    renderer_.drawOutline(leftConsole, theme_.border, 2.0f);
    renderer_.drawText(leftConsole.x + 14.0f, leftConsole.y + 12.0f, "COMMAND CONSOLE", 1.25f, theme_.accent);

    renderer_.drawRect(command, theme_.panelFill);
    renderer_.drawOutline(command, focus_ == FocusTarget::Command ? theme_.accent : theme_.border, 2.0f);

    renderer_.drawRect(inspector, theme_.panelFill);
    renderer_.drawOutline(inspector, theme_.border, 2.0f);
    renderer_.drawText(inspector.x + 14.0f, inspector.y + 12.0f, "INSPECTOR", 1.25f, theme_.accent);

    const float editorScale = 1.5f;
    const float editorLineHeight = renderer_.glyphHeight(editorScale) + 4.0f;
    const float editorStartX = leftEditor.x + 16.0f;
    const float editorStartY = leftEditor.y + 38.0f;
    const int maxEditorLines = static_cast<int>((leftEditor.h - 54.0f) / editorLineHeight);
    const float textStartX = editorStartX + 36.0f;
    if (editorBuffer_.hasSelection())
    {
        const TextBuffer::Position selectionStart = editorBuffer_.selectionStart();
        const TextBuffer::Position selectionEnd = editorBuffer_.selectionEnd();
        for (std::size_t row = selectionStart.row; row <= selectionEnd.row && row < editorBuffer_.lines().size(); ++row)
        {
            if (static_cast<int>(row) >= maxEditorLines)
            {
                break;
            }
            const std::size_t startColumn = row == selectionStart.row ? selectionStart.column : 0;
            const std::size_t endColumn = row == selectionEnd.row ? selectionEnd.column : editorBuffer_.lines()[row].size();
            const float x = textStartX + static_cast<float>(startColumn) * renderer_.glyphWidth(editorScale);
            const float y = editorStartY + static_cast<float>(row) * editorLineHeight;
            const float width = std::max(renderer_.glyphWidth(editorScale), static_cast<float>(endColumn - startColumn) * renderer_.glyphWidth(editorScale));
            renderer_.drawRect({ x, y - 1.0f, width, renderer_.glyphHeight(editorScale) + 2.0f }, { theme_.accent.r, theme_.accent.g, theme_.accent.b, 0.25f });
        }
    }
    for (int i = 0; i < maxEditorLines && i < static_cast<int>(editorBuffer_.lines().size()); ++i)
    {
        std::ostringstream label;
        label << (i + 1);
        renderer_.drawText(editorStartX, editorStartY + i * editorLineHeight, label.str(), 1.0f, theme_.textMuted);
        renderer_.drawText(textStartX, editorStartY + i * editorLineHeight, editorBuffer_.lines()[i], editorScale, theme_.text);
    }
    if (focus_ == FocusTarget::Editor && (SDL_GetTicks() / 500) % 2 == 0)
    {
        const float caretX = textStartX + static_cast<float>(editorBuffer_.caretColumn()) * renderer_.glyphWidth(editorScale);
        const float caretY = editorStartY + static_cast<float>(editorBuffer_.caretRow()) * editorLineHeight;
        renderer_.drawRect({ caretX, caretY, 2.0f, renderer_.glyphHeight(editorScale) }, theme_.accent);
    }

    const float consoleTextScale = 1.0f;
    const float consoleLineHeight = renderer_.glyphHeight(consoleTextScale) + 4.0f;
    const int maxConsoleLines = static_cast<int>((leftConsole.h - 60.0f) / consoleLineHeight);
    const int consoleStart = std::max(0, static_cast<int>(logLines_.size()) - maxConsoleLines);
    for (int i = consoleStart; i < static_cast<int>(logLines_.size()); ++i)
    {
        const float y = leftConsole.y + 38.0f + static_cast<float>(i - consoleStart) * consoleLineHeight;
        renderer_.drawText(leftConsole.x + 16.0f, y, logLines_[i], consoleTextScale, theme_.textMuted);
    }

    renderer_.drawText(command.x + 14.0f, command.y + 12.0f, "> " + commandBuffer_, 1.25f, theme_.text);
    if (focus_ == FocusTarget::Command && (SDL_GetTicks() / 500) % 2 == 0)
    {
        const float caretX = command.x + 14.0f + renderer_.glyphWidth(1.25f) * static_cast<float>(2 + commandCaret_);
        renderer_.drawRect({ caretX, command.y + 10.0f, 2.0f, renderer_.glyphHeight(1.25f) }, theme_.accent);
    }

    float infoY = inspector.y + 40.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, "LEVEL", 1.0f, theme_.accent);
    infoY += 18.0f;
    if (currentLevel_ != nullptr)
    {
        renderer_.drawText(inspector.x + 14.0f, infoY, currentLevel_->id, 1.0f, theme_.text);
        infoY += 18.0f;
        const std::string briefing = currentLevel_->briefing.empty() ? currentLevel_->description : currentLevel_->briefing;
        for (const std::string& line : wrapText(briefing, inspector.w - 28.0f, 1.0f))
        {
            renderer_.drawText(inspector.x + 14.0f, infoY, line, 1.0f, theme_.textMuted);
            infoY += 16.0f;
        }
    }

    infoY += 12.0f;
    renderer_.drawLine(inspector.x + 12.0f, infoY, inspector.x + inspector.w - 12.0f, infoY, theme_.border, 1.0f);
    infoY += 12.0f;

    renderer_.drawText(inspector.x + 14.0f, infoY, "REGISTERS", 1.0f, theme_.accent);
    infoY += 20.0f;
    const std::vector<RegisterName> regs = { RegisterName::A, RegisterName::B, RegisterName::C, RegisterName::D, RegisterName::SP };
    for (RegisterName reg : regs)
    {
        renderer_.drawText(inspector.x + 14.0f, infoY, Assembler::registerName(reg) + ": " + formatWord(machine_.reg(reg)), 1.0f, theme_.text);
        infoY += 16.0f;
    }
    renderer_.drawText(inspector.x + 14.0f, infoY, "IP: " + formatWord(machine_.ip()), 1.0f, theme_.text);
    infoY += 16.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, std::string("ZERO: ") + (machine_.zeroFlag() ? "1" : "0"), 1.0f, theme_.text);
    infoY += 20.0f;

    renderer_.drawText(inspector.x + 14.0f, infoY, "I/O", 1.0f, theme_.accent);
    infoY += 18.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, "IN : [" + joinWords(machine_.inputQueue()) + "]", 1.0f, theme_.textMuted);
    infoY += 16.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, "OUT: [" + joinWords(machine_.outputQueue()) + "]", 1.0f, levelSolved_ ? theme_.success : theme_.textMuted);
    infoY += 18.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, "Expected: [" + (currentLevel_ != nullptr ? joinWords(currentLevel_->expectedValues) : std::string()) + "]", 1.0f, theme_.textMuted);
    infoY += 22.0f;

    renderer_.drawText(inspector.x + 14.0f, infoY, "MEMORY", 1.0f, theme_.accent);
    infoY += 18.0f;
    const auto& memory = machine_.memory();
    const int memoryRows = std::min<int>(8, static_cast<int>(memory.size()));
    for (int i = 0; i < memoryRows; ++i)
    {
        renderer_.drawText(inspector.x + 14.0f, infoY,
            formatWord(static_cast<std::uint16_t>(i)) + ": " + formatWord(memory[static_cast<std::size_t>(i)]),
            1.0f, theme_.textMuted);
        infoY += 16.0f;
    }
    infoY += 6.0f;
    renderer_.drawText(inspector.x + 14.0f, infoY, "Touched: " + std::to_string(machine_.memoryHighWater()) + " words", 1.0f, theme_.textMuted);
    infoY += 22.0f;

    renderer_.drawText(inspector.x + 14.0f, infoY, "TRACE", 1.0f, theme_.accent);
    infoY += 18.0f;
    for (const std::string& line : machine_.trace())
    {
        renderer_.drawText(inspector.x + 14.0f, infoY, line, 1.0f, theme_.textMuted);
        infoY += 16.0f;
    }

    if (!lastValidationMessage_.empty())
    {
        const Color statusColor = levelSolved_ ? theme_.success : (machine_.hasError() ? theme_.warning : theme_.text);
        renderer_.drawText(inspector.x + 14.0f, inspector.y + inspector.h - 28.0f, lastValidationMessage_, 1.0f, statusColor);
    }

    if (tutorialVisible_)
    {
        renderTutorialOverlay(tutorialRect());
    }
}

void Application::renderTutorialOverlay(const Rect& bounds)
{
    renderer_.drawRect({ 0.0f, 0.0f, static_cast<float>(renderer_.width()), static_cast<float>(renderer_.height()) }, { 0.0f, 0.0f, 0.0f, 0.35f });
    renderer_.drawRect(bounds, theme_.panelFillAlt);
    renderer_.drawOutline(bounds, theme_.accent, 2.0f);
    renderer_.drawText(bounds.x + 16.0f, bounds.y + 14.0f, "GUIDED TUTORIAL", 1.5f, theme_.accent);

    std::string title = "Welcome";
    std::string body = currentLevel_ != nullptr ? currentLevel_->tutorialText : "";
    std::string footer = "Esc closes the overlay. tutorial replay restarts it.";
    std::string currentTask = "Review the briefing and get familiar with the workstation.";

    if (currentLevel_ != nullptr && currentLevel_->id == "signal_boost")
    {
        switch (tutorialStage_)
        {
        case TutorialStage::Welcome:
            title = "Start Here";
            body = "You are on your first live maintenance shift at Helix Relay Corporation. Terminal Core routes emergency field contracts to programmers when automatic recovery scripts fail. This workstation is how you inspect, patch, and re-run remote relay logic.";
            footer = "First task: make sure your source contains a comment line that starts with ';'.";
            currentTask = "Read the starter code and keep a ';' comment in the editor.";
            break;
        case TutorialStage::Comments:
            title = "Comments";
            body = "Comments begin with ';'. Everything after ';' on that line is ignored by the assembler. Helix operators leave quick notes in code so the next programmer can understand an emergency fix at a glance.";
            footer = "Next task: in the command line, type help or help in.";
            currentTask = "Use a comment to describe what the starter program does.";
            break;
        case TutorialStage::Help:
            title = "Ask The Terminal";
            body = "The terminal can explain both workstation commands and assembly instructions. Try help run, help step, help memory, help in, or help add whenever you need context during a repair job.";
            footer = "Next task: step once with F10 or by typing step.";
            currentTask = "Ask for help on a command or instruction, such as help in or help add.";
            break;
        case TutorialStage::Step:
            title = "Stepping";
            body = "Use F10 or step to execute one instruction at a time. After each step, read the inspector: registers show working values, IN shrinks as values are consumed, OUT grows as results are written, and TRACE shows the last instructions executed.";
            footer = "Next task: run the full program with F5 or run.";
            currentTask = "Step once and watch the inspector change.";
            break;
        case TutorialStage::Run:
            title = "Running";
            body = "Use F5 or run to execute the whole program. For this level, read one value from input port 0 into a register, add five to it, then send the result to output port 1. If the output matches the expected value, the contract is solved.";
            footer = "You can reopen this overlay with tutorial show or restart it with tutorial replay.";
            currentTask = "Run the full repair and match the expected output.";
            break;
        case TutorialStage::Complete:
            title = "Good";
            body = currentLevel_->successText.empty() ? "You have completed your onboarding contract at Helix Relay. Use the desktop to load more jobs, and use help <command> whenever you need a refresher." : currentLevel_->successText;
            footer = "Keep experimenting with step, reset, help add, help in, and help memory.";
            currentTask = "Contract complete. You are ready for the next queue item.";
            break;
        }
    }

    const std::array<TutorialChecklistItem, 4> checklist = {{
        { "Keep a ';' comment in the source", tutorialCommentSeen_ },
        { "Ask the terminal for help", tutorialHelpSeen_ },
        { "Step through one instruction", tutorialStepSeen_ },
        { "Run the full repair successfully", levelSolved_ || tutorialRunSeen_ },
    }};

    const Rect textRect{ bounds.x + 16.0f, bounds.y + 48.0f, bounds.w * 0.58f, bounds.h - 108.0f };
    const Rect checklistRect{ bounds.x + bounds.w * 0.63f, bounds.y + 48.0f, bounds.w * 0.31f, bounds.h - 108.0f };

    float y = textRect.y;
    renderer_.drawText(textRect.x, y, title, 1.25f, theme_.text);
    y += 24.0f;
    renderer_.drawText(textRect.x, y, std::string("Employer: ") + kEmployerName, 1.0f, theme_.accent);
    y += 22.0f;
    for (const std::string& paragraph : splitParagraphs(body))
    {
        for (const std::string& line : wrapText(paragraph, textRect.w - 8.0f, 1.0f))
        {
            renderer_.drawText(textRect.x, y, line, 1.0f, theme_.textMuted);
            y += 18.0f;
        }
        y += 10.0f;
    }

    renderer_.drawRect(checklistRect, theme_.panelFill);
    renderer_.drawOutline(checklistRect, theme_.border, 2.0f);
    renderer_.drawText(checklistRect.x + 12.0f, checklistRect.y + 12.0f, "CHECKLIST", 1.0f, theme_.accent);
    float checklistY = checklistRect.y + 38.0f;
    for (const TutorialChecklistItem& item : checklist)
    {
        const std::string prefix = item.complete ? "[x] " : "[ ] ";
        for (const std::string& line : wrapText(prefix + item.label, checklistRect.w - 24.0f, 1.0f))
        {
            renderer_.drawText(checklistRect.x + 12.0f, checklistY, line, 1.0f, item.complete ? theme_.success : theme_.text);
            checklistY += 18.0f;
        }
        checklistY += 8.0f;
    }
    checklistY += 6.0f;
    renderer_.drawLine(checklistRect.x + 12.0f, checklistY, checklistRect.x + checklistRect.w - 12.0f, checklistY, theme_.border, 1.0f);
    checklistY += 14.0f;
    renderer_.drawText(checklistRect.x + 12.0f, checklistY, "CURRENT TASK", 1.0f, theme_.accent);
    checklistY += 18.0f;
    for (const std::string& line : wrapText(currentTask, checklistRect.w - 24.0f, 1.0f))
    {
        renderer_.drawText(checklistRect.x + 12.0f, checklistY, line, 1.0f, theme_.textMuted);
        checklistY += 18.0f;
    }

    renderer_.drawLine(bounds.x + 16.0f, bounds.y + bounds.h - 48.0f, bounds.x + bounds.w - 16.0f, bounds.y + bounds.h - 48.0f, theme_.border, 1.0f);
    renderer_.drawText(bounds.x + 16.0f, bounds.y + bounds.h - 32.0f, footer, 1.0f, theme_.accent);
}

void Application::handleTextInput(const char* text)
{
    if (mode_ == AppMode::MainMenu)
    {
        handleMenuTextInput(text);
    }
    else
    {
        handleGameplayTextInput(text);
    }
}

void Application::handleMenuTextInput(const char* text)
{
    menuCommandBuffer_.insert(menuCommandCaret_, text);
    menuCommandCaret_ += SDL_strlen(text);
    audioSystem_.playKeyStroke();
}

void Application::handleGameplayTextInput(const char* text)
{
    if (focus_ == FocusTarget::Editor)
    {
        if (std::strchr(text, ';') != nullptr)
        {
            tutorialCommentSeen_ = true;
        }
        editorBuffer_.insertText(text);
        audioSystem_.playKeyStroke();
    }
    else
    {
        commandBuffer_.insert(commandCaret_, text);
        commandCaret_ += SDL_strlen(text);
        audioSystem_.playKeyStroke();
    }
}

void Application::handleKeyDown(const SDL_KeyboardEvent& event)
{
    if (event.keysym.sym == SDLK_F11)
    {
        toggleFullscreen(!fullscreenEnabled_);
        return;
    }

    if (mode_ == AppMode::MainMenu)
    {
        handleMenuKeyDown(event);
    }
    else
    {
        handleGameplayKeyDown(event);
    }
}

void Application::handleMenuKeyDown(const SDL_KeyboardEvent& event)
{
    switch (event.keysym.sym)
    {
    case SDLK_ESCAPE:
        running_ = false;
        return;
    case SDLK_RETURN:
        submitMenuCommand();
        return;
    case SDLK_BACKSPACE:
        if (menuCommandCaret_ > 0)
        {
            menuCommandBuffer_.erase(menuCommandCaret_ - 1, 1);
            --menuCommandCaret_;
            audioSystem_.playKeyStroke();
        }
        return;
    case SDLK_DELETE:
        if (menuCommandCaret_ < menuCommandBuffer_.size())
        {
            menuCommandBuffer_.erase(menuCommandCaret_, 1);
        }
        return;
    case SDLK_LEFT:
        if (menuCommandCaret_ > 0)
        {
            --menuCommandCaret_;
        }
        return;
    case SDLK_RIGHT:
        if (menuCommandCaret_ < menuCommandBuffer_.size())
        {
            ++menuCommandCaret_;
        }
        return;
    case SDLK_UP:
        moveCommandHistory(-1);
        return;
    case SDLK_DOWN:
        moveCommandHistory(1);
        return;
    default:
        return;
    }
}

void Application::handleGameplayKeyDown(const SDL_KeyboardEvent& event)
{
    if (event.keysym.sym == SDLK_ESCAPE)
    {
        if (tutorialVisible_)
        {
            tutorialVisible_ = false;
        }
        else
        {
            openMainMenu();
        }
        return;
    }
    if (event.keysym.sym == SDLK_TAB)
    {
        focus_ = focus_ == FocusTarget::Editor ? FocusTarget::Command : FocusTarget::Editor;
        audioSystem_.playUiMove();
        return;
    }
    if (event.keysym.sym == SDLK_F5)
    {
        runMachine();
        return;
    }
    if (event.keysym.sym == SDLK_F6)
    {
        resetMachine(true);
        return;
    }
    if (event.keysym.sym == SDLK_F10)
    {
        stepMachine();
        return;
    }

    if (focus_ == FocusTarget::Editor)
    {
        handleEditorKeyDown(event);
    }
    else
    {
        handleCommandKeyDown(event);
    }
}

void Application::handleEditorKeyDown(const SDL_KeyboardEvent& event)
{
    const SDL_Keymod modifiers = SDL_GetModState();
    const bool ctrl = (modifiers & KMOD_CTRL) != 0;
    const bool shift = (modifiers & KMOD_SHIFT) != 0;

    if (ctrl)
    {
        switch (event.keysym.sym)
        {
        case SDLK_c:
            editorBuffer_.copySelectionToClipboard();
            return;
        case SDLK_x:
            editorBuffer_.cutSelectionToClipboard();
            audioSystem_.playUiMove();
            return;
        case SDLK_v:
            if (editorBuffer_.pasteFromClipboard())
            {
                audioSystem_.playKeyStroke();
            }
            return;
        case SDLK_z:
            editorBuffer_.undo();
            audioSystem_.playUiMove();
            return;
        default:
            break;
        }
    }

    switch (event.keysym.sym)
    {
    case SDLK_RETURN:
        editorBuffer_.newLine();
        audioSystem_.playKeyStroke();
        break;
    case SDLK_BACKSPACE:
        editorBuffer_.backspace();
        audioSystem_.playKeyStroke();
        break;
    case SDLK_DELETE:
        editorBuffer_.deleteForward();
        break;
    case SDLK_LEFT:
        if (shift)
        {
            editorBuffer_.moveLeftSelecting();
        }
        else
        {
            editorBuffer_.moveLeft();
        }
        break;
    case SDLK_RIGHT:
        if (shift)
        {
            editorBuffer_.moveRightSelecting();
        }
        else
        {
            editorBuffer_.moveRight();
        }
        break;
    case SDLK_UP:
        if (shift)
        {
            editorBuffer_.moveUpSelecting();
        }
        else
        {
            editorBuffer_.moveUp();
        }
        break;
    case SDLK_DOWN:
        if (shift)
        {
            editorBuffer_.moveDownSelecting();
        }
        else
        {
            editorBuffer_.moveDown();
        }
        break;
    case SDLK_HOME:
        if (shift)
        {
            editorBuffer_.moveHomeSelecting();
        }
        else
        {
            editorBuffer_.moveHome();
        }
        break;
    case SDLK_END:
        if (shift)
        {
            editorBuffer_.moveEndSelecting();
        }
        else
        {
            editorBuffer_.moveEnd();
        }
        break;
    default:
        break;
    }
}

void Application::handleCommandKeyDown(const SDL_KeyboardEvent& event)
{
    switch (event.keysym.sym)
    {
    case SDLK_RETURN:
        submitCommandBuffer();
        break;
    case SDLK_BACKSPACE:
        if (commandCaret_ > 0)
        {
            commandBuffer_.erase(commandCaret_ - 1, 1);
            --commandCaret_;
            audioSystem_.playKeyStroke();
        }
        break;
    case SDLK_DELETE:
        if (commandCaret_ < commandBuffer_.size())
        {
            commandBuffer_.erase(commandCaret_, 1);
        }
        break;
    case SDLK_LEFT:
        if (commandCaret_ > 0)
        {
            --commandCaret_;
        }
        break;
    case SDLK_RIGHT:
        if (commandCaret_ < commandBuffer_.size())
        {
            ++commandCaret_;
        }
        break;
    case SDLK_UP:
        moveCommandHistory(-1);
        break;
    case SDLK_DOWN:
        moveCommandHistory(1);
        break;
    default:
        break;
    }
}

void Application::handleMouseDown(int x, int y)
{
    if (mode_ == AppMode::MainMenu)
    {
        const Rect sidebar = menuSidebarRect();
        for (int i = 0; i < 5; ++i)
        {
            const Rect button{ sidebar.x + 14.0f, sidebar.y + 48.0f + static_cast<float>(i) * 58.0f, sidebar.w - 28.0f, 44.0f };
            if (!button.contains(static_cast<float>(x), static_cast<float>(y)))
            {
                continue;
            }
            switch (i)
            {
            case 0:
                startCurrentLevel();
                break;
            case 1:
                startTutorialLevel();
                break;
            case 2:
                toggleFullscreen(!fullscreenEnabled_);
                break;
            case 3:
                audioSystem_.setEnabled(!audioSystem_.enabled());
                pushLog(std::string("Audio ") + (audioSystem_.enabled() ? "enabled." : "muted."));
                break;
            case 4:
                running_ = false;
                break;
            }
            audioSystem_.playUiConfirm();
            return;
        }

        const Rect levelsRect = menuLevelsRect();
        for (std::size_t i = 0; i < levels_.size(); ++i)
        {
            const Rect row{ levelsRect.x + 14.0f, levelsRect.y + 42.0f + static_cast<float>(i) * 58.0f, levelsRect.w - 28.0f, 46.0f };
            if (row.contains(static_cast<float>(x), static_cast<float>(y)))
            {
                loadLevelById(levels_[i].id, true);
                startCurrentLevel();
                audioSystem_.playUiConfirm();
                return;
            }
        }
        return;
    }

    if (tutorialVisible_ && tutorialRect().contains(static_cast<float>(x), static_cast<float>(y)))
    {
        tutorialVisible_ = false;
        return;
    }

    const float divider = dividerX();
    if (static_cast<float>(x) > divider - 6.0f && static_cast<float>(x) < divider + 6.0f)
    {
        draggingDivider_ = true;
        return;
    }

    if (editorRect().contains(static_cast<float>(x), static_cast<float>(y)))
    {
        focus_ = FocusTarget::Editor;
        const float scale = 1.5f;
        const float lineHeight = renderer_.glyphHeight(scale) + 4.0f;
        const float localX = static_cast<float>(x) - (editorRect().x + 52.0f);
        const float localY = static_cast<float>(y) - (editorRect().y + 38.0f);
        const std::size_t row = static_cast<std::size_t>(std::max(0.0f, localY / lineHeight));
        const std::size_t col = static_cast<std::size_t>(std::max(0.0f, localX / renderer_.glyphWidth(scale)));
        editorBuffer_.setCaret(row, col);
        draggingSelection_ = true;
        return;
    }

    if (commandRect().contains(static_cast<float>(x), static_cast<float>(y)))
    {
        focus_ = FocusTarget::Command;
    }
}

void Application::handleMouseUp()
{
    draggingDivider_ = false;
    draggingSelection_ = false;
}

void Application::handleMouseMotion(int x, int y)
{
    if (mode_ == AppMode::MainMenu)
    {
        const Rect levelsRect = menuLevelsRect();
        hoveredLevelIndex_ = -1;
        for (std::size_t i = 0; i < levels_.size(); ++i)
        {
            const Rect row{ levelsRect.x + 14.0f, levelsRect.y + 42.0f + static_cast<float>(i) * 58.0f, levelsRect.w - 28.0f, 46.0f };
            if (row.contains(static_cast<float>(x), static_cast<float>(y)))
            {
                hoveredLevelIndex_ = static_cast<int>(i);
                break;
            }
        }
        return;
    }

    if (draggingDivider_)
    {
        const float availableWidth = static_cast<float>(renderer_.width()) - kPanelMargin * 2.0f;
        const float inspectorWidth = static_cast<float>(renderer_.width()) - static_cast<float>(x) - kPanelMargin;
        inspectorWidthRatio_ = std::clamp(inspectorWidth / availableWidth, 0.22f, 0.45f);
        return;
    }

    if (draggingSelection_)
    {
        const float scale = 1.5f;
        const float lineHeight = renderer_.glyphHeight(scale) + 4.0f;
        const float localX = static_cast<float>(x) - (editorRect().x + 52.0f);
        const float localY = static_cast<float>(y) - (editorRect().y + 38.0f);
        const std::size_t row = static_cast<std::size_t>(std::max(0.0f, localY / lineHeight));
        const std::size_t col = static_cast<std::size_t>(std::max(0.0f, localX / renderer_.glyphWidth(scale)));
        editorBuffer_.setCaretWithSelection(row, col, true);
    }
}

void Application::submitCommandBuffer()
{
    executeCommand(commandBuffer_, false);
    if (!trim(commandBuffer_).empty())
    {
        commandHistory_.push_back(commandBuffer_);
    }
    commandBuffer_.clear();
    commandCaret_ = 0;
    commandHistoryIndex_ = -1;
}

void Application::submitMenuCommand()
{
    executeCommand(menuCommandBuffer_, true);
    if (!trim(menuCommandBuffer_).empty())
    {
        menuCommandHistory_.push_back(menuCommandBuffer_);
    }
    menuCommandBuffer_.clear();
    menuCommandCaret_ = 0;
    menuCommandHistoryIndex_ = -1;
}

void Application::moveCommandHistory(int delta)
{
    std::vector<std::string>& history = activeHistory();
    int& historyIndex = activeHistoryIndex();
    std::string& buffer = activeCommandBuffer();
    std::size_t& caret = activeCommandCaret();
    if (history.empty())
    {
        return;
    }

    if (delta < 0)
    {
        if (historyIndex < 0)
        {
            historyIndex = static_cast<int>(history.size()) - 1;
        }
        else if (historyIndex > 0)
        {
            --historyIndex;
        }
        buffer = history[static_cast<std::size_t>(historyIndex)];
    }
    else if (historyIndex >= 0)
    {
        if (historyIndex + 1 < static_cast<int>(history.size()))
        {
            ++historyIndex;
            buffer = history[static_cast<std::size_t>(historyIndex)];
        }
        else
        {
            historyIndex = -1;
            buffer.clear();
        }
    }
    caret = buffer.size();
}

void Application::executeCommand(const std::string& commandLine, bool fromMenu)
{
    const std::string trimmed = trim(commandLine);
    if (trimmed.empty())
    {
        return;
    }

    pushLog("> " + trimmed);

    const std::vector<std::string> parts = parseArguments(trimmed);
    if (parts.empty())
    {
        return;
    }
    const std::string command = toLower(parts.front());

    if (command == "help")
    {
        tutorialHelpSeen_ = true;
        if (parts.size() == 1)
        {
            pushLog("Commands:");
            for (const CommandSpec& spec : kCommands)
            {
                pushLog(std::string("- ") + spec.usage + " : " + spec.summary);
            }
            pushLog("Topics: help comments, help memory, help registers, help instructions, help in, help add");
            return;
        }

        const std::string topic = toLower(parts[1]);
        for (const std::string& line : helpLinesForTopic(topic))
        {
            pushLog(line);
        }
        return;
    }

    if (command == "levels")
    {
        for (const LevelDefinition& level : levels_)
        {
            pushLog(level.id + " - " + level.title);
        }
        return;
    }

    if (command == "load")
    {
        if (parts.size() < 2)
        {
            pushLog("Usage: load <id>");
            return;
        }
        if (!loadLevelById(parts[1], true))
        {
            pushLog("Level not found: " + parts[1]);
            audioSystem_.playUiError();
            return;
        }
        if (fromMenu)
        {
            startCurrentLevel();
        }
        audioSystem_.playUiConfirm();
        return;
    }

    if (command == "assemble")
    {
        assembleSource(true);
        return;
    }

    if (command == "run")
    {
        runMachine();
        return;
    }

    if (command == "step")
    {
        int steps = 1;
        if (parts.size() >= 2)
        {
            steps = std::max(1, std::atoi(parts[1].c_str()));
        }
        stepMachine(steps);
        return;
    }

    if (command == "reset")
    {
        resetMachine(true);
        return;
    }

    if (command == "theme")
    {
        if (parts.size() < 2)
        {
            pushLog("Usage: theme amber|green");
            return;
        }
        const std::string value = toLower(parts[1]);
        if (value == "amber")
        {
            theme_ = Theme::amber();
            pushLog("Theme set to amber phosphor.");
        }
        else if (value == "green")
        {
            theme_ = Theme::green();
            pushLog("Theme set to green phosphor.");
        }
        else
        {
            pushLog("Unknown theme: " + value);
        }
        return;
    }

    if (command == "crt")
    {
        bool value = crtEnabled_;
        if (parts.size() < 2 || !parseBoolToken(parts[1], crtEnabled_, value))
        {
            pushLog("Usage: crt on|off");
            return;
        }
        crtEnabled_ = value;
        pushLog(std::string("CRT effects ") + (crtEnabled_ ? "enabled." : "disabled."));
        return;
    }

    if (command == "scanlines")
    {
        bool value = scanlinesEnabled_;
        if (parts.size() < 2 || !parseBoolToken(parts[1], scanlinesEnabled_, value))
        {
            pushLog("Usage: scanlines on|off");
            return;
        }
        scanlinesEnabled_ = value;
        pushLog(std::string("Scanlines ") + (scanlinesEnabled_ ? "enabled." : "disabled."));
        return;
    }

    if (command == "audio")
    {
        bool value = audioSystem_.enabled();
        if (parts.size() < 2 || !parseBoolToken(parts[1], audioSystem_.enabled(), value))
        {
            pushLog("Usage: audio on|off");
            return;
        }
        audioSystem_.setEnabled(value);
        pushLog(std::string("Audio ") + (value ? "enabled." : "muted."));
        return;
    }

    if (command == "fullscreen")
    {
        bool value = fullscreenEnabled_;
        if (parts.size() >= 2)
        {
            if (!parseBoolToken(parts[1], fullscreenEnabled_, value))
            {
                pushLog("Usage: fullscreen on|off|toggle");
                return;
            }
        }
        else
        {
            value = !fullscreenEnabled_;
        }
        toggleFullscreen(value);
        return;
    }

    if (command == "tutorial")
    {
        const std::string action = parts.size() >= 2 ? toLower(parts[1]) : "show";
        if (action == "show")
        {
            toggleTutorial(true);
        }
        else if (action == "hide")
        {
            toggleTutorial(false);
        }
        else if (action == "replay")
        {
            tutorialCommentSeen_ = false;
            tutorialHelpSeen_ = false;
            tutorialStepSeen_ = false;
            tutorialRunSeen_ = false;
            tutorialStage_ = TutorialStage::Welcome;
            toggleTutorial(true);
        }
        else
        {
            pushLog("Usage: tutorial show|hide|replay");
        }
        return;
    }

    if (command == "menu")
    {
        openMainMenu();
        return;
    }

    if (command == "continue" || command == "play")
    {
        startCurrentLevel();
        return;
    }

    if (command == "quit")
    {
        running_ = false;
        return;
    }

    pushLog("Unknown command. Try 'help' or 'help <command>'.");
    audioSystem_.playUiError();
}

bool Application::assembleSource(bool logSuccess)
{
    if (currentLevel_ == nullptr)
    {
        pushLog("No level loaded.");
        return false;
    }

    const AssembleResult result = assembler_.assemble(editorBuffer_.toString(), currentLevel_->allowedInstructions);
    if (!result.success)
    {
        for (const AssemblerError& error : result.errors)
        {
            pushLog("Line " + std::to_string(error.line) + ": " + error.message);
        }
        programLoaded_ = false;
        lastValidationMessage_ = "Assembly failed.";
        levelSolved_ = false;
        audioSystem_.playUiError();
        return false;
    }

    program_ = result.program;
    machine_.loadProgram(program_, currentLevel_->memoryWords, currentLevel_->inputValues, currentLevel_->maxCycles);
    programLoaded_ = true;
    editorBuffer_.clearDirty();
    lastValidationMessage_ = "Program assembled.";
    levelSolved_ = false;

    if (logSuccess)
    {
        pushLog("Assembly successful. " + std::to_string(program_.instructions.size()) + " instructions ready.");
    }
    audioSystem_.playUiConfirm();
    return true;
}

void Application::stepMachine(int steps)
{
    if (currentLevel_ == nullptr)
    {
        return;
    }
    if (!programLoaded_ || editorBuffer_.dirty())
    {
        if (!assembleSource(true))
        {
            return;
        }
    }

    for (int i = 0; i < steps; ++i)
    {
        const StepResult result = machine_.step();
        tutorialStepSeen_ = true;
        if (result == StepResult::Error)
        {
            pushLog("Machine fault: " + machine_.lastError());
            lastValidationMessage_ = machine_.lastError();
            levelSolved_ = false;
            audioSystem_.playUiError();
            return;
        }
        if (result == StepResult::Halted)
        {
            validateOutputs(false);
            return;
        }
        lastValidationMessage_ = "Stepped instruction at IP " + std::to_string(machine_.ip());
    }
    audioSystem_.playUiMove();
}

void Application::runMachine()
{
    if (currentLevel_ == nullptr)
    {
        return;
    }
    if (!programLoaded_ || editorBuffer_.dirty())
    {
        if (!assembleSource(true))
        {
            return;
        }
    }

    const RunSummary summary = machine_.runToEnd();
    tutorialRunSeen_ = true;
    if (!summary.success)
    {
        pushLog("Run failed: " + summary.message);
        lastValidationMessage_ = summary.message;
        levelSolved_ = false;
        audioSystem_.playUiError();
        return;
    }

    validateOutputs(true);
}

void Application::resetMachine(bool logAction)
{
    if (currentLevel_ == nullptr)
    {
        return;
    }
    if (programLoaded_)
    {
        machine_.reset(currentLevel_->inputValues);
    }
    else
    {
        machine_.loadProgram({}, currentLevel_->memoryWords, currentLevel_->inputValues, currentLevel_->maxCycles);
        machine_.reset(currentLevel_->inputValues);
    }
    lastValidationMessage_ = "Machine reset.";
    levelSolved_ = false;
    if (logAction)
    {
        pushLog("Machine reset.");
    }
}

bool Application::loadLevelById(const std::string& id, bool logAction)
{
    auto it = std::find_if(levels_.begin(), levels_.end(), [&](const LevelDefinition& level) {
        return toLower(level.id) == toLower(id);
    });
    if (it == levels_.end())
    {
        return false;
    }

    currentLevel_ = &(*it);
    editorBuffer_.setText(currentLevel_->starterSource);
    programLoaded_ = false;
    machine_.loadProgram({}, currentLevel_->memoryWords, currentLevel_->inputValues, currentLevel_->maxCycles);
    machine_.reset(currentLevel_->inputValues);
    lastValidationMessage_ = "Level ready.";
    levelSolved_ = false;
    tutorialCommentSeen_ = currentLevel_->starterSource.find(';') != std::string::npos;
    tutorialHelpSeen_ = false;
    tutorialStepSeen_ = false;
    tutorialRunSeen_ = false;
    tutorialStage_ = TutorialStage::Welcome;
    tutorialVisible_ = currentLevel_->id == "signal_boost";
    if (logAction)
    {
        pushLog("Loaded level '" + currentLevel_->title + "'.");
    }
    return true;
}

void Application::validateOutputs(bool fromRun)
{
    if (currentLevel_ == nullptr)
    {
        return;
    }

    if (machine_.outputQueue() == currentLevel_->expectedValues)
    {
        levelSolved_ = true;
        std::ostringstream message;
        if (!currentLevel_->successText.empty())
        {
            message << currentLevel_->successText << ' ';
        }
        message << "Solved in " << machine_.cycles() << " cycles, memory high-water " << machine_.memoryHighWater() << " words.";
        lastValidationMessage_ = message.str();
        pushLog(lastValidationMessage_);
        tutorialStage_ = TutorialStage::Complete;
        tutorialVisible_ = currentLevel_->id == "signal_boost";
        audioSystem_.playUiConfirm();
    }
    else
    {
        levelSolved_ = false;
        lastValidationMessage_ = "Output mismatch. Expected [" + joinWords(currentLevel_->expectedValues) + "] but got [" + joinWords(machine_.outputQueue()) + "].";
        if (fromRun)
        {
            pushLog(lastValidationMessage_);
        }
        audioSystem_.playUiError();
    }
}

void Application::openMainMenu()
{
    mode_ = AppMode::MainMenu;
}

void Application::startCurrentLevel()
{
    mode_ = AppMode::Playing;
    focus_ = FocusTarget::Editor;
}

void Application::startTutorialLevel()
{
    if (!levels_.empty())
    {
        loadLevelById(levels_.front().id, true);
    }
    mode_ = AppMode::Playing;
    tutorialVisible_ = true;
}

void Application::toggleTutorial(bool enabled)
{
    tutorialVisible_ = enabled;
    if (enabled)
    {
        mode_ = AppMode::Playing;
    }
}

void Application::updateTutorialProgress()
{
    if (currentLevel_ == nullptr || currentLevel_->id != "signal_boost" || levelSolved_)
    {
        return;
    }
    if (!tutorialCommentSeen_ && editorBuffer_.toString().find(';') != std::string::npos)
    {
        tutorialCommentSeen_ = true;
    }

    if (!tutorialCommentSeen_)
    {
        tutorialStage_ = TutorialStage::Comments;
    }
    else if (!tutorialHelpSeen_)
    {
        tutorialStage_ = TutorialStage::Help;
    }
    else if (!tutorialStepSeen_)
    {
        tutorialStage_ = TutorialStage::Step;
    }
    else if (!tutorialRunSeen_)
    {
        tutorialStage_ = TutorialStage::Run;
    }
    else
    {
        tutorialStage_ = TutorialStage::Run;
    }
}

void Application::toggleFullscreen(bool enabled)
{
    if (window_ == nullptr || enabled == fullscreenEnabled_)
    {
        return;
    }

    if (!enabled)
    {
        SDL_SetWindowFullscreen(window_, 0);
        SDL_SetWindowSize(window_, windowedWidth_, windowedHeight_);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        fullscreenEnabled_ = false;
        pushLog("Fullscreen disabled.");
        return;
    }

    SDL_DisplayMode primaryMode{};
    if (SDL_GetCurrentDisplayMode(0, &primaryMode) != 0)
    {
        pushLog(std::string("Could not read primary display mode: ") + SDL_GetError());
        return;
    }

    SDL_SetWindowDisplayMode(window_, &primaryMode);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0));
    if (SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN) != 0)
    {
        pushLog(std::string("Exclusive fullscreen failed: ") + SDL_GetError());
        return;
    }
    fullscreenEnabled_ = true;
    pushLog("Fullscreen enabled.");
}

std::vector<std::string> Application::parseArguments(const std::string& text) const
{
    std::vector<std::string> parts;
    std::string current;
    bool quoted = false;
    for (char ch : text)
    {
        if (ch == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) != 0 && !quoted)
        {
            if (!current.empty())
            {
                parts.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty())
    {
        parts.push_back(current);
    }
    return parts;
}

std::string Application::currentCommandBuffer() const
{
    return mode_ == AppMode::MainMenu ? menuCommandBuffer_ : commandBuffer_;
}

std::size_t Application::currentCommandCaret() const
{
    return mode_ == AppMode::MainMenu ? menuCommandCaret_ : commandCaret_;
}

void Application::setCurrentCommandCaret(std::size_t caret)
{
    if (mode_ == AppMode::MainMenu)
    {
        menuCommandCaret_ = caret;
    }
    else
    {
        commandCaret_ = caret;
    }
}

std::string& Application::activeCommandBuffer()
{
    return mode_ == AppMode::MainMenu ? menuCommandBuffer_ : commandBuffer_;
}

std::size_t& Application::activeCommandCaret()
{
    return mode_ == AppMode::MainMenu ? menuCommandCaret_ : commandCaret_;
}

int& Application::activeHistoryIndex()
{
    return mode_ == AppMode::MainMenu ? menuCommandHistoryIndex_ : commandHistoryIndex_;
}

const std::vector<std::string>& Application::activeHistory() const
{
    return mode_ == AppMode::MainMenu ? menuCommandHistory_ : commandHistory_;
}

std::vector<std::string>& Application::activeHistory()
{
    return mode_ == AppMode::MainMenu ? menuCommandHistory_ : commandHistory_;
}

Rect Application::menuDesktopRect() const
{
    return { kPanelMargin, kPanelMargin, static_cast<float>(renderer_.width()) - kPanelMargin * 2.0f, static_cast<float>(renderer_.height()) - kPanelMargin * 2.0f };
}

Rect Application::menuSidebarRect() const
{
    const Rect desktop = menuDesktopRect();
    return { desktop.x + 14.0f, desktop.y + 48.0f, 280.0f, desktop.h - 62.0f };
}

Rect Application::menuTerminalRect() const
{
    const Rect desktop = menuDesktopRect();
    return { desktop.x + 308.0f, desktop.y + desktop.h - 250.0f, desktop.w - 322.0f, 236.0f };
}

Rect Application::menuLevelsRect() const
{
    const Rect desktop = menuDesktopRect();
    return { desktop.x + 308.0f, desktop.y + 48.0f, desktop.w - 322.0f, desktop.h - 312.0f };
}

Rect Application::tutorialRect() const
{
    const float width = std::min(680.0f, static_cast<float>(renderer_.width()) - 80.0f);
    const float height = std::min(340.0f, static_cast<float>(renderer_.height()) - 120.0f);
    return { (static_cast<float>(renderer_.width()) - width) * 0.5f, (static_cast<float>(renderer_.height()) - height) * 0.5f, width, height };
}

void Application::pushLog(const std::string& line)
{
    const std::vector<std::string> wrapped = wrapText(line, (mode_ == AppMode::MainMenu ? menuTerminalRect().w : consoleRect().w) - 32.0f, 1.0f);
    logLines_.insert(logLines_.end(), wrapped.begin(), wrapped.end());
    constexpr std::size_t maxLogLines = 256;
    if (logLines_.size() > maxLogLines)
    {
        logLines_.erase(logLines_.begin(), logLines_.begin() + static_cast<std::ptrdiff_t>(logLines_.size() - maxLogLines));
    }
}

std::vector<std::string> Application::wrapText(const std::string& text, float maxWidth, float scale) const
{
    std::vector<std::string> result;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(1.0f, maxWidth / renderer_.glyphWidth(scale)));
    std::stringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph, '\n'))
    {
        std::stringstream stream(paragraph);
        std::string word;
        std::string current;
        while (stream >> word)
        {
            if (current.empty())
            {
                current = word;
            }
            else if (current.size() + 1 + word.size() <= maxChars)
            {
                current += ' ';
                current += word;
            }
            else
            {
                result.push_back(current);
                current = word;
            }
        }
        if (!current.empty())
        {
            result.push_back(current);
        }
        else if (paragraph.empty())
        {
            result.push_back("");
        }
    }
    if (result.empty())
    {
        result.push_back("");
    }
    return result;
}

std::string Application::formatWord(std::uint16_t value) const
{
    std::ostringstream stream;
    stream.setf(std::ios::hex, std::ios::basefield);
    stream.setf(std::ios::uppercase);
    stream.width(4);
    stream.fill('0');
    stream << value;
    return stream.str();
}

std::string Application::joinWords(const std::vector<std::uint16_t>& values) const
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

std::vector<std::string> Application::splitParagraphs(const std::string& text) const
{
    std::vector<std::string> paragraphs;
    std::stringstream stream(text);
    std::string line;
    std::string current;
    while (std::getline(stream, line))
    {
        if (trim(line).empty())
        {
            if (!current.empty())
            {
                paragraphs.push_back(trim(current));
                current.clear();
            }
            continue;
        }
        if (!current.empty())
        {
            current += ' ';
        }
        current += trim(line);
    }
    if (!current.empty())
    {
        paragraphs.push_back(trim(current));
    }
    if (paragraphs.empty() && !text.empty())
    {
        paragraphs.push_back(text);
    }
    return paragraphs;
}

std::vector<std::string> Application::helpLinesForTopic(const std::string& topic) const
{
    if (const CommandSpec* spec = findCommandSpec(topic))
    {
        return {
            std::string(spec->usage) + " - " + spec->summary,
            spec->details,
        };
    }

    if (topic == "comments")
    {
        return {
            "Comments start with ';'.",
            "The assembler ignores everything from ';' to the end of the line, so comments are safe notes for humans.",
        };
    }
    if (topic == "memory")
    {
        return {
            "Memory is a small 16-bit word array.",
            "LOAD reads from [addr], STORE writes to [addr], and the inspector shows how much memory your program has touched.",
        };
    }
    if (topic == "registers")
    {
        return {
            "Registers A, B, C, and D hold working values.",
            "SP starts at the end of memory, and IP is the instruction pointer shown in the inspector.",
        };
    }
    if (topic == "instructions")
    {
        return {
            "Assembly instructions are the operations your program can execute.",
            "Try help in, help out, help add, help sub, help cmp, help jmp, help jz, or help halt for specific instruction details.",
        };
    }
    if (topic == "in")
    {
        return {
            "IN reg, port - read a value from an input port into a register.",
            "Example: IN A, 0 reads the next value from input port 0 into register A.",
            "In the current prototype only input port 0 exists.",
        };
    }
    if (topic == "out")
    {
        return {
            "OUT port, value - send a value to an output port.",
            "Example: OUT 1, A writes the current value of register A to output port 1.",
            "In the current prototype only output port 1 exists.",
        };
    }
    if (topic == "add")
    {
        return {
            "ADD reg, value - add a number or register value into a register.",
            "Example: ADD A, 5 increases A by five.",
            "This is useful for offsets, counters, and transforming input before you write output.",
        };
    }
    if (topic == "sub")
    {
        return {
            "SUB reg, value - subtract a number or register value from a register.",
            "Example: SUB A, 1 decreases A by one.",
        };
    }
    if (topic == "cmp")
    {
        return {
            "CMP reg, value - compare a register against a value and update the zero flag.",
            "Use it before JZ or JNZ to control loops and branches.",
        };
    }
    if (topic == "jmp")
    {
        return {
            "JMP label - jump to another instruction unconditionally.",
            "Use labels like loop: and then JMP loop to repeat work.",
        };
    }
    if (topic == "jz")
    {
        return {
            "JZ label - jump when the zero flag is set.",
            "A common pattern is CMP A, 0 followed by JZ done.",
        };
    }
    if (topic == "jnz")
    {
        return {
            "JNZ label - jump when the zero flag is not set.",
            "Use it to keep looping until a comparison becomes equal.",
        };
    }
    if (topic == "load")
    {
        return {
            "load <id> - load a puzzle level by id.",
            "If you wanted the instruction LOAD, type help instructions or help memory. The current starter levels do not use the LOAD opcode yet.",
        };
    }
    if (topic == "halt")
    {
        return {
            "HALT - stop program execution cleanly.",
            "Most levels expect your program to halt after writing the correct output.",
        };
    }

    return { "Unknown help topic: " + topic };
}

Rect Application::editorRect() const
{
    const float left = kPanelMargin;
    const float top = kPanelMargin + kHeaderHeight + 14.0f;
    const float width = dividerX() - left - 12.0f;
    const float height = static_cast<float>(renderer_.height()) - top - kPanelMargin - kConsoleHeight - kCommandHeight - 18.0f;
    return { left, top, width, height };
}

Rect Application::consoleRect() const
{
    const Rect editor = editorRect();
    return { editor.x, editor.y + editor.h + 12.0f, editor.w, kConsoleHeight };
}

Rect Application::commandRect() const
{
    const Rect console = consoleRect();
    return { console.x, console.y + console.h + 8.0f, console.w, kCommandHeight };
}

Rect Application::inspectorRect() const
{
    const float width = (static_cast<float>(renderer_.width()) - kPanelMargin * 2.0f) * inspectorWidthRatio_;
    return {
        static_cast<float>(renderer_.width()) - kPanelMargin - width,
        kPanelMargin + kHeaderHeight + 14.0f,
        width,
        static_cast<float>(renderer_.height()) - (kPanelMargin * 2.0f + kHeaderHeight + 14.0f),
    };
}

float Application::dividerX() const
{
    return inspectorRect().x - 12.0f;
}
