#pragma once

#include <cstddef>
#include <string>
#include <vector>

class TextBuffer
{
public:
    struct Position
    {
        std::size_t row = 0;
        std::size_t column = 0;

        bool operator==(const Position& other) const
        {
            return row == other.row && column == other.column;
        }

        bool operator<(const Position& other) const
        {
            return row < other.row || (row == other.row && column < other.column);
        }
    };

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
    void moveLeftSelecting();
    void moveRightSelecting();
    void moveUpSelecting();
    void moveDownSelecting();
    void moveHomeSelecting();
    void moveEndSelecting();

    void setCaret(std::size_t row, std::size_t column);
    void setCaretWithSelection(std::size_t row, std::size_t column, bool selecting);

    bool hasSelection() const;
    Position selectionStart() const;
    Position selectionEnd() const;
    std::string selectedText() const;
    void clearSelection();
    bool deleteSelection();
    bool copySelectionToClipboard() const;
    bool cutSelectionToClipboard();
    bool pasteFromClipboard();
    bool undo();

    const std::vector<std::string>& lines() const { return lines_; }
    std::size_t caretRow() const { return caretRow_; }
    std::size_t caretColumn() const { return caretColumn_; }

    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

private:
    struct Snapshot
    {
        std::vector<std::string> lines;
        std::size_t caretRow = 0;
        std::size_t caretColumn = 0;
        bool hasSelection = false;
        Position selectionAnchor;
        Position selectionCaret;
    };

    void clampCaret();
    void beginSelection();
    void updateSelectionAfterMove(bool selecting);
    void recordUndoState();
    void restoreSnapshot(const Snapshot& snapshot);
    Position caretPosition() const;

    std::vector<std::string> lines_ = { "" };
    std::size_t caretRow_ = 0;
    std::size_t caretColumn_ = 0;
    bool hasSelection_ = false;
    Position selectionAnchor_;
    Position selectionCaret_;
    bool dirty_ = true;
    std::vector<Snapshot> undoStack_;
};
