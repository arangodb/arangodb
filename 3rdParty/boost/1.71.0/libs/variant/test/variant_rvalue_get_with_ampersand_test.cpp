//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_get_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2017-2017 Albert Sverdlov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/variant/get.hpp"
#include "boost/variant/variant.hpp"
#include "boost/core/lightweight_test.hpp"

#include <boost/move/move.hpp>
#include <boost/static_assert.hpp>

#include <string>

#define UNUSED(v) (void)(v)

inline void run()
{
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    typedef boost::variant<int, std::string> var_t;

    std::string s = "abacaba";
    var_t v = s;

    // must spit an error at compile-time because of 'std::string&'
    std::string new_s = boost::strict_get<std::string&>(boost::move(v));
    UNUSED(new_s);
#else
    BOOST_STATIC_ASSERT_MSG(false, "Dummy compile-time error to pass the test on C++03");
#endif
}

int main()
{
    run();
    return boost::report_errors();
}
