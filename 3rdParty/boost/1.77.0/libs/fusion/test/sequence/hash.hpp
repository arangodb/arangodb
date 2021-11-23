/*=============================================================================
    Copyright (c) 2014 Christoph Weiss
    Copyright (c) 2017 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <string>
#include <utility>

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/sequence/hash.hpp>
#include <boost/functional/hash.hpp>

void hash_test()
{
    namespace fusion = boost::fusion;
    using namespace fusion;

    const FUSION_SEQUENCE<int, char, bool, std::string> v0(42, 'x', false, "Aurea prima");
    const FUSION_SEQUENCE<int, char, bool, std::string> v1(42, 'x', false, "Aurea prima");
    BOOST_TEST(fusion::hash_value(v0) == fusion::hash_value(v1));

    const FUSION_SEQUENCE<int, char, bool, std::string> w(41, 'x', false, "Aurea prima");
    BOOST_TEST(fusion::hash_value(w) != fusion::hash_value(v0));

    const FUSION_SEQUENCE<int, char, bool, std::string> x(42, 'y', false, "Aurea prima");
    BOOST_TEST(fusion::hash_value(x) != fusion::hash_value(v0));

    const FUSION_SEQUENCE<int, char, bool, std::string> y(42, 'x', true, "Aurea prima");
    BOOST_TEST(fusion::hash_value(y) != fusion::hash_value(v0));

    const FUSION_SEQUENCE<int, char, bool, std::string> z(42, 'x', false, "quae vindice nullo");
    BOOST_TEST(fusion::hash_value(z) != fusion::hash_value(v0));
}
