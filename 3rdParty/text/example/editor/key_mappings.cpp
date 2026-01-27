// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "key_mappings.hpp"

extern "C" {
#include <ncurses.h>
}


namespace {

    key_code_t to_key_code(boost::text::string_view name)
    {
        assert(0 < (intptr_t)tigetstr((char *)name.begin()));
        int const k = key_defined(name.begin());
        assert(0 < k);
        return {k};
    }
}

ctrl_t ctrl;
alt_t alt;

key_code_t::key_code_t(key k) : x_(0), y_(0)
{
    switch (k) {
    case up: key_ = KEY_UP; break;
    case down: key_ = KEY_DOWN; break;
    case left: key_ = KEY_LEFT; break;
    case right: key_ = KEY_RIGHT; break;
    case home: key_ = KEY_HOME; break;
    case end: key_ = KEY_END; break;
    case backspace: key_ = KEY_BACKSPACE; break;
    case delete_: key_ = KEY_DC; break;
    case page_up: key_ = KEY_PPAGE; break;
    case page_down: key_ = KEY_NPAGE; break;
    case left_click: key_ = BUTTON1_CLICKED; break;
    case left_double_click: key_ = BUTTON1_DOUBLE_CLICKED; break;
    case left_triple_click: key_ = BUTTON1_TRIPLE_CLICKED; break;
    default: assert(!"Unhandled case in key_sequence_t (key)");
    }
}

key_code_t operator-(ctrl_t, char c)
{
    assert(' ' <= c && c <= '~');
    if ('a' <= c && c <= 'z')
        c -= 96;
    else if ('@' <= c && c <= '_')
        c -= 64;
    return {c};
}

key_code_t operator-(ctrl_t, key k)
{
    switch (k) {
    case up: return to_key_code("kUP5");
    case down: return to_key_code("kDN5");
    case left: return to_key_code("kLFT5");
    case right: return to_key_code("kRIT5");
    default: assert(!"Unhandled case in operator- (ctrl_t, key)");
    }
    return {}; // This should never execute.
}

key_sequence_t operator-(alt_t, char c) { return ctrl - '[', c; }

key_code_t operator-(alt_t, key k)
{
    switch (k) {
    case up: return to_key_code("kUP3");
    case down: return to_key_code("kDN3");
    case left: return to_key_code("kLFT3");
    case right: return to_key_code("kRIT3");
    default: assert(!"Unhandled case in operator- (alt_t, key)");
    }
    return {}; // This should never execute.
}
