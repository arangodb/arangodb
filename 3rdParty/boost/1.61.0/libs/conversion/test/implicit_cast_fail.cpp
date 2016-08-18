// Copyright David Abrahams 2003.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/implicit_cast.hpp>

using boost::implicit_cast;

struct foo
{
    explicit foo(char const*) {}
};

int main()
{
    foo x = implicit_cast<foo>("foobar");
    (void)x;            // warning suppression.
    return 0;
}
