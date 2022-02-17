// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef EDITOR_KEY_MAPPINGS
#define EDITOR_KEY_MAPPINGS

#include <boost/container/static_vector.hpp>
#include <boost/function.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/text/string_view.hpp>


struct app_state_t;
struct key_sequence_t;
struct screen_pos_t;

// These are the special keys we support.  Anything that is not UTF-8 or one
// of these is ignored.
enum key {
    up = 256,
    down,
    left,
    right,
    home,
    end,
    page_up,
    page_down,
    left_click,
    left_double_click,
    left_triple_click,
    backspace,
    delete_
};

//[ editor_key_mappings_0
// Represents a key press or a mouse event.
struct key_code_t
{
    key_code_t() : key_(0), x_(0), y_(0) {}
    key_code_t(int c) : key_(c), x_(0), y_(0) {}
    key_code_t(key k);
    key_code_t(int k, int x, int y) : key_(k), x_(x), y_(y) {}

    bool operator==(key_code_t rhs) const { return key_ == rhs.key_; }
    bool operator!=(key_code_t rhs) const { return !(*this == rhs); }

    key_sequence_t operator,(key_code_t rhs) &&;

    int key_;
    int x_;
    int y_;
};

// A sequence of key_code_ts.  Using the ctrl_t and alt_t types and the
// function below, a key_sequence_t can be made using a natural syntax like
// "ctrl-'x', ctrl-'c'" (which means a Control-x followed by a Control-C.
struct key_sequence_t
{
    static const int max_size = 8;

    using iterator =
        boost::container::static_vector<key_code_t, max_size>::const_iterator;

    key_sequence_t() {}
    key_sequence_t(char c) { keys_.push_back(c); }
    key_sequence_t(key k) { keys_.push_back(k); }
    key_sequence_t(key_code_t k) { keys_.push_back(k); }

    bool operator==(key_sequence_t rhs) const { return keys_ == rhs.keys_; }
    bool operator!=(key_sequence_t rhs) const { return !(*this == rhs); }

    bool single_key() const { return keys_.size() == 1; }

    key_code_t get_single_key() const
    {
        assert(single_key());
        return keys_.front();
    }

    iterator begin() const { return keys_.begin(); }
    iterator end() const { return keys_.end(); }

    void append(key_code_t k) { keys_.push_back(k); }

    key_sequence_t operator,(key_code_t k) &&
    {
        key_sequence_t retval(*this);
        keys_.push_back(k);
        return retval;
    }

private:
    boost::container::static_vector<key_code_t, max_size> keys_;
};
//]

// This is the type of function object used to store all the possible commands
// we can execute in the editor.
using command_t = boost::function<boost::optional<app_state_t>(
    app_state_t, screen_pos_t screen_size, screen_pos_t xy)>;

// A pairing of a single key sequence with the function that implements its
// behavior.
struct key_map_entry_t
{
    key_sequence_t key_seq_;
    command_t command_;
};

using key_map_t = std::vector<key_map_entry_t>;

struct ctrl_t
{};
struct alt_t
{};

extern ctrl_t ctrl;
extern alt_t alt;

key_code_t operator-(ctrl_t, char c);
key_code_t operator-(ctrl_t, key k);
key_sequence_t operator-(alt_t, char c);
key_code_t operator-(alt_t, key k);

//[ editor_key_mappings_1
// A key mapping that uses a subset of the Emacs key bindings.
key_map_t emacs_lite();
//]


// implementations

inline key_sequence_t key_code_t::operator,(key_code_t rhs) &&
{
    key_sequence_t retval;
    retval.append(*this);
    retval.append(rhs);
    return retval;
}

#endif
