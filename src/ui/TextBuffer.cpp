#include "ui/TextBuffer.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <sstream>

void TextBuffer::setText(const std::string& text)
{
    undoStack_.clear();
    lines_.clear();
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines_.push_back(line);
    }
    if (lines_.empty())
    {
        lines_.push_back("");
    }
    caretRow_ = 0;
    caretColumn_ = 0;
    hasSelection_ = false;
    dirty_ = true;
    clampCaret();
}

std::string TextBuffer::toString() const
{
    std::ostringstream output;
    for (std::size_t i = 0; i < lines_.size(); ++i)
    {
        output << lines_[i];
        if (i + 1 < lines_.size())
        {
            output << '\n';
        }
    }
    return output.str();
}

void TextBuffer::insertCharacter(char value)
{
    recordUndoState();
    deleteSelection();
    lines_[caretRow_].insert(lines_[caretRow_].begin() + static_cast<std::ptrdiff_t>(caretColumn_), value);
    ++caretColumn_;
    clearSelection();
    dirty_ = true;
}

void TextBuffer::insertText(const std::string& text)
{
    for (char ch : text)
    {
        if (ch == '\n')
        {
            newLine();
        }
        else if (ch != '\r')
        {
            insertCharacter(ch);
        }
    }
}

void TextBuffer::newLine()
{
    recordUndoState();
    deleteSelection();
    std::string tail = lines_[caretRow_].substr(caretColumn_);
    lines_[caretRow_].erase(caretColumn_);
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(caretRow_ + 1), tail);
    ++caretRow_;
    caretColumn_ = 0;
    clearSelection();
    dirty_ = true;
}

void TextBuffer::backspace()
{
    if (hasSelection())
    {
        recordUndoState();
        deleteSelection();
        dirty_ = true;
        return;
    }

    if (caretColumn_ > 0)
    {
        recordUndoState();
        lines_[caretRow_].erase(caretColumn_ - 1, 1);
        --caretColumn_;
        dirty_ = true;
        return;
    }

    if (caretRow_ > 0)
    {
        recordUndoState();
        const std::size_t previousSize = lines_[caretRow_ - 1].size();
        lines_[caretRow_ - 1] += lines_[caretRow_];
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(caretRow_));
        --caretRow_;
        caretColumn_ = previousSize;
        dirty_ = true;
    }
}

void TextBuffer::deleteForward()
{
    if (hasSelection())
    {
        recordUndoState();
        deleteSelection();
        dirty_ = true;
        return;
    }

    if (caretColumn_ < lines_[caretRow_].size())
    {
        recordUndoState();
        lines_[caretRow_].erase(caretColumn_, 1);
        dirty_ = true;
        return;
    }

    if (caretRow_ + 1 < lines_.size())
    {
        recordUndoState();
        lines_[caretRow_] += lines_[caretRow_ + 1];
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(caretRow_ + 1));
        dirty_ = true;
    }
}

void TextBuffer::moveLeft()
{
    if (caretColumn_ > 0)
    {
        --caretColumn_;
    }
    else if (caretRow_ > 0)
    {
        --caretRow_;
        caretColumn_ = lines_[caretRow_].size();
    }
    clearSelection();
}

void TextBuffer::moveRight()
{
    if (caretColumn_ < lines_[caretRow_].size())
    {
        ++caretColumn_;
    }
    else if (caretRow_ + 1 < lines_.size())
    {
        ++caretRow_;
        caretColumn_ = 0;
    }
    clearSelection();
}

void TextBuffer::moveUp()
{
    if (caretRow_ > 0)
    {
        --caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
    clearSelection();
}

void TextBuffer::moveDown()
{
    if (caretRow_ + 1 < lines_.size())
    {
        ++caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
    clearSelection();
}

void TextBuffer::moveHome()
{
    caretColumn_ = 0;
    clearSelection();
}

void TextBuffer::moveEnd()
{
    caretColumn_ = lines_[caretRow_].size();
    clearSelection();
}

void TextBuffer::moveLeftSelecting()
{
    beginSelection();
    if (caretColumn_ > 0)
    {
        --caretColumn_;
    }
    else if (caretRow_ > 0)
    {
        --caretRow_;
        caretColumn_ = lines_[caretRow_].size();
    }
    updateSelectionAfterMove(true);
}

void TextBuffer::moveRightSelecting()
{
    beginSelection();
    if (caretColumn_ < lines_[caretRow_].size())
    {
        ++caretColumn_;
    }
    else if (caretRow_ + 1 < lines_.size())
    {
        ++caretRow_;
        caretColumn_ = 0;
    }
    updateSelectionAfterMove(true);
}

void TextBuffer::moveUpSelecting()
{
    beginSelection();
    if (caretRow_ > 0)
    {
        --caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
    updateSelectionAfterMove(true);
}

void TextBuffer::moveDownSelecting()
{
    beginSelection();
    if (caretRow_ + 1 < lines_.size())
    {
        ++caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
    updateSelectionAfterMove(true);
}

void TextBuffer::moveHomeSelecting()
{
    beginSelection();
    caretColumn_ = 0;
    updateSelectionAfterMove(true);
}

void TextBuffer::moveEndSelecting()
{
    beginSelection();
    caretColumn_ = lines_[caretRow_].size();
    updateSelectionAfterMove(true);
}

void TextBuffer::setCaret(std::size_t row, std::size_t column)
{
    caretRow_ = row;
    caretColumn_ = column;
    clampCaret();
    clearSelection();
}

void TextBuffer::setCaretWithSelection(std::size_t row, std::size_t column, bool selecting)
{
    if (selecting)
    {
        beginSelection();
    }
    caretRow_ = row;
    caretColumn_ = column;
    clampCaret();
    updateSelectionAfterMove(selecting);
}

bool TextBuffer::hasSelection() const
{
    return hasSelection_ && selectionAnchor_ != selectionCaret_;
}

TextBuffer::Position TextBuffer::selectionStart() const
{
    if (!hasSelection())
    {
        return caretPosition();
    }
    return selectionCaret_ < selectionAnchor_ ? selectionCaret_ : selectionAnchor_;
}

TextBuffer::Position TextBuffer::selectionEnd() const
{
    if (!hasSelection())
    {
        return caretPosition();
    }
    return selectionAnchor_ < selectionCaret_ ? selectionCaret_ : selectionAnchor_;
}

std::string TextBuffer::selectedText() const
{
    if (!hasSelection())
    {
        return "";
    }

    const Position start = selectionStart();
    const Position end = selectionEnd();
    std::ostringstream output;
    for (std::size_t row = start.row; row <= end.row; ++row)
    {
        const std::size_t startColumn = row == start.row ? start.column : 0;
        const std::size_t endColumn = row == end.row ? end.column : lines_[row].size();
        output << lines_[row].substr(startColumn, endColumn - startColumn);
        if (row != end.row)
        {
            output << '\n';
        }
    }
    return output.str();
}

void TextBuffer::clearSelection()
{
    hasSelection_ = false;
    selectionAnchor_ = caretPosition();
    selectionCaret_ = caretPosition();
}

bool TextBuffer::deleteSelection()
{
    if (!hasSelection())
    {
        return false;
    }

    const Position start = selectionStart();
    const Position end = selectionEnd();
    if (start.row == end.row)
    {
        lines_[start.row].erase(start.column, end.column - start.column);
    }
    else
    {
        std::string merged = lines_[start.row].substr(0, start.column);
        merged += lines_[end.row].substr(end.column);
        lines_[start.row] = merged;
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(start.row + 1),
            lines_.begin() + static_cast<std::ptrdiff_t>(end.row + 1));
    }

    caretRow_ = start.row;
    caretColumn_ = start.column;
    clampCaret();
    clearSelection();
    return true;
}

bool TextBuffer::copySelectionToClipboard() const
{
    if (!hasSelection())
    {
        return false;
    }
    return SDL_SetClipboardText(selectedText().c_str()) == 0;
}

bool TextBuffer::cutSelectionToClipboard()
{
    if (!hasSelection())
    {
        return false;
    }
    if (!copySelectionToClipboard())
    {
        return false;
    }
    recordUndoState();
    deleteSelection();
    dirty_ = true;
    return true;
}

bool TextBuffer::pasteFromClipboard()
{
    char* clipboard = SDL_GetClipboardText();
    if (clipboard == nullptr)
    {
        return false;
    }

    const std::string text = clipboard;
    SDL_free(clipboard);
    if (text.empty())
    {
        return false;
    }

    recordUndoState();
    deleteSelection();
    insertText(text);
    return true;
}

bool TextBuffer::undo()
{
    if (undoStack_.empty())
    {
        return false;
    }
    restoreSnapshot(undoStack_.back());
    undoStack_.pop_back();
    dirty_ = true;
    return true;
}

void TextBuffer::clampCaret()
{
    if (lines_.empty())
    {
        lines_.push_back("");
    }
    caretRow_ = std::min(caretRow_, lines_.size() - 1);
    caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
}

void TextBuffer::beginSelection()
{
    if (!hasSelection_)
    {
        hasSelection_ = true;
        selectionAnchor_ = caretPosition();
        selectionCaret_ = selectionAnchor_;
    }
}

void TextBuffer::updateSelectionAfterMove(bool selecting)
{
    if (!selecting)
    {
        clearSelection();
        return;
    }
    selectionCaret_ = caretPosition();
    if (selectionAnchor_ == selectionCaret_)
    {
        hasSelection_ = false;
    }
}

void TextBuffer::recordUndoState()
{
    undoStack_.push_back({ lines_, caretRow_, caretColumn_, hasSelection_, selectionAnchor_, selectionCaret_ });
    constexpr std::size_t kMaxUndoStates = 128;
    if (undoStack_.size() > kMaxUndoStates)
    {
        undoStack_.erase(undoStack_.begin());
    }
}

void TextBuffer::restoreSnapshot(const Snapshot& snapshot)
{
    lines_ = snapshot.lines;
    caretRow_ = snapshot.caretRow;
    caretColumn_ = snapshot.caretColumn;
    hasSelection_ = snapshot.hasSelection;
    selectionAnchor_ = snapshot.selectionAnchor;
    selectionCaret_ = snapshot.selectionCaret;
    clampCaret();
}

TextBuffer::Position TextBuffer::caretPosition() const
{
    return { caretRow_, caretColumn_ };
}
