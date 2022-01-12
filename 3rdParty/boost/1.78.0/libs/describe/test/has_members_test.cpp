// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test_trait.hpp>

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

struct X1 {};
BOOST_DESCRIBE_STRUCT(X1, (), ())

class X2
{
    BOOST_DESCRIBE_CLASS(X2, (), (), (), ())
};

struct X3 {};
class X4 {};
union X5 {};

int main()
{
    using boost::describe::has_describe_members;

#if defined(BOOST_DESCRIBE_CXX14)

    BOOST_TEST_TRAIT_TRUE((has_describe_members<X1>));
    BOOST_TEST_TRAIT_TRUE((has_describe_members<X2>));

#else

    BOOST_TEST_TRAIT_FALSE((has_describe_members<X1>));
    BOOST_TEST_TRAIT_FALSE((has_describe_members<X2>));

#endif

    BOOST_TEST_TRAIT_FALSE((has_describe_members<X3>));
    BOOST_TEST_TRAIT_FALSE((has_describe_members<X4>));
    BOOST_TEST_TRAIT_FALSE((has_describe_members<X5>));
    BOOST_TEST_TRAIT_FALSE((has_describe_members<int>));
    BOOST_TEST_TRAIT_FALSE((has_describe_members<void>));

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
