// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <laws/base.hpp>

#include <string>
#include <utility>
namespace hana = boost::hana;
namespace test = hana::test;


struct MoveOnly {
    int data_;
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;
    MoveOnly(int data = 1) : data_(data) { }
    MoveOnly(MoveOnly&& x) : data_(x.data_) { x.data_ = 0; }

    MoveOnly& operator=(MoveOnly&& x)
    { data_ = x.data_; x.data_ = 0; return *this; }

    int get() const {return data_;}
    bool operator==(const MoveOnly& x) const { return data_ == x.data_; }
    bool operator< (const MoveOnly& x) const { return data_ <  x.data_; }
};

int main() {
    {
        using Map = hana::map<>;
        Map map0;
        Map map;
        map0 = std::move(map);
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, MoveOnly>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{999}));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{4}));
        map0 = std::move(map);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == MoveOnly{4});
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, MoveOnly>,
                              hana::pair<test::ct_eq<1>, MoveOnly>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{999}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{888}));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{4}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{5}));
        map0 = std::move(map);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == MoveOnly{4});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<1>{}) == MoveOnly{5});
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, MoveOnly>,
                              hana::pair<test::ct_eq<1>, MoveOnly>,
                              hana::pair<test::ct_eq<2>, MoveOnly>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{999}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{888}),
                                  hana::make_pair(test::ct_eq<2>{}, MoveOnly{777}));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{4}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{5}),
                                  hana::make_pair(test::ct_eq<2>{}, MoveOnly{6}));
        map0 = std::move(map);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == MoveOnly{4});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<1>{}) == MoveOnly{5});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<2>{}) == MoveOnly{6});
    }
    {
        using Map = hana::map<hana::pair<test::ct_eq<0>, MoveOnly>,
                              hana::pair<test::ct_eq<1>, MoveOnly>,
                              hana::pair<test::ct_eq<2>, MoveOnly>,
                              hana::pair<test::ct_eq<3>, std::string>>;
        Map map0 = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{999}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{888}),
                                  hana::make_pair(test::ct_eq<2>{}, MoveOnly{777}),
                                  hana::make_pair(test::ct_eq<3>{}, std::string{"zzzzz"}));
        Map map  = hana::make_map(hana::make_pair(test::ct_eq<0>{}, MoveOnly{4}),
                                  hana::make_pair(test::ct_eq<1>{}, MoveOnly{5}),
                                  hana::make_pair(test::ct_eq<2>{}, MoveOnly{6}),
                                  hana::make_pair(test::ct_eq<3>{}, std::string{"abc"}));
        map0 = std::move(map);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<0>{}) == MoveOnly{4});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<1>{}) == MoveOnly{5});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<2>{}) == MoveOnly{6});
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(map0, test::ct_eq<3>{}) == std::string{"abc"});
    }
}
