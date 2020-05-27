//  Unit test for boost::lexical_cast.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2012-2019.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <boost/config.hpp>

#if defined(__INTEL_COMPILER)
#pragma warning(disable: 193 383 488 981 1418 1419)
#elif defined(BOOST_MSVC)
#pragma warning(disable: 4097 4100 4121 4127 4146 4244 4245 4511 4512 4701 4800)
#endif

#include <boost/lexical_cast.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/range/iterator_range.hpp>

#include <cstdlib>

#ifndef BOOST_NO_EXCEPTIONS
#error "This test must be compiled with -DBOOST_NO_EXCEPTIONS"
#endif

struct Escape
{
    Escape(){}
    Escape(const std::string& s)
        : str_(s)
    {}

    std::string str_;
};

inline std::ostream& operator<< (std::ostream& o, const Escape& rhs)
{
    return o << rhs.str_;
}

inline std::istream& operator>> (std::istream& i, Escape& rhs)
{
    return i >> rhs.str_;
}

namespace boost {

BOOST_NORETURN void throw_exception(std::exception const & ) {
    static int state = 0;
    ++ state;

    Escape v("");
    switch(state) {
    case 1: 
        lexical_cast<char>(v); // should call boost::throw_exception
        std::exit(1);
    case 2:    
        lexical_cast<unsigned char>(v); // should call boost::throw_exception
        std::exit(2);
    }
    std::exit(boost::report_errors());
}

}

void test_exceptions_off() {
    using namespace boost;
    Escape v("");
    
    v = lexical_cast<Escape>(100);
    BOOST_TEST_EQ(lexical_cast<int>(v), 100);
    BOOST_TEST_EQ(lexical_cast<unsigned int>(v), 100u);

    v = lexical_cast<Escape>(0.0);
    BOOST_TEST_EQ(lexical_cast<double>(v), 0.0);

    BOOST_TEST_EQ(lexical_cast<short>(100), 100);
    BOOST_TEST_EQ(lexical_cast<float>(0.0), 0.0);

    lexical_cast<short>(700000); // should call boost::throw_exception
    BOOST_TEST(false);
}

int main() {
    test_exceptions_off();

    return boost::report_errors();
}

