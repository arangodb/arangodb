// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ editor_main
#include "app_state.hpp"
#include "curses_interface.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <iostream>


int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "error: You must supply at least a filename.\n";
        return 1;
    }

    boost::filesystem::path path(argv[1]);
    if (!exists(path)) {
        std::cerr << "error: Could not access filename " << argv[1] << ".\n";
        return 1;
    }

    // Requirement of libcurses.
    std::locale::global(std::locale(""));

    curses_interface_t curses_interface;

    // Read the size, but just this once; resizing is not supported.
    auto const screen_size = curses_interface.screen_size();

    app_state_t app_state = {
        // Initial state is the contents of the file from the command line.
        load_buffer(path, curses_interface.screen_size().col_),
        // Keybindings are simplified Emacs.
        emacs_lite()};
    // Initial render.
    render(app_state.buffer_, screen_size);

    // Loop until the user says we're done.  Each iteration blocks for an
    // event from libcurses, then passes the current app state and the event
    // to update(), which returns the new app state.
    boost::optional<app_state_t> next_app_state;
    while (
        (next_app_state =
             update(std::move(app_state), curses_interface.next_event()))) {
        app_state = *next_app_state;
        render(app_state.buffer_, screen_size);
    }
}
//]
