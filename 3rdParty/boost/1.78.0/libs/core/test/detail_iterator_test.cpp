//
// Test for boost/detail/iterator.hpp
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#define BOOST_ALLOW_DEPRECATED_HEADERS
#include <boost/detail/iterator.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstddef>
#include <list>

/*

namespace boost {
namespace detail {

template <class Iterator> struct iterator_traits: std::iterator_traits<Iterator>
{
};

using std::distance;

} // namespace detail
} // namespace boost

*/

// struct C {} doesn't wotk with libc++.
typedef std::forward_iterator_tag C;

struct T
{
};

struct D
{
};

struct P
{
};

struct R
{
};

template< class Category, class T, class Distance = std::ptrdiff_t, class Pointer = T*, class Reference = T& >
struct iterator
{
    typedef T value_type;
    typedef Distance difference_type;
    typedef Pointer pointer;
    typedef Reference reference;
    typedef Category iterator_category;
};

int main()
{
    using boost::core::is_same;

/*
    template<class Iterator> struct iterator_traits {
        typedef typename Iterator::difference_type difference_type;
        typedef typename Iterator::value_type value_type;
        typedef typename Iterator::pointer pointer;
        typedef typename Iterator::reference reference;
        typedef typename Iterator::iterator_category iterator_category;
    };
*/
    {
        typedef ::iterator<C,T,D,P,R> It;

        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::iterator_category,C>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::value_type,T>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::difference_type,D>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::pointer,P>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::reference,R>));
    }
/*
    template<class T> struct iterator_traits<T*> {
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef random_access_iterator_tag iterator_category;
    };
*/
    {
        typedef T* It;

        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::iterator_category,std::random_access_iterator_tag>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::value_type,T>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::difference_type,std::ptrdiff_t>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::pointer,T*>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::reference,T&>));
    }

/*
    template<class T> struct iterator_traits<const T*> {
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef random_access_iterator_tag iterator_category;
    };
*/
    {
        typedef T const* It;

        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::iterator_category,std::random_access_iterator_tag>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::value_type,T>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::difference_type,std::ptrdiff_t>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::pointer,T const*>));
        BOOST_TEST_TRAIT_TRUE((is_same<boost::detail::iterator_traits<It>::reference,T const&>));
    }
/*
    template<class InputIterator>
    typename iterator_traits<InputIterator>::difference_type
    distance( InputIterator first, InputIterator last );
*/
    {
        int const N = 5;
        T x[ N ] = {};

        BOOST_TEST_EQ( boost::detail::distance( x, x + N ), N );
    }

    {
        int const N = 5;
        T const x[ N ] = {};

        BOOST_TEST_EQ( boost::detail::distance( x, x + N ), N );
    }

    {
        int const N = 5;
        std::list<T> x( N );

        BOOST_TEST_EQ( boost::detail::distance( x.begin(), x.end() ), x.size() );
    }

    return boost::report_errors();
}
