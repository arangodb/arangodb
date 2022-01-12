/*=============================================================================
    Copyright (c) 2001-2017 Joel de Guzman
    Copyright (c) 2017 think-cell GmbH

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <iostream>
#include <string>
#include <functional>

int main()
{
    using boost::adaptors::transform;
    using boost::spirit::x3::raw;
    using boost::spirit::x3::eps;
    using boost::spirit::x3::eoi;
    using boost::spirit::x3::upper;
    using boost::spirit::x3::repeat;
    using boost::spirit::x3::parse;

    std::string input = "abcde";
    std::function<char(char)> func = [](char c) {
        return c < 'a' || 'z' < c ? c : static_cast<char>(c - 'a' + 'A');
    };
    auto const rng = transform(input, func);

    {
        std::string str;
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), +upper >> eoi, str)));
        BOOST_TEST(("ABCDE"==str));
    }

    {
        boost::iterator_range<decltype(boost::begin(rng))> str;
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), raw[+upper >> eoi], str)));
        BOOST_TEST((boost::equal(std::string("ABCDE"), str)));
    }

    {
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), (repeat(6)[upper] | repeat(5)[upper]) >> eoi)));
    }

    return boost::report_errors();
}
