// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")

#else

namespace
{

enum E { v };

BOOST_DESCRIBE_ENUM(E, v)

struct S
{
};

BOOST_DESCRIBE_STRUCT(S, (), ())

class C
{
    BOOST_DESCRIBE_CLASS(C, (), (), (), ())
};

} // namespace

#endif // !defined(BOOST_DESCRIBE_CXX14)
