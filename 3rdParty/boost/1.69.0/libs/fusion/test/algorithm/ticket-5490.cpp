/*=============================================================================
    Copyright (c) 2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/transform.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/core/ignore_unused.hpp>

using namespace boost::fusion;

template <typename>
struct predicate {};

struct unique {};

template <typename>
struct meta_func
{
    typedef unique result_type;

    template <typename T>
    unique operator()(const T&) const;
};

int main()
{
    vector<int> v;

    typedef predicate<boost::mpl::_1> lambda_t;
    typedef boost::mpl::quote1<predicate> quote_t;

    vector<unique> l = transform(v, meta_func<lambda_t>());

    vector<unique> q = transform(v, meta_func<quote_t>());

    boost::ignore_unused(l, q);
}
