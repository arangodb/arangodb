// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

<% if input_size > 10 %>
    #define FUSION_MAX_LIST_SIZE <%= ((input_size + 9) / 10) * 10 %>
<% end %>

<% if input_size > 6 %>
    #define BOOST_FUSION_INVOKE_MAX_ARITY <%= input_size %>
<% end %>

#include <boost/fusion/include/invoke.hpp>
#include <boost/fusion/include/make_list.hpp>
namespace fusion = boost::fusion;


struct f {
    template <typename ...T>
    constexpr void operator()(T const& ...) const { }
};

template <int i>
struct x { };

int main() {
    auto xs = fusion::make_list(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    fusion::invoke(f{}, xs);
}
