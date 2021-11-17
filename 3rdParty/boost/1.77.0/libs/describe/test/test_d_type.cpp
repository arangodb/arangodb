// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enum.hpp>
#include <boost/describe/class.hpp>

namespace N1
{
    enum D
    {
        d
    };

    BOOST_DESCRIBE_ENUM(D, d)
}

namespace N2
{
    enum E
    {
        D
    };

    BOOST_DESCRIBE_ENUM(E, D)
}

namespace N3
{
    struct D
    {
        int d;
    };

    BOOST_DESCRIBE_STRUCT(D, (), (d))
}

namespace N4
{
    struct E
    {
        int D;
    };

    BOOST_DESCRIBE_STRUCT(E, (), (D))
}

namespace N5
{
    class D
    {
        int d;
        BOOST_DESCRIBE_CLASS(D, (), (), (), (d))
    };
}

namespace N6
{
    struct D {};
    struct E: D	{};
    BOOST_DESCRIBE_STRUCT(E, (D), ())
}

namespace N7
{
    struct D {};
    class E: D
    {
        int d;
        BOOST_DESCRIBE_CLASS(E, (D), (), (), (d))
    };
}

int main()
{
}
