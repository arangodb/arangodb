
// Copyright 2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/detail/allocator_utilities.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/is_same.hpp>
#include <memory>

typedef std::allocator<int> std_int_allocator;
typedef boost::detail::allocator::rebind_to<std_int_allocator, char>::type char_allocator;
typedef boost::detail::allocator::rebind_to<char_allocator, int>::type int_allocator;
typedef boost::detail::allocator::rebind_to<int_allocator, char>::type char_allocator2;

int main()
{
    BOOST_STATIC_ASSERT((!boost::is_same<char_allocator, int_allocator>::value));
    BOOST_STATIC_ASSERT((boost::is_same<char_allocator, char_allocator2>::value));

    // Check the constructors works okay
    std_int_allocator a1;
    char_allocator a2(a1);
    char_allocator a2a(a2);
    int_allocator a3(a2);

    // Check allocate works okay
    {
        boost::allocator_pointer<char_allocator>::type p = a2.allocate(10);
        BOOST_TEST(!!p);
        a2.deallocate(p, 10);
    }

    // Try using the standalone construct/destroy
    {
        boost::allocator_pointer<int_allocator>::type p2 = a3.allocate(1);
        boost::detail::allocator::construct(p2, 25);
        BOOST_TEST(*p2 == 25);
        boost::detail::allocator::destroy(p2);
        a3.deallocate(p2, 1);
    }

    return boost::report_errors();
}
