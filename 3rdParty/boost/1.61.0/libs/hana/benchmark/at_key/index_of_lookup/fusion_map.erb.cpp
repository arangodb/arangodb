// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>

#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/make_map.hpp>
namespace hana = boost::hana;
namespace fusion = boost::fusion;


struct undefined { };

int main() {
    auto map = fusion::make_map<
        <%= env[:range].map { |n| "hana::int_<#{n}>" }.join(', ') %>
    >(
        <%= env[:range].map { |n| "undefined{}" }.join(', ') %>
    );

    fusion::at_key<hana::int_<<%= input_size %>>>(map);
}
