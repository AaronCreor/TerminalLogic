#include "ui/TextBuffer.h"

#include <algorithm>
#include <sstream>

void TextBuffer::setText(const std::string& text)
{
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
    lines_[caretRow_].insert(lines_[caretRow_].begin() + static_cast<std::ptrdiff_t>(caretColumn_), value);
    ++caretColumn_;
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
    std::string tail = lines_[caretRow_].substr(caretColumn_);
    lines_[caretRow_].erase(caretColumn_);
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(caretRow_ + 1), tail);
    ++caretRow_;
    caretColumn_ = 0;
    dirty_ = true;
}

void TextBuffer::backspace()
{
    if (caretColumn_ > 0)
    {
        lines_[caretRow_].erase(caretColumn_ - 1, 1);
        --caretColumn_;
        dirty_ = true;
        return;
    }

    if (caretRow_ > 0)
    {
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
    if (caretColumn_ < lines_[caretRow_].size())
    {
        lines_[caretRow_].erase(caretColumn_, 1);
        dirty_ = true;
        return;
    }

    if (caretRow_ + 1 < lines_.size())
    {
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
}

void TextBuffer::moveUp()
{
    if (caretRow_ > 0)
    {
        --caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
}

void TextBuffer::moveDown()
{
    if (caretRow_ + 1 < lines_.size())
    {
        ++caretRow_;
        caretColumn_ = std::min(caretColumn_, lines_[caretRow_].size());
    }
}

void TextBuffer::moveHome()
{
    caretColumn_ = 0;
}

void TextBuffer::moveEnd()
{
    caretColumn_ = lines_[caretRow_].size();
}

void TextBuffer::setCaret(std::size_t row, std::size_t column)
{
    caretRow_ = row;
    caretColumn_ = column;
    clampCaret();
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
