// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_flatten_iterator
#endif

#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <list>
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

#include <boost/assign/std/vector.hpp>
#include <boost/assign/std/list.hpp>
#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/iterators/flatten_iterator.hpp>

#include "test_iterator_common.hpp"


using namespace boost::assign;


template <typename InnerContainer>
struct access_begin
{
    using return_type = std::conditional_t
        <
            std::is_const<InnerContainer>::value,
            typename InnerContainer::const_iterator,
            typename InnerContainer::iterator
        >;

    static inline return_type apply(InnerContainer& inner)
    {
        return inner.begin();
    }
};


template <typename InnerContainer>
struct access_end
{
    using return_type = std::conditional_t
        <
            std::is_const<InnerContainer>::value,
            typename InnerContainer::const_iterator,
            typename InnerContainer::iterator
        >;

    static inline return_type apply(InnerContainer& inner)
    {
        return inner.end();
    }
};


template <typename NestedContainer>
inline std::size_t number_of_elements(NestedContainer const& c)
{
    std::size_t num_elements(0);
    for (typename NestedContainer::const_iterator outer = c.begin();
         outer != c.end(); ++outer)
    {
        num_elements += outer->size();
    }
    return num_elements;
}


struct test_flatten_iterator
{
    template
    <
        typename FlattenIterator,
        typename ConstFlattenIterator,
        typename NestedContainer
    >
    static inline
    void test_using_max_element(FlattenIterator first,
                                FlattenIterator beyond,
                                ConstFlattenIterator const_first,
                                ConstFlattenIterator const_beyond,
                                NestedContainer const& c)
    {
        typedef typename std::iterator_traits
            <
                FlattenIterator
            >::value_type value_type;

        typedef typename NestedContainer::const_iterator const_outer_iterator;
        typedef typename NestedContainer::value_type inner_container;
        typedef typename inner_container::const_iterator const_inner_iterator;

        if ( first == beyond )
        {
            return;
        }

        FlattenIterator it_max = std::max_element(first, beyond);
        ConstFlattenIterator const_it_max =
            std::max_element(const_first, const_beyond);

        BOOST_CHECK( it_max == const_it_max );
        BOOST_CHECK( *it_max == *const_it_max );

        value_type old_value = *const_first;
        value_type new_value = *it_max + 1;

        *first = *it_max + 1;
        const_outer_iterator outer = c.begin();
        while ( outer->begin() == outer->end() )
        {
            ++outer;
        }
        const_inner_iterator inner = outer->begin();
            
        BOOST_CHECK( *inner == new_value );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << "modified 1st element of 1st non-empty "
                  << "inner container:" << std::endl;
        print_nested_container(std::cout, c.begin(), c.end(), "nested   :")
            << std::endl;
        print_container(std::cout, first, beyond, "flattened:") << std::endl;
#endif

        *first = old_value;
        BOOST_CHECK( *inner == old_value );
    }

    template <typename NestedContainer>
    static inline void apply(NestedContainer& c,
                             std::string const& case_id,
                             std::string const& container_id)
    {
        boost::ignore_unused(case_id, container_id);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::stringstream sstream;
        sstream << case_id << " [" << container_id << "]";

        std::cout << "case id: " << sstream.str() << std::endl;
#endif
        typedef typename NestedContainer::const_iterator const_outer_iterator;
        typedef typename NestedContainer::iterator outer_iterator;
        typedef typename NestedContainer::value_type inner_container;

        typedef typename inner_container::const_iterator const_inner_iterator;
        typedef typename inner_container::iterator inner_iterator;

        typedef boost::geometry::flatten_iterator
            <
                const_outer_iterator,
                const_inner_iterator,
                typename inner_container::value_type const,
                access_begin<inner_container const>,
                access_end<inner_container const>
            > const_flatten_iterator;

        typedef boost::geometry::flatten_iterator
            <
                outer_iterator,
                inner_iterator,
                typename inner_container::value_type,
                access_begin<inner_container>,
                access_end<inner_container>
            > flatten_iterator;

        typedef typename std::iterator_traits
            <
                flatten_iterator
            >::value_type value_type;

        flatten_iterator begin(c.begin(), c.end());
        flatten_iterator end(c.end());
        const_flatten_iterator const_begin(begin);
        const_flatten_iterator const_end(end);
        const_begin = begin;
        const_end = end;

        // test copying, dereferencing and element equality
        std::vector<value_type> combined;
        for (const_outer_iterator outer = c.begin();
             outer != c.end(); ++outer)
        {
            std::copy(outer->begin(), outer->end(),
                      std::back_inserter(combined));
        }
        test_equality(begin, end, combined);
        test_equality(const_begin, const_end, combined);        

        combined.clear();
        std::copy(begin, end, std::back_inserter(combined));
        test_equality(begin, end, combined);
        test_equality(const_begin, const_end, combined);

        combined.clear();
        std::copy(const_begin, const_end, std::back_inserter(combined));
        test_equality(begin, end, combined);
        test_equality(const_begin, const_end, combined);

        // test sizes (and std::distance)
        test_size(begin, end, combined);
        test_size(const_begin, const_end, combined);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        print_nested_container(std::cout, c.begin(), c.end(), "nested    :")
            << std::endl;
        print_container(std::cout, begin, end, "flattened :")
            << std::endl;

        if ( begin != end )
        {
            std::cout << "min element: "
                      << *std::min_element(begin, end)
                      << std::endl;
            std::cout << "max element: "
                      << *std::max_element(const_begin, const_end)
                      << std::endl;
        }
#endif

        // perform reversals (std::reverse)
        test_using_reverse(begin, end, combined);

        // test std::max_element, dereferencing and value assigment
        test_using_max_element(begin, end, const_begin, const_end, c);

        // test std::count_if / std::remove_if
        test_using_remove_if(begin, end, combined);
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "====================" << std::endl << std::endl;
#endif
    }
};



// the actual test cases -- START
template <int CaseNumber>
struct test_case_per_container;

template<>
struct test_case_per_container<0>
{
    template <typename NestedContainer>
    static inline void apply(std::string const& case_id,
                             std::string const& container_id)
    {
        NestedContainer c;
        test_flatten_iterator::apply(c, case_id, container_id);
    }
};

template<>
struct test_case_per_container<1>
{
    template <typename NestedContainer>
    static inline void apply(std::string const& case_id,
                             std::string const& container_id)
    {
        NestedContainer c;
        for (int i = 0; i < 5; ++i)
        {
            c += typename NestedContainer::value_type();
        }
        test_flatten_iterator::apply(c, case_id, container_id);
    }
};

template<>
struct test_case_per_container<2>
{
    template <typename NestedContainer>
    static inline void apply(std::string const& case_id,
                             std::string const& container_id)
    {
        NestedContainer c;
        typename NestedContainer::value_type ic[4];

        ic[0] += 5,4,3,2,1;
        ic[1] += 6,7,8;
        ic[2] += 9;
        ic[3] += 9,8,7,6,5;
        c += ic[0],ic[1],ic[2],ic[3];

        test_flatten_iterator::apply(c, case_id, container_id);
    }
};

template<>
struct test_case_per_container<3>
{
    template <typename NestedContainer>
    static inline void apply(std::string const& case_id,
                             std::string const& container_id)
    {
        NestedContainer c;
        typename NestedContainer::value_type ic[20];

        ic[2] += 5,4,3,2,1;
        ic[3] += 6,7,8;
        ic[8] += 9;
        ic[9] += 9,8,7,6,5;
        ic[14] += 4,3,2,1;
        for (std::size_t i = 0; i < 20; ++i)
        {
            c += ic[i];
        }

        test_flatten_iterator::apply(c, case_id, container_id);
    }
};
// the actual test cases -- END



template <int CaseNumber>
inline void test_case_all_containers(std::string const& case_id)
{
    typedef typename std::vector<std::vector<int> > VV;
    typedef typename std::vector<std::list<int> > VL;
    typedef typename std::list<std::vector<int> > LV;
    typedef typename std::list<std::list<int> > LL;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
#endif
    test_case_per_container<CaseNumber>::template apply<VV>(case_id, "VV");
    test_case_per_container<CaseNumber>::template apply<VL>(case_id, "VL");
    test_case_per_container<CaseNumber>::template apply<LV>(case_id, "LV");
    test_case_per_container<CaseNumber>::template apply<LL>(case_id, "LL");

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "********************************************************"
              << std::endl << std::endl;
#endif
}



BOOST_AUTO_TEST_CASE( test_flatten_iterator_all )
{
    test_case_all_containers<0>("empty");
    test_case_all_containers<1>("case1");
    test_case_all_containers<2>("case2");
    test_case_all_containers<3>("case3");
}
