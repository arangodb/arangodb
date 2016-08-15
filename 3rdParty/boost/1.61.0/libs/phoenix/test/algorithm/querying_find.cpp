/*=============================================================================
    Copyright (c) 2005 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2007 Hartmut Kaiser
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/stl/algorithm/querying.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/assign/list_of.hpp>

#include <boost/phoenix/config.hpp>

#if (defined (BOOST_NO_CXX11_HDR_UNORDERED_MAP) || \
     defined (BOOST_NO_CXX11_HDR_UNORDERED_SET) )
#ifdef BOOST_HAS_HASH
#define _GLIBCXX_PERMIT_BACKWARD_HASH
#include BOOST_HASH_SET_HEADER
#include BOOST_HASH_MAP_HEADER
#define BOOST_PHOENIX_HAS_HASH
#define BOOST_PHOENIX_HASH_NAMESPACE BOOST_STD_EXTENSION_NAMESPACE
#elif defined(BOOST_DINKUMWARE_STDLIB) && (BOOST_DINKUMWARE_STDLIB < 610)
#include <hash_set>
#include <hash_map>
#define BOOST_PHOENIX_HAS_HASH
#define BOOST_PHOENIX_HASH_NAMESPACE stdext
#endif
#else
#define BOOST_PHOENIX_HAS_UNDORDERED_SET_AND_MAP
#include <unordered_set>
#include <unordered_map>
#endif

#include <set>
#include <map>
#include <functional>

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
        BOOST_TEST(boost::phoenix::find(arg1,2)(array) == array + 1);

        std::set<int> s(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(s) == s.find(2));

        //#if !(defined(BOOST_MSVC) && (BOOST_MSVC >= 1900))
        //std::map<int, int> m = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).to_container(m);
        std::map<int, int> m = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
                               convert_to_container<std::map<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(m) == m.find(2));
        //#endif

#ifdef BOOST_PHOENIX_HAS_UNDORDERED_SET_AND_MAP
        std::unordered_set<int> hs(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(hs) == hs.find(2));

        //std::unordered_map<int, int> hm = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
        // convert_to_container<std::unordered_map<int, int> >();
        // BOOST_TEST(boost::phoenix::find(arg1, 2)(hm) == hm.find(2));

#else
#ifdef BOOST_PHOENIX_HAS_HASH

        BOOST_PHOENIX_HASH_NAMESPACE::hash_set<int> hs(array, array + 3);
        BOOST_TEST(boost::phoenix::find(arg1, 2)(hs) == hs.find(2));

        BOOST_PHOENIX_HASH_NAMESPACE::hash_map<int, int> hm = boost::assign::map_list_of(0, 1)(2, 3)(4, 5).
        convert_to_container<BOOST_PHOENIX_HASH_NAMESPACE::hash_map<int, int> >();
        BOOST_TEST(boost::phoenix::find(arg1, 2)(hm) == hm.find(2));

#endif
#endif
        return;
    }
}

int main()
{
    find_test();
    return boost::report_errors();
}

