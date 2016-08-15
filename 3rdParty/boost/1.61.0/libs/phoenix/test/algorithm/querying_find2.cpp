/*=============================================================================
    Copyright (c) 2005 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2007 Hartmut Kaiser
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/config.hpp>

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/stl/algorithm/querying.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/assign/list_of.hpp>

#include <set>
#include <map>
#include <functional>

#ifdef BOOST_PHOENIX_HAS_HASH
#include BOOST_PHOENIX_HASH_SET_HEADER
#include BOOST_PHOENIX_HASH_MAP_HEADER
#elif defined(BOOST_DINKUMWARE_STDLIB) && (BOOST_DINKUMWARE_STDLIB < 610)
#include <hash_set>
#include <hash_map>
#define BOOST_PHOENIX_HAS_HASH
#define BOOST_PHOENIX_HASH_NAMESPACE stdext
#elif defined BOOST_PHOENIX_HAS_UNORDERED_SET_AND_MAP
#include BOOST_PHOENIX_UNORDERED_SET_HEADER
#include BOOST_PHOENIX_UNORDERED_MAP_HEADER
#endif


namespace
{
    struct even
    {
        bool operator()(const int i) const
        {
            return i % 2 == 0;
        }
    };

    struct mod_2_comparison
    {
        bool operator()(
            const int lhs,
            const int rhs)
        {
            return lhs % 2 == rhs % 2;
        }
    };

    void find_test()
    {
        using boost::phoenix::arg_names::arg1;
        int array[] = {1,2,3};
        int marray[] = {1,1,2,3,3};
        BOOST_TEST(boost::phoenix::find(arg1,2)(array) == array + 1);

        std::set<int> s(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(s) == s.find(2));

        std::map<int, int> m = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
                               convert_to_container<std::map<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(m) == m.find(2));

#ifdef BOOST_PHOENIX_HAS_HASH

        BOOST_PHOENIX_HASH_NAMESPACE::hash_set<int> hs(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(hs) == hs.find(2));

        BOOST_PHOENIX_HASH_NAMESPACE::hash_map<int, int> hm = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
        convert_to_container<BOOST_PHOENIX_HASH_NAMESPACE::hash_map<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(hm) == hm.find(2));

#elif defined BOOST_PHOENIX_HAS_UNORDERED_SET_AND_MAP

        std::unordered_set<int> us(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(us) == us.find(2));

        std::unordered_multiset<int> ums(marray, marray + 5);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(ums) == ums.find(2));

        std::unordered_map<int, int> um =
             boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
             convert_to_container<std::unordered_map<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(um) == um.find(2));

        std::unordered_multimap<int, int> umm =
             boost::assign::map_list_of(0, 1)(2, 3)(4, 5)(4, 6).
             convert_to_container<std::unordered_multimap<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(umm) == umm.find(2));

#endif
        return;
    }
}

int main()
{
    find_test();
    return boost::report_errors();
}

