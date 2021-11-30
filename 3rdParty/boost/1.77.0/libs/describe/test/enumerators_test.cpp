// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enumerators.hpp>
#include <boost/describe/enum.hpp>
#include <boost/core/lightweight_test.hpp>

enum E1
{
    v1_1 = 5
};

BOOST_DESCRIBE_ENUM(E1, v1_1);

BOOST_DEFINE_ENUM(E3, v3_1, v3_2, v3_3);

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

enum class E2
{
    v2_1,
    v2_2 = 7
};

BOOST_DESCRIBE_ENUM(E2, v2_1, v2_2);

BOOST_DEFINE_FIXED_ENUM(E4, int, v4_1, v4_2, v4_3, v4_4);

BOOST_DEFINE_ENUM_CLASS(E5, v5_1, v5_2, v5_3, v5_4, v5_5);
BOOST_DEFINE_FIXED_ENUM_CLASS(E6, int, v6_1, v6_2, v6_3, v6_4, v6_5, v6_6);

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

#include <boost/mp11.hpp>
using namespace boost::mp11;

int main()
{
    {
        using D1 = boost::describe::describe_enumerators<E1>;

        BOOST_TEST_EQ( mp_size<D1>::value, 1 );

        BOOST_TEST_EQ( (mp_at_c<D1, 0>::value), v1_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D1, 0>::name), "v1_1" );
    }

    {
        using D2 = boost::describe::describe_enumerators<E2>;

        BOOST_TEST_EQ( mp_size<D2>::value, 2 );

        BOOST_TEST_EQ( (int)(mp_at_c<D2, 0>::value), (int)E2::v2_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D2, 0>::name), "v2_1" );

        BOOST_TEST_EQ( (int)(mp_at_c<D2, 1>::value), (int)E2::v2_2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D2, 1>::name), "v2_2" );
    }

    {
        using D3 = boost::describe::describe_enumerators<E3>;

        BOOST_TEST_EQ( mp_size<D3>::value, 3 );

        BOOST_TEST_EQ( (mp_at_c<D3, 0>::value), v3_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D3, 0>::name), "v3_1" );

        BOOST_TEST_EQ( (mp_at_c<D3, 1>::value), v3_2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D3, 1>::name), "v3_2" );

        BOOST_TEST_EQ( (mp_at_c<D3, 2>::value), v3_3 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D3, 2>::name), "v3_3" );
    }

    {
        using D4 = boost::describe::describe_enumerators<E4>;

        BOOST_TEST_EQ( mp_size<D4>::value, 4 );

        BOOST_TEST_EQ( (mp_at_c<D4, 0>::value), v4_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D4, 0>::name), "v4_1" );

        BOOST_TEST_EQ( (mp_at_c<D4, 1>::value), v4_2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D4, 1>::name), "v4_2" );

        BOOST_TEST_EQ( (mp_at_c<D4, 2>::value), v4_3 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D4, 2>::name), "v4_3" );

        BOOST_TEST_EQ( (mp_at_c<D4, 3>::value), v4_4 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D4, 3>::name), "v4_4" );
    }

    {
        using D5 = boost::describe::describe_enumerators<E5>;

        BOOST_TEST_EQ( mp_size<D5>::value, 5 );

        BOOST_TEST_EQ( (int)(mp_at_c<D5, 0>::value), (int)E5::v5_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D5, 0>::name), "v5_1" );

        BOOST_TEST_EQ( (int)(mp_at_c<D5, 1>::value), (int)E5::v5_2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D5, 1>::name), "v5_2" );

        BOOST_TEST_EQ( (int)(mp_at_c<D5, 2>::value), (int)E5::v5_3 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D5, 2>::name), "v5_3" );

        BOOST_TEST_EQ( (int)(mp_at_c<D5, 3>::value), (int)E5::v5_4 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D5, 3>::name), "v5_4" );

        BOOST_TEST_EQ( (int)(mp_at_c<D5, 4>::value), (int)E5::v5_5 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D5, 4>::name), "v5_5" );
    }

    {
        using D6 = boost::describe::describe_enumerators<E6>;

        BOOST_TEST_EQ( mp_size<D6>::value, 6 );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 0>::value), (int)E6::v6_1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 0>::name), "v6_1" );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 1>::value), (int)E6::v6_2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 1>::name), "v6_2" );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 2>::value), (int)E6::v6_3 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 2>::name), "v6_3" );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 3>::value), (int)E6::v6_4 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 3>::name), "v6_4" );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 4>::value), (int)E6::v6_5 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 4>::name), "v6_5" );

        BOOST_TEST_EQ( (int)(mp_at_c<D6, 5>::value), (int)E6::v6_6 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D6, 5>::name), "v6_6" );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
#endif // !defined(BOOST_DESCRIBE_CXX11)
