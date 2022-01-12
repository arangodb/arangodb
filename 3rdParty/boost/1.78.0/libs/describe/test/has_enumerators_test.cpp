// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enumerators.hpp>
#include <boost/describe/enum.hpp>
#include <boost/core/lightweight_test_trait.hpp>

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

enum E1 { v1 };
BOOST_DESCRIBE_ENUM(E1, v1)

enum class E2 { v2 };
BOOST_DESCRIBE_ENUM(E2, v2)

BOOST_DEFINE_ENUM(E3, v3);
BOOST_DEFINE_ENUM_CLASS(E4, v4)

enum E5 { v5 };
enum class E6 { v6 };

int main()
{
    using boost::describe::has_describe_enumerators;

#if defined(BOOST_DESCRIBE_CXX14)

    BOOST_TEST_TRAIT_TRUE((has_describe_enumerators<E1>));
    BOOST_TEST_TRAIT_TRUE((has_describe_enumerators<E2>));
    BOOST_TEST_TRAIT_TRUE((has_describe_enumerators<E3>));
    BOOST_TEST_TRAIT_TRUE((has_describe_enumerators<E4>));

#else

    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E1>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E2>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E3>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E4>));

#endif

    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E5>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<E6>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<int>));
    BOOST_TEST_TRAIT_FALSE((has_describe_enumerators<void>));

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
