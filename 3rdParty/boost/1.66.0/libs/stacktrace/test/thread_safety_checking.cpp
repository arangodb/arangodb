// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/stacktrace/stacktrace_fwd.hpp>

#include <sstream>

#include <boost/stacktrace.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <boost/core/lightweight_test.hpp>

#include <boost/timer/timer.hpp>

#ifdef BOOST_STACKTRACE_DYN_LINK
#   define BOOST_ST_API BOOST_SYMBOL_IMPORT
#else
#   define BOOST_ST_API
#endif

using boost::stacktrace::stacktrace;

typedef std::pair<stacktrace, stacktrace> (*foo1_t)(int i);
BOOST_ST_API std::pair<stacktrace, stacktrace> foo2(int i, foo1_t foo1);
BOOST_ST_API stacktrace return_from_nested_namespaces();

BOOST_NOINLINE std::pair<stacktrace, stacktrace> foo1(int i) {
    if (i) {
        return foo2(i - 1, foo1);
    }

    std::pair<stacktrace, stacktrace> ret;
    try {
        throw std::logic_error("test");
    } catch (const std::logic_error& /*e*/) {
        ret.second = stacktrace();
        return ret;
    }
}

void main_test_loop() {
    std::size_t loops = 100;
    int Depth = 25;

    boost::optional<std::pair<stacktrace, stacktrace> > ethalon;
    std::stringstream ss_ethalon;

    while (--loops) {
        std::pair<stacktrace, stacktrace> res = foo2(Depth, foo1);
        if (ethalon) {
            BOOST_TEST(res == *ethalon);

            std::stringstream ss;
            ss << res.first;
            BOOST_TEST(ss.str() == ss_ethalon.str());
        } else {
            ethalon = res;
            ss_ethalon << ethalon->first;
        }
    }
}

#if defined(BOOST_STACKTRACE_TEST_COM_PREINIT_MT) || defined(BOOST_STACKTRACE_TEST_COM_PREINIT_ST)
#   include <windows.h>
#   include "dbgeng.h"
#endif

int main() {
#if defined(BOOST_STACKTRACE_TEST_COM_PREINIT_MT)
    ::CoInitializeEx(0, COINIT_MULTITHREADED);
#elif defined(BOOST_STACKTRACE_TEST_COM_PREINIT_ST)
    ::CoInitializeEx(0, COINIT_APARTMENTTHREADED);
#endif

    boost::timer::auto_cpu_timer t;

    boost::thread t1(main_test_loop);
    boost::thread t2(main_test_loop);
    boost::thread t3(main_test_loop);
    main_test_loop();

    t1.join();
    t2.join();
    t3.join();

#if defined(BOOST_STACKTRACE_TEST_COM_PREINIT_MT) || defined(BOOST_STACKTRACE_TEST_COM_PREINIT_ST)
    ::CoUninitialize();
#endif

    return boost::report_errors();
}
