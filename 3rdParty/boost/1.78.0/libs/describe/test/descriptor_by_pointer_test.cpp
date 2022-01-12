// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/descriptor_by_pointer.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/members.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(__cpp_nontype_template_parameter_auto) || __cpp_nontype_template_parameter_auto < 201606L

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because __cpp_nontype_template_parameter_auto is not defined")
int main() {}

#else

struct X
{
    int a = 1;
    float b = 3.14f;
};

BOOST_DESCRIBE_STRUCT(X, (), (a, b))

struct Y: public X
{
    long a = 2;
    double c = 3.14;
};

BOOST_DESCRIBE_STRUCT(Y, (X), (a, c))

int main()
{
    using namespace boost::describe;

    {
        using L = describe_members<X, mod_any_access>;

        using Da = descriptor_by_pointer<L, &X::a>;
        BOOST_TEST_CSTR_EQ( Da::name, "a" );

        using Db = descriptor_by_pointer<L, &X::b>;
        BOOST_TEST_CSTR_EQ( Db::name, "b" );
    }

    {
        using L = describe_members<Y, mod_any_access | mod_inherited>;

        using Da = descriptor_by_pointer<L, &Y::a>;
        BOOST_TEST_CSTR_EQ( Da::name, "a" );

        using Db = descriptor_by_pointer<L, &Y::b>;
        BOOST_TEST_CSTR_EQ( Db::name, "b" );

        using Dc = descriptor_by_pointer<L, &Y::c>;
        BOOST_TEST_CSTR_EQ( Dc::name, "c" );
    }

    return boost::report_errors();
}

#endif // __cpp_nontype_template_parameter_auto
