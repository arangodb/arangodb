// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>
#include <utility>

class X
{
private:

    std::pair<int, int> p_;

public:

    std::pair<int, int>& f() { return p_; }
    std::pair<int, int> const& f() const { return p_; }

    BOOST_DESCRIBE_CLASS(X, (), ((std::pair<int, int>& ()) f, (std::pair<int, int> const& () const) f), (), (p_))
};


#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

#include <boost/mp11.hpp>

int main()
{
    using namespace boost::describe;
    using namespace boost::mp11;

    {
        using L1 = describe_members<X, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L1>::value, 1 );

        using D1 = mp_at_c<L1, 0>;

        BOOST_TEST_CSTR_EQ( D1::name, "p_" );
        BOOST_TEST_EQ( D1::modifiers, mod_private );

        X x;

        auto& p = x.*D1::pointer;

        using L2 = describe_members<X, mod_any_access | mod_function>;

        BOOST_TEST_EQ( mp_size<L2>::value, 2 );

        using D2 = mp_at_c<L2, 0>;
        using D3 = mp_at_c<L2, 1>;

        BOOST_TEST_EQ( &(x.*D2::pointer)(), &p );
        BOOST_TEST_CSTR_EQ( D2::name, "f" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_function );

        BOOST_TEST_EQ( &(x.*D3::pointer)(), &p );
        BOOST_TEST_CSTR_EQ( D3::name, "f" );
        BOOST_TEST_EQ( D3::modifiers, mod_public | mod_function );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
