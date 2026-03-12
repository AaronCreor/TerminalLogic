#pragma once

#include <cstddef>
#include <string>
#include <vector>

class TextBuffer
{
public:
    void setText(const std::string& text);
    std::string toString() const;

    void insertCharacter(char value);
    void insertText(const std::string& text);
    void newLine();
    void backspace();
    void deleteForward();

    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveHome();
    void moveEnd();

    void setCaret(std::size_t row, std::size_t column);

    const std::vector<std::string>& lines() const { return lines_; }
    std::size_t caretRow() const { return caretRow_; }
    std::size_t caretColumn() const { return caretColumn_; }

    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

private:
    void clampCaret();

    std::vector<std::string> lines_ = { "" };
    std::size_t caretRow_ = 0;
    std::size_t caretColumn_ = 0;
    bool dirty_ = true;
};
