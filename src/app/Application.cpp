#include "app/Application.h"

#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 900;
constexpr float kPanelMargin = 20.0f;
constexpr float kHeaderHeight = 64.0f;
constexpr float kCommandHeight = 40.0f;
constexpr float kConsoleHeight = 220.0f;

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

std::vector<std::string> splitSpaces(const std::string& text)
{
    std::stringstream stream(text);
    std::vector<std::string> parts;
    std::string part;
    while (stream >> part)
    {
        parts.push_back(part);
    }
    return parts;
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
    if (!loadLevels())
    {
        return false;
    }

    pushLog("AsmLab boot sequence complete.");
    pushLog("Press Tab to switch focus. Use F5 to run and F10 to step.");
    executeCommand("help");

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
        "AsmLab",
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
            renderer_.resize(event.window.data1, event.window.data2);
        }
        break;
    default:
        break;
    }
}

void Application::update()
{
}

void Application::render()
{
    renderer_.beginFrame(theme_);

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
    renderer_.drawText(header.x + header.w - 220.0f, header.y + 40.0f,
        "cycles " + std::to_string(machine_.cycles()) + "/" + std::to_string(machine_.cycleLimit()),
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
    for (int i = 0; i < maxEditorLines && i < static_cast<int>(editorBuffer_.lines().size()); ++i)
    {
        std::ostringstream label;
        label << (i + 1);
        renderer_.drawText(editorStartX, editorStartY + i * editorLineHeight, label.str(), 1.0f, theme_.textMuted);
        renderer_.drawText(editorStartX + 36.0f, editorStartY + i * editorLineHeight, editorBuffer_.lines()[i], editorScale, theme_.text);
    }

    if (focus_ == FocusTarget::Editor && (SDL_GetTicks() / 500) % 2 == 0)
    {
        const float caretX = editorStartX + 36.0f + static_cast<float>(editorBuffer_.caretColumn()) * renderer_.glyphWidth(editorScale);
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
        for (const std::string& line : wrapText(currentLevel_->description, inspector.w - 28.0f, 1.0f))
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

    renderer_.endFrame(crtEnabled_, scanlinesEnabled_, curvatureEnabled_);
}

void Application::handleTextInput(const char* text)
{
    if (focus_ == FocusTarget::Editor)
    {
        editorBuffer_.insertText(text);
    }
    else
    {
        commandBuffer_.insert(commandCaret_, text);
        commandCaret_ += SDL_strlen(text);
    }
}

void Application::handleKeyDown(const SDL_KeyboardEvent& event)
{
    if (event.keysym.sym == SDLK_ESCAPE)
    {
        running_ = false;
        return;
    }
    if (event.keysym.sym == SDLK_TAB)
    {
        focus_ = focus_ == FocusTarget::Editor ? FocusTarget::Command : FocusTarget::Editor;
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
        switch (event.keysym.sym)
        {
        case SDLK_RETURN:
            editorBuffer_.newLine();
            break;
        case SDLK_BACKSPACE:
            editorBuffer_.backspace();
            break;
        case SDLK_DELETE:
            editorBuffer_.deleteForward();
            break;
        case SDLK_LEFT:
            editorBuffer_.moveLeft();
            break;
        case SDLK_RIGHT:
            editorBuffer_.moveRight();
            break;
        case SDLK_UP:
            editorBuffer_.moveUp();
            break;
        case SDLK_DOWN:
            editorBuffer_.moveDown();
            break;
        case SDLK_HOME:
            editorBuffer_.moveHome();
            break;
        case SDLK_END:
            editorBuffer_.moveEnd();
            break;
        default:
            break;
        }
        return;
    }

    switch (event.keysym.sym)
    {
    case SDLK_RETURN:
        executeCommand(commandBuffer_);
        if (!trim(commandBuffer_).empty())
        {
            commandHistory_.push_back(commandBuffer_);
        }
        commandBuffer_.clear();
        commandCaret_ = 0;
        commandHistoryIndex_ = -1;
        break;
    case SDLK_BACKSPACE:
        if (commandCaret_ > 0)
        {
            commandBuffer_.erase(commandCaret_ - 1, 1);
            --commandCaret_;
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
        if (!commandHistory_.empty())
        {
            if (commandHistoryIndex_ < 0)
            {
                commandHistoryIndex_ = static_cast<int>(commandHistory_.size()) - 1;
            }
            else if (commandHistoryIndex_ > 0)
            {
                --commandHistoryIndex_;
            }
            commandBuffer_ = commandHistory_[static_cast<std::size_t>(commandHistoryIndex_)];
            commandCaret_ = commandBuffer_.size();
        }
        break;
    case SDLK_DOWN:
        if (commandHistoryIndex_ >= 0)
        {
            if (commandHistoryIndex_ + 1 < static_cast<int>(commandHistory_.size()))
            {
                ++commandHistoryIndex_;
                commandBuffer_ = commandHistory_[static_cast<std::size_t>(commandHistoryIndex_)];
            }
            else
            {
                commandHistoryIndex_ = -1;
                commandBuffer_.clear();
            }
            commandCaret_ = commandBuffer_.size();
        }
        break;
    default:
        break;
    }
}

void Application::handleMouseDown(int x, int y)
{
    const float divider = dividerX();
    if (x > divider - 6.0f && x < divider + 6.0f)
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
}

void Application::handleMouseMotion(int x, int /*y*/)
{
    if (!draggingDivider_)
    {
        return;
    }

    const float availableWidth = static_cast<float>(renderer_.width()) - kPanelMargin * 2.0f;
    const float inspectorWidth = static_cast<float>(renderer_.width()) - static_cast<float>(x) - kPanelMargin;
    inspectorWidthRatio_ = std::clamp(inspectorWidth / availableWidth, 0.22f, 0.45f);
}

void Application::executeCommand(const std::string& commandLine)
{
    const std::string trimmed = trim(commandLine);
    if (trimmed.empty())
    {
        return;
    }

    pushLog("> " + trimmed);

    const std::vector<std::string> parts = splitSpaces(trimmed);
    const std::string command = toLower(parts.front());

    if (command == "help")
    {
        pushLog("Commands: help, levels, load <id>, assemble, run, step, reset, theme amber|green, crt on|off, scanlines on|off, curvature on|off, quit");
    }
    else if (command == "levels")
    {
        for (const LevelDefinition& level : levels_)
        {
            pushLog(level.id + " - " + level.title);
        }
    }
    else if (command == "load" && parts.size() >= 2)
    {
        if (!loadLevelById(parts[1], true))
        {
            pushLog("Level not found: " + parts[1]);
        }
    }
    else if (command == "assemble")
    {
        assembleSource(true);
    }
    else if (command == "run")
    {
        runMachine();
    }
    else if (command == "step")
    {
        stepMachine();
    }
    else if (command == "reset")
    {
        resetMachine(true);
    }
    else if (command == "theme" && parts.size() >= 2)
    {
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
    }
    else if (command == "crt" && parts.size() >= 2)
    {
        crtEnabled_ = toLower(parts[1]) == "on";
        pushLog(std::string("CRT effects ") + (crtEnabled_ ? "enabled." : "disabled."));
    }
    else if (command == "scanlines" && parts.size() >= 2)
    {
        scanlinesEnabled_ = toLower(parts[1]) == "on";
        pushLog(std::string("Scanlines ") + (scanlinesEnabled_ ? "enabled." : "disabled."));
    }
    else if (command == "curvature" && parts.size() >= 2)
    {
        curvatureEnabled_ = toLower(parts[1]) == "on";
        pushLog(std::string("Curvature mask ") + (curvatureEnabled_ ? "enabled." : "disabled."));
    }
    else if (command == "quit")
    {
        running_ = false;
    }
    else
    {
        pushLog("Unknown command. Try 'help'.");
    }
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
    return true;
}

void Application::stepMachine()
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

    const StepResult result = machine_.step();
    if (result == StepResult::Error)
    {
        pushLog("Machine fault: " + machine_.lastError());
        lastValidationMessage_ = machine_.lastError();
        levelSolved_ = false;
    }
    else if (result == StepResult::Halted)
    {
        validateOutputs(false);
    }
    else
    {
        lastValidationMessage_ = "Stepped instruction at IP " + std::to_string(machine_.ip());
    }
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
    if (!summary.success)
    {
        pushLog("Run failed: " + summary.message);
        lastValidationMessage_ = summary.message;
        levelSolved_ = false;
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
        message << "Solved in " << machine_.cycles() << " cycles, memory high-water " << machine_.memoryHighWater() << " words.";
        lastValidationMessage_ = message.str();
        pushLog(lastValidationMessage_);
    }
    else
    {
        levelSolved_ = false;
        lastValidationMessage_ = "Output mismatch. Expected [" + joinWords(currentLevel_->expectedValues) + "] but got [" + joinWords(machine_.outputQueue()) + "].";
        if (fromRun)
        {
            pushLog(lastValidationMessage_);
        }
    }
}

void Application::pushLog(const std::string& line)
{
    const std::vector<std::string> wrapped = wrapText(line, consoleRect().w - 32.0f, 1.0f);
    logLines_.insert(logLines_.end(), wrapped.begin(), wrapped.end());
    constexpr std::size_t maxLogLines = 256;
    if (logLines_.size() > maxLogLines)
    {
        logLines_.erase(logLines_.begin(), logLines_.begin() + static_cast<std::ptrdiff_t>(logLines_.size() - maxLogLines));
    }
}

std::vector<std::string> Application::wrapText(const std::string& text, float maxWidth, float scale) const
{
    const std::size_t maxChars = static_cast<std::size_t>(std::max(1.0f, maxWidth / renderer_.glyphWidth(scale)));
    std::stringstream stream(text);
    std::vector<std::string> lines;
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
            lines.push_back(current);
            current = word;
        }
    }
    if (!current.empty())
    {
        lines.push_back(current);
    }
    if (lines.empty())
    {
        lines.push_back("");
    }
    return lines;
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
