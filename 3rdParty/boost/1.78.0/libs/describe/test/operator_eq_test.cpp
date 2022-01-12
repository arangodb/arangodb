// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/operators.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

namespace app
{

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

using boost::describe::operators::operator==;
using boost::describe::operators::operator<<;

struct Y
{
    int a = 1;
};

BOOST_DESCRIBE_STRUCT(Y, (), (a))

using boost::describe::operators::operator==;
using boost::describe::operators::operator!=;
using boost::describe::operators::operator<<;

struct Z: X, Y
{
    int b = 4;
};

BOOST_DESCRIBE_STRUCT(Z, (X, Y), (b))

using boost::describe::operators::operator==;
using boost::describe::operators::operator!=;
using boost::describe::operators::operator<<;

} // namespace app

int main()
{
    using app::X;

    {
        X x1, x2;
        BOOST_TEST_EQ( x1, x2 );
    }

    using app::Y;

    {
        Y y1, y2, y3;

        y3.a = 2;

        BOOST_TEST_EQ( y1, y2 );
        BOOST_TEST_NE( y1, y3 );
    }

    using app::Z;

    {
        Z z1, z2, z3, z4;

        z3.a = 2;
        z4.b = 3;

        BOOST_TEST_EQ( z1, z2 );
        BOOST_TEST_NE( z1, z3 );
        BOOST_TEST_NE( z1, z4 );
        BOOST_TEST_NE( z3, z4 );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
