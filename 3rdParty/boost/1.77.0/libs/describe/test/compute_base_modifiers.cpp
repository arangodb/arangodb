// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/detail/config.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

#include <boost/describe/detail/compute_base_modifiers.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

struct X1
{
};

struct X2
{
};

struct X3
{
};

struct V1
{
    V1(int);
};

struct V2
{
    V2(int);
};

struct V3
{
    V3(int);
};

struct Y: public X1, protected X2, private X3, public virtual V1, protected virtual V2, private virtual V3
{
};

struct Z final: public X1, protected X2, private X3, public virtual V1, protected virtual V2, private virtual V3
{
};

int main()
{
    using namespace boost::describe::detail;
    using namespace boost::describe;

    BOOST_TEST( (is_public_base_of<X1, Y>::value) );
    BOOST_TEST( (!is_virtual_base_of<X1, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, X1>()), mod_public );

    BOOST_TEST( (!is_public_base_of<X2, Y>::value) );
    BOOST_TEST( (is_protected_base_of<X2, Y>::value) );
    BOOST_TEST( (!is_virtual_base_of<X2, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, X2>()), mod_protected );

    BOOST_TEST( (!is_public_base_of<X3, Y>::value) );
    BOOST_TEST( (!is_protected_base_of<X3, Y>::value) );
    BOOST_TEST( (!is_virtual_base_of<X3, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, X3>()), mod_private );

    BOOST_TEST( (is_public_base_of<V1, Y>::value) );
    BOOST_TEST( (is_virtual_base_of<V1, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, V1>()), mod_public | mod_virtual );

    BOOST_TEST( (!is_public_base_of<V2, Y>::value) );
    BOOST_TEST( (is_protected_base_of<V2, Y>::value) );
    BOOST_TEST( (is_virtual_base_of<V2, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, V2>()), mod_protected | mod_virtual );

    BOOST_TEST( (!is_public_base_of<V3, Y>::value) );
    BOOST_TEST( (!is_protected_base_of<V3, Y>::value) );
    BOOST_TEST( (is_virtual_base_of<V3, Y>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Y, V3>()), mod_private | mod_virtual );

    BOOST_TEST( (is_public_base_of<X1, Z>::value) );
    BOOST_TEST( (!is_virtual_base_of<X1, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, X1>()), mod_public );

    BOOST_TEST( (!is_public_base_of<X2, Z>::value) );
    BOOST_TEST( (!is_protected_base_of<X2, Z>::value) );
    BOOST_TEST( (!is_virtual_base_of<X2, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, X2>()), mod_private );

    BOOST_TEST( (!is_public_base_of<X3, Z>::value) );
    BOOST_TEST( (!is_protected_base_of<X3, Z>::value) );
    BOOST_TEST( (!is_virtual_base_of<X3, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, X3>()), mod_private );

    BOOST_TEST( (is_public_base_of<V1, Z>::value) );
    BOOST_TEST( (is_virtual_base_of<V1, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, V1>()), mod_public | mod_virtual );

    BOOST_TEST( (!is_public_base_of<V2, Z>::value) );
    BOOST_TEST( (!is_protected_base_of<V2, Z>::value) );
    BOOST_TEST( (is_virtual_base_of<V2, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, V2>()), mod_private | mod_virtual );

    BOOST_TEST( (!is_public_base_of<V3, Z>::value) );
    BOOST_TEST( (!is_protected_base_of<V3, Z>::value) );
    BOOST_TEST( (is_virtual_base_of<V3, Z>::value) );
    BOOST_TEST_EQ( (compute_base_modifiers<Z, V3>()), mod_private | mod_virtual );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
