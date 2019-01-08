// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <laws/base.hpp>

#include <string>
namespace hana = boost::hana;
namespace test = hana::test;


int main() {
    {
        using Map = hana::map<>;
        Map map0;
        Map map;
        map0 = map;
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, int>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 999));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 4));
        map0 = map;
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == 4);
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, int>,
                              hana::pair<test::ct_eq<1>, char>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 999),
                                  hana::make_pair(test::ct_eq<1>{}, 'z'));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 4),
                                  hana::make_pair(test::ct_eq<1>{}, 'a'));
        map0 = map;
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == 4);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<1>{}) == 'a');
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, int>,
                              hana::pair<test::ct_eq<1>, char>,
                              hana::pair<test::ct_eq<2>, std::string>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 999),
                                  hana::make_pair(test::ct_eq<1>{}, 'z'),
                                  hana::make_pair(test::ct_eq<2>{}, std::string{"zzzzzzzz"}));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, 4),
                                  hana::make_pair(test::ct_eq<1>{}, 'a'),
                                  hana::make_pair(test::ct_eq<2>{}, std::string{"abc"}));
        map0 = map;
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == 4);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<1>{}) == 'a');
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<2>{}) == "abc");
    }
}
