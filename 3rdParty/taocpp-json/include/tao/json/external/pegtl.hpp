// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_HPP
#define TAO_JSON_PEGTL_HPP

#include "pegtl/config.hpp"
#include "pegtl/version.hpp"

#include "pegtl/parse.hpp"

#include "pegtl/ascii.hpp"
#include "pegtl/rules.hpp"
#include "pegtl/uint16.hpp"
#include "pegtl/uint32.hpp"
#include "pegtl/uint64.hpp"
#include "pegtl/uint8.hpp"
#include "pegtl/utf16.hpp"
#include "pegtl/utf32.hpp"
#include "pegtl/utf8.hpp"

#include "pegtl/argv_input.hpp"
#include "pegtl/buffer_input.hpp"
#include "pegtl/cstream_input.hpp"
#include "pegtl/istream_input.hpp"
#include "pegtl/memory_input.hpp"
#include "pegtl/read_input.hpp"
#include "pegtl/string_input.hpp"

// this has to be included *after* the above inputs,
// otherwise the amalgamated header will not work!
#include "pegtl/file_input.hpp"

#include "pegtl/change_action.hpp"
#include "pegtl/change_action_and_state.hpp"
#include "pegtl/change_action_and_states.hpp"
#include "pegtl/change_control.hpp"
#include "pegtl/change_state.hpp"
#include "pegtl/change_states.hpp"

#include "pegtl/disable_action.hpp"
#include "pegtl/enable_action.hpp"

#include "pegtl/discard_input.hpp"
#include "pegtl/discard_input_on_failure.hpp"
#include "pegtl/discard_input_on_success.hpp"

// The following are not included by
// default because they include <iostream>.

// #include "pegtl/analyze.hpp"

#endif
