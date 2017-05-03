// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef GEOMETRY_TEST_ITERATORS_TEST_ITERATOR_COMMON_HPP
#define GEOMETRY_TEST_ITERATORS_TEST_ITERATOR_COMMON_HPP

#include <cstddef>
#include <iostream>
#include <iterator>
#include <string>
#include <algorithm>

#include <boost/test/included/unit_test.hpp>

// helper functions for testing the concatenate and flatten iterators

template <typename Iterator>
inline std::ostream& print_container(std::ostream& os,
                                     Iterator begin, Iterator end,
                                     std::string const& header)
{
    std::cout << header;
    for (Iterator it = begin; it != end; ++it)
    {
        os << " " << *it;
    }
    return os;
}


template <typename Iterator>
inline std::ostream& print_nested_container(std::ostream& os,
                                            Iterator begin, Iterator end,
                                            std::string const& header)
{
    typedef typename std::iterator_traits<Iterator>::value_type inner_container;
    typedef typename inner_container::const_iterator inner_iterator;

    std::cout << header;
    for (Iterator outer = begin; outer != end; ++outer)
    {
        os << " (";
        for (inner_iterator inner = outer->begin();
             inner != outer->end(); ++inner)
        {
            if ( inner != outer->begin() )
            {
                os << " ";
            }
            os << *inner;
        }
        os << ") ";
    }
    return os;
}


template <typename T>
struct is_odd
{
    inline bool operator()(T const& t) const
    {
        return t % 2 != 0;
    }
};


template <typename T>
struct is_even
{
    inline bool operator()(T const& t) const
    {
        return !is_odd<T>()(t);
    }
};




template <typename CombinedIterator, typename CombinedContainer>
inline void test_size(CombinedIterator first, CombinedIterator beyond,
                      CombinedContainer const& combined)
{
    std::size_t size = static_cast<std::size_t>(std::distance(first, beyond));
    BOOST_CHECK( combined.size() == size );

    size = 0;
    for (CombinedIterator it = first; it != beyond; ++it)
    {
        ++size;
    }
    BOOST_CHECK( combined.size() == size );

    size = 0;
    for (CombinedIterator it = beyond; it != first; --it)
    {
        ++size;
    }
    BOOST_CHECK( combined.size() == size );
}



template <typename CombinedIterator, typename CombinedContainer>
inline void test_equality(CombinedIterator first, CombinedIterator beyond,
                          CombinedContainer const& combined)
{
    typedef typename CombinedContainer::const_iterator iterator;

    iterator it = combined.begin();
    for (CombinedIterator cit = first; cit != beyond; ++cit, ++it)
    {
        BOOST_CHECK( *cit == *it );
    }

    if ( combined.begin() != combined.end() )
    {
        BOOST_CHECK( first != beyond );
        iterator it = combined.end();
        CombinedIterator cit = beyond;
        for (--cit, --it; cit != first; --cit, --it)
        {
            BOOST_CHECK( *cit == *it );
        }
        BOOST_CHECK( cit == first && it == combined.begin() );
        BOOST_CHECK( *cit == *it );
    }
    else
    {
        BOOST_CHECK( first == beyond );
    }
}



template <typename CombinedIterator, typename CombinedContainer>
inline void test_using_remove_if(CombinedIterator first,
                                 CombinedIterator beyond,
                                 CombinedContainer& combined)
{
    typedef typename std::iterator_traits
        <
            CombinedIterator
        >::value_type value_type;
    
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "odd elements removed:" << std::endl;
    print_container(std::cout, first, beyond, "before:") << std::endl;
#endif
    typename std::iterator_traits<CombinedIterator>::difference_type
        num_even = std::count_if(first, beyond, is_even<value_type>());

    CombinedIterator new_beyond =
        std::remove_if(first, beyond, is_odd<value_type>());

    std::size_t new_size = std::distance(first, new_beyond);

    for (CombinedIterator it = first; it != new_beyond; ++it)
    {
        BOOST_CHECK( !is_odd<value_type>()(*it) );
    }
    BOOST_CHECK( new_size == static_cast<std::size_t>(num_even) );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    print_container(std::cout, first, new_beyond, "after :") << std::endl;
#endif

    combined.erase(std::remove_if(combined.begin(), combined.end(),
                                  is_odd<value_type>()),
                   combined.end());
    test_equality(first, new_beyond, combined);
}


template <typename CombinedIterator, typename CombinedContainer>
inline void test_using_reverse(CombinedIterator first,
                               CombinedIterator beyond,
                               CombinedContainer& combined)
{
    std::reverse(first, beyond);
    std::reverse(combined.begin(), combined.end());
    test_equality(first, beyond, combined);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    print_container(std::cout, first, beyond, "reversed:") << std::endl;
#endif

    std::reverse(first, beyond);
    std::reverse(combined.begin(), combined.end());
    test_equality(first, beyond, combined);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    print_container(std::cout, first, beyond, "re-reversed:") << std::endl;
#endif
}


#endif // GEOMETRY_TEST_ITERATORS_TEST_ITERATOR_COMMON_HPP
