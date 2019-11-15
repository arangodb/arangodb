// Copyright Jason Rice 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/type.hpp>

#include <regex>
#include <sstream>
namespace hana = boost::hana;

namespace foo {
    template <typename T> struct my_template { };
    template <typename ...> struct my_mf { struct type; };
    struct my_mf_class { template <typename ...> struct apply { struct type; }; };
}

int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::template_<foo::my_template>
        );
        BOOST_HANA_RUNTIME_CHECK(std::regex_match(ss.str(),
            std::regex("template<(?:struct )?foo::my_template>")));
    }
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::metafunction<foo::my_mf>
        );
        BOOST_HANA_RUNTIME_CHECK(std::regex_match(ss.str(),
            std::regex("metafunction<(?:struct )?foo::my_mf>")));
    }
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::metafunction_class<foo::my_mf_class>
        );
        BOOST_HANA_RUNTIME_CHECK(std::regex_match(ss.str(),
            std::regex("metafunction_class<(?:struct )?foo::my_mf_class>")));
    }
}
