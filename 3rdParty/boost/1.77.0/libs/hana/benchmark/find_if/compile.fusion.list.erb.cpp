// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

<% if input_size > 10 %>
    #define FUSION_MAX_LIST_SIZE <%= ((input_size + 9) / 10) * 10 %>
<% end %>

#include <boost/fusion/include/find_if.hpp>
#include <boost/fusion/include/make_list.hpp>
#include <boost/mpl/integral_c.hpp>
namespace fusion = boost::fusion;
namespace mpl = boost::mpl;


struct is_last {
    template <typename N>
    struct apply
        : mpl::integral_c<bool, N::type::value == <%= input_size %>>
    { };
};

int main() {
    auto ints = fusion::make_list(
        <%= (1..input_size).map { |n| "mpl::integral_c<int, #{n}>{}" }.join(', ') %>
    );

    auto result = fusion::find_if<is_last>(ints);
    (void)result;
}
