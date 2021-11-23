/*=============================================================================
    Copyright (c) 2018 Nikita Kniazev

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix.hpp>
#include <boost/function.hpp>
#include <string>

// Checks that rhs Phoenix actor is taken by value on assignment.
// The wrapper function is used to ensure that created temporaries are
// out of scope (as they will be created on the other stack frame).

boost::function<void()> make_assignment_test(std::string & s)
{
    return boost::phoenix::ref(s) = "asd";
}

int main()
{
    std::string s;
    make_assignment_test(s)();
    BOOST_TEST(s == "asd");

    return boost::report_errors();
}
