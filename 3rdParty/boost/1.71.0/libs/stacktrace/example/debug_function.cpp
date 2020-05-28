// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[getting_started_debug_function
#include <signal.h>     // ::signal
#include <boost/stacktrace/frame.hpp>
#include <iostream>     // std::cerr
#include <cstdlib>      // std::exit

void print_signal_handler_and_exit() {
    typedef void(*function_t)(int);

    function_t old_signal_function = ::signal(SIGSEGV, SIG_DFL);
    boost::stacktrace::frame f(old_signal_function);
    std::cout << f << std::endl;
    std::exit(0);
}
//]


void my_signal_handler(int /*signum*/) {
    std::exit(1);
}

int main() {
    ::signal(SIGSEGV, &my_signal_handler);
    print_signal_handler_and_exit();
}


