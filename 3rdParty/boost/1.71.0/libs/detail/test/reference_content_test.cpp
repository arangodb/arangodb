
// Copyright 2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/detail/reference_content.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

BOOST_STATIC_ASSERT((boost::is_same<int, boost::detail::make_reference_content<int>::type>::value));
BOOST_STATIC_ASSERT((boost::is_same<boost::detail::reference_content<int&>, boost::detail::make_reference_content<int&>::type>::value));
BOOST_STATIC_ASSERT((boost::is_same<int, boost::detail::make_reference_content<>::apply<int>::type>::value));
BOOST_STATIC_ASSERT((boost::is_same<boost::detail::reference_content<int&>, boost::detail::make_reference_content<>::apply<int&>::type>::value));
BOOST_STATIC_ASSERT((boost::has_nothrow_copy<boost::detail::make_reference_content<int&>::type>::value));

int main() {}
