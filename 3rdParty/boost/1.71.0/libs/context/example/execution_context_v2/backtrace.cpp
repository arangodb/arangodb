
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define UNW_LOCAL_ONLY

#include <cstdlib>
#include <iostream>

#include <libunwind.h>

#include <boost/context/execution_context.hpp>

namespace ctx = boost::context;

void backtrace() {
	unw_cursor_t cursor;
	unw_context_t context;
	unw_getcontext( & context);
	unw_init_local( & cursor, & context);
	while ( 0 < unw_step( & cursor) ) {
		unw_word_t offset, pc;
		unw_get_reg( & cursor, UNW_REG_IP, & pc);
		if ( 0 == pc) {
			break;
		}
		std::cout << "0x" << pc << ":";

		char sym[256];
		if ( 0 == unw_get_proc_name( & cursor, sym, sizeof( sym), & offset) ) {
			std::cout << " (" << sym << "+0x" << offset << ")" << std::endl;
		} else {
			std::cout << " -- error: unable to obtain symbol name for this frame" << std::endl;
		}
	}
}

void bar() {
	backtrace();
}

void foo() {
	bar();
}

ctx::execution_context< void > f1( ctx::execution_context< void > && ctxm) {
    foo();
    return std::move( ctxm);
}

int main() {
    ctx::execution_context< void > ctx1( f1);
    ctx1 = ctx1();
    std::cout << "main: done" << std::endl;
    return EXIT_SUCCESS;
}
