// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef EDITOR_CURSES_INTERFACE
#define EDITOR_CURSES_INTERFACE

#include "buffer.hpp"
#include "event.hpp"


struct _win_st;

struct curses_interface_t
{
    curses_interface_t();
    ~curses_interface_t();

    screen_pos_t screen_size() const;
    event_t next_event() const;

private:
    _win_st * win_;
};

void render(buffer_t const & buffer, screen_pos_t screen_size);

#endif
