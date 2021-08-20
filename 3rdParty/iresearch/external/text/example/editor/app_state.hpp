// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ editor_app_state_header
#ifndef EDITOR_APP_STATE_HPP
#define EDITOR_APP_STATE_HPP

#include "buffer.hpp"
#include "event.hpp"
#include "key_mappings.hpp"

#include <boost/optional.hpp>


// The state of our entire editor.  This consists of a single buffer, the key
// bindings in use, and the current incomplete key sequence, if any.
//
// Due to buffer_'s unbounded history size and therefore potentially costly
// copy, you should move app_state_t's when possible.
struct app_state_t
{
    buffer_t buffer_;
    key_map_t key_map_;
    key_sequence_t input_seq_;
};

// Takes the current state of the editor and an event, and returns the new
// state.  It is expected that when the returned optional is empty, the editor
// exits.
boost::optional<app_state_t> update(app_state_t state, event_t event);

#endif
//]
