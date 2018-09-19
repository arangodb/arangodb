/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include <iostream>
#include <map>
#include <unordered_map>
#include <boost/unordered_map.hpp>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <unordered_set>
#include <boost/unordered_set.hpp>
#include <string>
#include "test.hpp"

namespace x3 = boost::spirit::x3;

x3::rule<class pair_rule, std::pair<std::string,std::string>> const pair_rule("pair");
x3::rule<class string_rule, std::string> const string_rule("string");

auto const pair_rule_def = string_rule > x3::lit('=') > string_rule;
auto const string_rule_def = x3::lexeme[*x3::alnum];

BOOST_SPIRIT_DEFINE(pair_rule, string_rule);

template <typename Container>
void test_map_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {{"k1", "v1"}, {"k2", "v2"}};
    auto const rule = pair_rule % x3::lit(',');

    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", rule, container));
    BOOST_TEST(container.size() == 2);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = pair_rule >> ',' >> pair_rule >> ',' >> pair_rule;
    container.clear();
    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = pair_rule >> +(',' >> pair_rule);
    container.clear();
    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", cic_rule, container));
}

template <typename Container>
void test_multimap_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {{"k1", "v1"}, {"k2", "v2"}, {"k2", "v3"}};
    auto const rule = pair_rule % x3::lit(',');

    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", rule, container));
    BOOST_TEST(container.size() == 3);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = pair_rule >> ',' >> pair_rule >> ',' >> pair_rule;
    container.clear();
    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = pair_rule >> +(',' >> pair_rule);
    container.clear();
    BOOST_TEST(test_attr("k1=v1,k2=v2,k2=v3", cic_rule, container));
}

template <typename Container>
void test_sequence_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {"e1", "e2", "e2"};
    auto const rule = string_rule % x3::lit(',');

    BOOST_TEST(test_attr("e1,e2,e2", rule, container));
    BOOST_TEST(container.size() == 3);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = string_rule >> ',' >> string_rule >> ',' >> string_rule;
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = string_rule >> +(',' >> string_rule);
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", cic_rule, container));
}

template <typename Container>
void test_set_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {"e1", "e2"};
    auto const rule = string_rule % x3::lit(',');

    BOOST_TEST(test_attr("e1,e2,e2", rule, container));
    BOOST_TEST(container.size() == 2);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = string_rule >> ',' >> string_rule >> ',' >> string_rule;
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = string_rule >> +(',' >> string_rule);
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", cic_rule, container));
}

template <typename Container>
void test_multiset_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {"e1", "e2", "e2"};
    auto const rule = string_rule % x3::lit(',');

    BOOST_TEST(test_attr("e1,e2,e2", rule, container));
    BOOST_TEST(container.size() == 3);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = string_rule >> ',' >> string_rule >> ',' >> string_rule;
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = string_rule >> +(',' >> string_rule);
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", cic_rule, container));
}

template <typename Container>
void test_string_support(Container&& container)
{
    using spirit_test::test_attr;

    Container const compare {"e1e2e2"};
    auto const rule = string_rule % x3::lit(',');

    BOOST_TEST(test_attr("e1,e2,e2", rule, container));
    BOOST_TEST(container.size() == 6);
    BOOST_TEST(container == compare);

    // test sequences parsing into containers
    auto const seq_rule = string_rule >> ',' >> string_rule >> ',' >> string_rule;
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", seq_rule, container));

    // test parsing container into container
    auto const cic_rule = string_rule >> +(',' >> string_rule);
    container.clear();
    BOOST_TEST(test_attr("e1,e2,e2", cic_rule, container));
}

int
main()
{
    using x3::traits::is_associative;
    using x3::traits::is_reservable;

    static_assert(is_reservable<std::vector<int>>::value, "is_reservable problem");
    static_assert(is_reservable<std::string>::value, "is_reservable problem");
    static_assert(is_reservable<std::unordered_set<int>>::value, "is_reservable problem");
    static_assert(is_reservable<boost::unordered_set<int>>::value, "is_reservable problem");
    static_assert(is_reservable<std::unordered_multiset<int>>::value, "is_reservable problem");
    static_assert(is_reservable<boost::unordered_multiset<int>>::value, "is_reservable problem");
    static_assert(is_reservable<std::unordered_map<int,int>>::value, "is_reservable problem");
    static_assert(is_reservable<boost::unordered_map<int,int>>::value, "is_reservable problem");
    static_assert(is_reservable<std::unordered_multimap<int,int>>::value, "is_reservable problem");
    static_assert(is_reservable<boost::unordered_multimap<int,int>>::value, "is_reservable problem");

    static_assert(!is_reservable<std::deque<int>>::value, "is_reservable problem");
    static_assert(!is_reservable<std::list<int>>::value, "is_reservable problem");
    static_assert(!is_reservable<std::set<int>>::value, "is_reservable problem");
    static_assert(!is_reservable<std::multiset<int>>::value, "is_reservable problem");
    static_assert(!is_reservable<std::map<int,int>>::value, "is_reservable problem");
    static_assert(!is_reservable<std::multimap<int,int>>::value, "is_reservable problem");

    // ------------------------------------------------------------------

    static_assert(is_associative<std::set<int>>::value, "is_associative problem");
    static_assert(is_associative<std::unordered_set<int>>::value, "is_associative problem");
    static_assert(is_associative<boost::unordered_set<int>>::value, "is_associative problem");
    static_assert(is_associative<std::multiset<int>>::value, "is_associative problem");
    static_assert(is_associative<std::unordered_multiset<int>>::value, "is_associative problem");
    static_assert(is_associative<boost::unordered_multiset<int>>::value, "is_associative problem");
    static_assert(is_associative<std::map<int,int>>::value, "is_associative problem");
    static_assert(is_associative<std::unordered_map<int,int>>::value, "is_associative problem");
    static_assert(is_associative<boost::unordered_map<int,int>>::value, "is_associative problem");
    static_assert(is_associative<std::multimap<int,int>>::value, "is_associative problem");
    static_assert(is_associative<std::unordered_multimap<int,int>>::value, "is_associative problem");
    static_assert(is_associative<boost::unordered_multimap<int,int>>::value, "is_associative problem");

    static_assert(!is_associative<std::vector<int>>::value, "is_associative problem");
    static_assert(!is_associative<std::string>::value, "is_associative problem");
    static_assert(!is_associative<std::deque<int>>::value, "is_associative problem");
    static_assert(!is_associative<std::list<int>>::value, "is_associative problem");

    // ------------------------------------------------------------------

    test_string_support(std::string());

    test_sequence_support(std::vector<std::string>());
    test_sequence_support(std::list<std::string>());
    test_sequence_support(std::deque<std::string>());

    test_set_support(std::set<std::string>());
    test_set_support(std::unordered_set<std::string>());
    test_set_support(boost::unordered_set<std::string>());

    test_multiset_support(std::multiset<std::string>());
    test_multiset_support(std::unordered_multiset<std::string>());
    test_multiset_support(boost::unordered_multiset<std::string>());

    test_map_support(std::map<std::string,std::string>());
    test_map_support(std::unordered_map<std::string,std::string>());
    test_map_support(boost::unordered_map<std::string,std::string>());

    test_multimap_support(std::multimap<std::string,std::string>());
    test_multimap_support(std::unordered_multimap<std::string,std::string>());
    test_multimap_support(boost::unordered_multimap<std::string,std::string>());

    return boost::report_errors();
}
