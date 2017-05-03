// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
namespace hana = boost::hana;


struct NoDefault {
    NoDefault() = delete;
    explicit constexpr NoDefault(int) { }
};

struct DefaultOnly {
    DefaultOnly() = default;
    DefaultOnly(DefaultOnly const&) = delete;
    DefaultOnly(DefaultOnly&&) = delete;
};

struct NonConstexprDefault {
    NonConstexprDefault() { }
};

int main() {
    {
        hana::pair<float, short*> p;
        BOOST_HANA_RUNTIME_CHECK(hana::first(p) == 0.0f);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p) == nullptr);
    }

    // make sure it also works constexpr
    {
        constexpr hana::pair<float, short*> p;
        static_assert(hana::first(p) == 0.0f, "");
        static_assert(hana::second(p) == nullptr, "");
    }

    // make sure the default constructor is not instantiated when the
    // members of the pair are not default-constructible
    {
        hana::pair<NoDefault, NoDefault> p{NoDefault{1}, NoDefault{2}};
        (void)p;
    }

    // make sure it works when only the default constructor is defined
    {
        hana::pair<DefaultOnly, DefaultOnly> p;
        (void)p;
    }

    {
        hana::pair<NonConstexprDefault, NonConstexprDefault> p;
        (void)p;
    }
}
