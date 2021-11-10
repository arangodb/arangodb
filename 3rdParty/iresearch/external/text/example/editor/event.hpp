// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef EDITOR_EVENT
#define EDITOR_EVENT

#include "key_mappings.hpp"


struct screen_pos_t
{
    int row_ = 0;
    int col_ = 0;
};

inline bool operator==(screen_pos_t lhs, screen_pos_t rhs)
{
    return lhs.row_ == rhs.row_ && lhs.col_ == rhs.col_;
}

inline bool operator!=(screen_pos_t lhs, screen_pos_t rhs)
{
    return !(lhs == rhs);
}

struct event_t
{
    key_code_t key_code_;
    screen_pos_t screen_size_;
};

#endif
