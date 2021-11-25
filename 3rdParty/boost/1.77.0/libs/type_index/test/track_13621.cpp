//
// Copyright 2018-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index/ctti_type_index.hpp>
#include <string>

#include <boost/core/lightweight_test.hpp>

class empty
{
};

int main()
{
    std::string name = boost::typeindex::ctti_type_index::type_id<empty>().pretty_name();
    BOOST_TEST(name.find("empty") != std::string::npos);
    return boost::report_errors();
}

