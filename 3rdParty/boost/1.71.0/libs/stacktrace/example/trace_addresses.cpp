// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#ifdef BOOST_NO_CXX11_RANGE_BASED_FOR
#include <boost/stacktrace.hpp>
#include <iostream>     // std::cout

namespace bs = boost::stacktrace;
void dump_compact(const bs::stacktrace& st) {
    for (unsigned i = 0; i < st.size(); ++i) {
        bs::frame frame = st[i];
        std::cout << frame.address() << ',';
    }

    std::cout << std::endl;
}
#else
//[getting_started_trace_addresses
#include <boost/stacktrace.hpp>
#include <iostream>     // std::cout

namespace bs = boost::stacktrace;
void dump_compact(const bs::stacktrace& st) {
    for (bs::frame frame: st) {
        std::cout << frame.address() << ',';
    }

    std::cout << std::endl;
}
//]
#endif

BOOST_NOINLINE boost::stacktrace::stacktrace rec1(int i);
BOOST_NOINLINE boost::stacktrace::stacktrace rec2(int i);

BOOST_NOINLINE boost::stacktrace::stacktrace rec1(int i) {
    if (i < 5) {
        if (!i) return boost::stacktrace::stacktrace();
        return rec2(--i);
    }

    return rec2(i - 2);
}

BOOST_NOINLINE boost::stacktrace::stacktrace rec2(int i) {
    if (i < 5) {
        if (!i) return boost::stacktrace::stacktrace();
        return rec2(--i);
    }

    return rec2(i - 2);
}

int main() {
    dump_compact(rec1(8));
}


