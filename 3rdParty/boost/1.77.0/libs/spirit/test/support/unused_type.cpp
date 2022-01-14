/*=============================================================================
    Copyright (c) 2019 Nikita Kniazev

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/home/support/unused.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#if !defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) && \
    !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
#include <type_traits>
static_assert(std::is_trivial<boost::spirit::unused_type>::value, "");
#endif

void test_use(boost::spirit::unused_type) {}

template <typename Expected, typename T>
void test(T&)
{
    BOOST_STATIC_ASSERT((boost::is_same<T&, Expected>::value));
}

int main()
{
    using boost::spirit::unused;
    using boost::spirit::unused_type;

    unused_type unused_mut;
    test<unused_type const&>(unused);
    test<unused_type&>(unused_mut);
    test<unused_type const&>(unused = 123);
    test<unused_type const&>(unused = *&unused);
    test<unused_type const&>(unused = unused_mut);
    test<unused_type&>(unused_mut = 123);
    test<unused_type&>(unused_mut = unused);
    test<unused_type&>(unused_mut = *&unused_mut);

    test_use(0);
    test_use(unused);
    test_use(unused_mut);
}
