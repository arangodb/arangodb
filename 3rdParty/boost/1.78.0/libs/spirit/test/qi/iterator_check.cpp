/*=============================================================================
    Copyright (c) 2001-2017 Joel de Guzman
    Copyright (c) 2017 think-cell GmbH

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/qi.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <iostream>
#include <string>
#include <functional>

namespace {
    char transform_func(char c) {
        return c < 'a' || 'z' < c ? c : static_cast<char>(c - 'a' + 'A');
    }
}

int main()
{
    using boost::adaptors::transform;
    using boost::spirit::qi::raw;
    using boost::spirit::qi::eps;
    using boost::spirit::qi::eoi;
    using boost::spirit::qi::upper;
    using boost::spirit::qi::repeat;
    using boost::spirit::qi::parse;

    std::string input = "abcde";
    boost::transformed_range<char(*)(char), std::string> const rng = transform(input, transform_func);

    {
        std::string str;
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), +upper >> eoi, str)));
        BOOST_TEST(("ABCDE"==str));
    }

    {
        boost::iterator_range<boost::range_iterator<boost::transformed_range<char(*)(char), std::string> const>::type> str;
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), raw[+upper >> eoi], str)));
        BOOST_TEST((boost::equal(std::string("ABCDE"), str)));
    }

    {
        BOOST_TEST((parse(boost::begin(rng), boost::end(rng), (repeat(6)[upper] | repeat(5)[upper]) >> eoi)));
    }

    return boost::report_errors();
}
