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

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

class Y
{
    BOOST_DESCRIBE_CLASS(Y, (), (), (), ())
};

} // namespace

using namespace boost::describe;

using L1 = describe_enumerators<E>;

using L2 = describe_bases<X, mod_any_access>;
using L3 = describe_members<X, mod_any_access>;

using L4 = describe_bases<Y, mod_any_access>;
using L5 = describe_members<Y, mod_any_access>;

#endif // !defined(BOOST_DESCRIBE_CXX14)
