// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "curses_interface.hpp"

extern "C" {
#include <ncurses.h>
}


curses_interface_t::curses_interface_t() : win_(initscr())
{
    if (win_ != stdscr)
        throw std::runtime_error("ncurses initscr() failed.");

    raw();
    noecho();
    keypad(stdscr, true);
    start_color();
    use_default_colors();
    mmask_t old_mouse_events;
    mousemask(
        BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED |
            REPORT_MOUSE_POSITION,
        &old_mouse_events);
    set_tabsize(1);
}

curses_interface_t::~curses_interface_t() { endwin(); }

screen_pos_t curses_interface_t::screen_size() const
{
    return {getmaxy(stdscr), getmaxx(stdscr)};
}

event_t curses_interface_t::next_event() const
{
    int const k = wgetch(win_);

    // Mouse events.
    if (k == KEY_MOUSE) {
        MEVENT e;
        if (getmouse(&e) == ERR)
            return {key_code_t{KEY_MAX}, screen_size()};
        return {key_code_t{(int)e.bstate, e.x, e.y}, screen_size()};
    }

    // Everything else.
    return {key_code_t{k}, screen_size()};
}

namespace {

    void render_text(snapshot_t const & snapshot, screen_pos_t screen_size)
    {
        int row = 0;
        std::vector<char> buf;
        std::ptrdiff_t pos = snapshot.first_char_index_;
        auto line_first = snapshot.first_row_;
        auto const line_last = std::min<std::ptrdiff_t>(
            line_first + screen_size.row_ - 2, snapshot.lines_.size());
        for (; line_first != line_last; ++line_first) {
            auto const line = snapshot.lines_[line_first];
            auto first = snapshot.content_.begin().base().base() + pos;
            auto const last = first + line.code_units_;
            move(row, 0);
            buf.clear();
            std::copy(first, last, std::back_inserter(buf));
            if (!buf.empty() && buf.back() == '\n')
                buf.pop_back();
            if (!buf.empty() && buf.back() == '\r')
                buf.pop_back();
            buf.push_back('\0');
            addstr(&buf[0]);
            pos += line.code_units_;
            ++row;
        }
    }
}

void render(buffer_t const & buffer, screen_pos_t screen_size)
{
    erase();

    auto const size = screen_pos_t{screen_size.row_ - 2, screen_size.col_};
    render_text(buffer.snapshot_, screen_size);

    // render the info line
    move(size.row_, 0);
    attron(A_REVERSE);
    printw(
        " %s %s  (%d, %d)",
        dirty(buffer) ? "**" : "--",
        buffer.path_.c_str(),
        buffer.snapshot_.first_row_ + buffer.snapshot_.cursor_pos_.row_ + 1,
        buffer.snapshot_.cursor_pos_.col_);
    attroff(A_REVERSE);
    hline(' ', size.col_);

    move(buffer.snapshot_.cursor_pos_.row_, buffer.snapshot_.cursor_pos_.col_);
    curs_set(true);

    refresh();
}
