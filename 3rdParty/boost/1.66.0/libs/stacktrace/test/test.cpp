// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/stacktrace/stacktrace_fwd.hpp>

#include <boost/stacktrace.hpp>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <boost/core/lightweight_test.hpp>

#include <boost/functional/hash.hpp>


using boost::stacktrace::stacktrace;
using boost::stacktrace::frame;

#ifdef BOOST_STACKTRACE_DYN_LINK
#   define BOOST_ST_API BOOST_SYMBOL_IMPORT
#else
#   define BOOST_ST_API
#endif


#if (defined(BOOST_GCC) && defined(BOOST_WINDOWS) && !defined(BOOST_STACKTRACE_USE_BACKTRACE) && !defined(BOOST_STACKTRACE_USE_ADDR2LINE)) \
    || defined(BOOST_STACKTRACE_TEST_NO_DEBUG_AT_ALL)

#   define BOOST_STACKTRACE_SYMNAME 0
#else
#   define BOOST_STACKTRACE_SYMNAME 1
#endif

typedef std::pair<stacktrace, stacktrace> (*foo1_t)(int i);
BOOST_ST_API std::pair<stacktrace, stacktrace> foo2(int i, foo1_t foo1);
BOOST_ST_API stacktrace return_from_nested_namespaces();
BOOST_ST_API boost::stacktrace::stacktrace bar1();
BOOST_ST_API boost::stacktrace::stacktrace bar2();

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

void test_deeply_nested_namespaces() {
    std::stringstream ss;
    ss << return_from_nested_namespaces();
    std::cout << ss.str() << '\n';
#if BOOST_STACKTRACE_SYMNAME
    BOOST_TEST(ss.str().find("main") != std::string::npos);

    BOOST_TEST(ss.str().find("get_backtrace_from_nested_namespaces") != std::string::npos
        || ss.str().find("1# return_from_nested_namespaces") != std::string::npos); // GCC with -O1 has strange inlining, so this line is true while the prev one is false.

    BOOST_TEST(ss.str().find("return_from_nested_namespaces") != std::string::npos);
#endif

    stacktrace ns1 = return_from_nested_namespaces();
    BOOST_TEST(ns1 != return_from_nested_namespaces()); // Different addresses in test_deeply_nested_namespaces() function
}

// Template parameter Depth is to produce different functions on each Depth. This simplifies debugging when one of the tests catches error
template <std::size_t Depth>
void test_nested() {
    std::pair<stacktrace, stacktrace> res = foo2(Depth, foo1);

    std::stringstream ss1, ss2;

    ss1 << res.first;
    ss2 << res.second;
    std::cout << "'" << ss1.str() << "'\n\n" << ss2.str() << std::endl;
    BOOST_TEST(!ss1.str().empty());
    BOOST_TEST(!ss2.str().empty());

    BOOST_TEST(ss1.str().find(" 0# ") != std::string::npos);
    BOOST_TEST(ss2.str().find(" 0# ") != std::string::npos);

    BOOST_TEST(ss1.str().find(" 1# ") != std::string::npos);
    BOOST_TEST(ss2.str().find(" 1# ") != std::string::npos);
    
    BOOST_TEST(ss1.str().find(" in ") != std::string::npos);
    BOOST_TEST(ss2.str().find(" in ") != std::string::npos);

#if BOOST_STACKTRACE_SYMNAME
    BOOST_TEST(ss1.str().find("main") != std::string::npos);
    BOOST_TEST(ss2.str().find("main") != std::string::npos);

    BOOST_TEST(ss1.str().find("foo2") != std::string::npos);
    BOOST_TEST(ss2.str().find("foo2") != std::string::npos);

    BOOST_TEST(ss1.str().find("foo1") != std::string::npos);
    BOOST_TEST(ss2.str().find("foo1") != std::string::npos);
#endif

    //BOOST_TEST(false);
}

template <class Bt>
void test_comparisons_base(Bt nst, Bt st) {
    Bt cst(st);
    st = st;
    cst = cst;
    BOOST_TEST(nst);
    BOOST_TEST(st);
#if !defined(BOOST_MSVC) && !defined(BOOST_STACKTRACE_USE_WINDBG)
    // This is very dependent on compiler and link flags. No sane way to make it work, because
    // BOOST_NOINLINE could be ignored by MSVC compiler if link-time optimization is enabled.
    BOOST_TEST(nst[0] != st[0]);
#endif

    BOOST_TEST(nst != st);
    BOOST_TEST(st != nst);
    BOOST_TEST(st == st);
    BOOST_TEST(nst == nst);

    BOOST_TEST(nst != cst);
    BOOST_TEST(cst != nst);
    BOOST_TEST(cst == st);
    BOOST_TEST(cst == cst);

    BOOST_TEST(nst < st || nst > st);
    BOOST_TEST(st < nst || nst < st);
    BOOST_TEST(st <= st);
    BOOST_TEST(nst <= nst);
    BOOST_TEST(st >= st);
    BOOST_TEST(nst >= nst);

    BOOST_TEST(nst < cst || cst < nst);
    BOOST_TEST(nst > cst || cst > nst);


    BOOST_TEST(hash_value(nst) == hash_value(nst));
    BOOST_TEST(hash_value(cst) == hash_value(st));

    BOOST_TEST(hash_value(nst) != hash_value(cst));
    BOOST_TEST(hash_value(st) != hash_value(nst));
}

void test_comparisons() {
    stacktrace nst = return_from_nested_namespaces();
    stacktrace st;
    test_comparisons_base(nst, st);
}

void test_iterators() {
    stacktrace st;

    BOOST_TEST(st.begin() == st.begin());
    BOOST_TEST(st.cbegin() == st.cbegin());
    BOOST_TEST(st.crbegin() == st.crbegin());
    BOOST_TEST(st.rbegin() == st.rbegin());

    BOOST_TEST(st.begin() + 1 == st.begin() + 1);
    BOOST_TEST(st.cbegin() + 1 == st.cbegin() + 1);
    BOOST_TEST(st.crbegin() + 1 == st.crbegin() + 1);
    BOOST_TEST(st.rbegin() + 1 == st.rbegin() + 1);

    BOOST_TEST(st.end() == st.end());
    BOOST_TEST(st.cend() == st.cend());
    BOOST_TEST(st.crend() == st.crend());
    BOOST_TEST(st.rend() == st.rend());

    BOOST_TEST(st.end() > st.begin());
    BOOST_TEST(st.end() > st.cbegin());
    BOOST_TEST(st.cend() > st.cbegin());
    BOOST_TEST(st.cend() > st.begin());

    BOOST_TEST(st.size() == static_cast<std::size_t>(st.end() - st.begin()));
    BOOST_TEST(st.size() == static_cast<std::size_t>(st.end() - st.cbegin()));
    BOOST_TEST(st.size() == static_cast<std::size_t>(st.cend() - st.cbegin()));
    BOOST_TEST(st.size() == static_cast<std::size_t>(st.cend() - st.begin()));

    BOOST_TEST(st.size() == static_cast<std::size_t>(std::distance(st.rbegin(), st.rend())));
    BOOST_TEST(st.size() == static_cast<std::size_t>(std::distance(st.crbegin(), st.rend())));
    BOOST_TEST(st.size() == static_cast<std::size_t>(std::distance(st.crbegin(), st.crend())));
    BOOST_TEST(st.size() == static_cast<std::size_t>(std::distance(st.rbegin(), st.crend())));


    boost::stacktrace::stacktrace::iterator it = st.begin();
    ++ it;
    BOOST_TEST(it == st.begin() + 1);
}

void test_frame() {
    stacktrace nst = return_from_nested_namespaces();
    stacktrace st;

    const std::size_t min_size = (nst.size() < st.size() ? nst.size() : st.size());
    BOOST_TEST(min_size > 2);

    for (std::size_t i = 0; i < min_size; ++i) {
        BOOST_TEST(st[i] == st[i]);
        BOOST_TEST(st[i].source_file() == st[i].source_file());
        BOOST_TEST(st[i].source_line() == st[i].source_line());
        BOOST_TEST(st[i] <= st[i]);
        BOOST_TEST(st[i] >= st[i]);

        frame fv = nst[i];
        BOOST_TEST(fv);
        if (i > 1 && i < min_size - 3) {       // Begin ...and end of the trace may match, skipping
            BOOST_TEST(st[i] != fv);
            
#if !(defined(BOOST_STACKTRACE_TEST_NO_DEBUG_AT_ALL) && defined(BOOST_MSVC))
            // MSVC can not get function name withhout debug symbols even if it is exported
            BOOST_TEST(st[i].name() != fv.name());
            BOOST_TEST(st[i] != fv);
            BOOST_TEST(st[i] < fv || st[i] > fv);
            BOOST_TEST(hash_value(st[i]) != hash_value(fv));
#endif

            if (st[i].source_line()) {
                BOOST_TEST(st[i].source_file() != fv.source_file() || st[i].source_line() != fv.source_line());
            }
            BOOST_TEST(st[i]);
        }

        fv = st[i];
        BOOST_TEST(hash_value(st[i]) == hash_value(fv));
    }

    boost::stacktrace::frame empty_frame;
    BOOST_TEST(!empty_frame);
    BOOST_TEST(empty_frame.source_file() == "");
    BOOST_TEST(empty_frame.name() == "");
    BOOST_TEST(empty_frame.source_line() == 0);
}

// Template parameter bool BySkip is to produce different functions on each BySkip. This simplifies debugging when one of the tests catches error
template <bool BySkip>
void test_empty_basic_stacktrace() {
    typedef boost::stacktrace::stacktrace st_t;
    st_t st = BySkip ? st_t(100500, 1024) :  st_t(0, 0);

    BOOST_TEST(!st);
    BOOST_TEST(st.empty());
    BOOST_TEST(st.size() == 0);
    BOOST_TEST(st.begin() == st.end());
    BOOST_TEST(st.cbegin() == st.end());
    BOOST_TEST(st.cbegin() == st.cend());
    BOOST_TEST(st.begin() == st.cend());

    BOOST_TEST(st.rbegin() == st.rend());
    BOOST_TEST(st.crbegin() == st.rend());
    BOOST_TEST(st.crbegin() == st.crend());
    BOOST_TEST(st.rbegin() == st.crend());

    BOOST_TEST(hash_value(st) == hash_value(st_t(0, 0)));
    BOOST_TEST(st == st_t(0, 0));
    BOOST_TEST(!(st < st_t(0, 0)));
    BOOST_TEST(!(st > st_t(0, 0)));
}

int main() {
    test_deeply_nested_namespaces();
    test_nested<15>();
    test_comparisons();
    test_iterators();
    test_frame();
    test_empty_basic_stacktrace<true>();
    test_empty_basic_stacktrace<false>();

    BOOST_TEST(&bar1 != &bar2);
    boost::stacktrace::stacktrace b1 = bar1();
    BOOST_TEST(b1.size() == 4);
    boost::stacktrace::stacktrace b2 = bar2();
    BOOST_TEST(b2.size() == 4);
    test_comparisons_base(bar1(), bar2());

    test_nested<300>();
    BOOST_TEST(boost::stacktrace::stacktrace(0, 1).size() == 1);
    BOOST_TEST(boost::stacktrace::stacktrace(1, 1).size() == 1);

    return boost::report_errors();
}
