// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/to.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/core/tag_of.hpp>

#include <string>
#include <type_traits>
namespace hana = boost::hana;


template <typename X, typename Y>
constexpr auto operator==(X x, Y y)
{ return x.value == y.value; }

struct Datatype {
    int value;
    using hana_tag = Datatype;
};

struct Other {
    int value;
    using hana_tag = Datatype;
};

struct SpecializedFrom;
struct specialized_from {
    int value;
    using hana_tag = SpecializedFrom;
};

struct SpecializedTo;
struct specialized_to {
    int value;
    using hana_tag = SpecializedTo;
};

namespace boost { namespace hana {
    template <>
    struct to_impl<SpecializedTo, SpecializedFrom> {
        template <typename T>
        static constexpr auto apply(T t)
        { return specialized_to{t.value}; }
    };
}}

template <typename F, typename T>
void check_convert(F f, T t) {
    using From = hana::tag_of_t<F>;
    using To = hana::tag_of_t<T>;

    // Check From -> To conversion
    BOOST_HANA_RUNTIME_CHECK(hana::to<To>(f) == t);
    static_assert(std::is_same<
        hana::tag_of_t<decltype(hana::to<To>(f))>, To
    >{}, "");

    static_assert(hana::is_convertible<From, To>{}, "");


    // Make sure From -> From and To -> To are the identity.
    BOOST_HANA_RUNTIME_CHECK(hana::to<From>(f) == f);
    static_assert(std::is_same<
        hana::tag_of_t<decltype(hana::to<From>(f))>, From
    >{}, "");

    BOOST_HANA_RUNTIME_CHECK(hana::to<To>(t) == t);
    static_assert(std::is_same<
        hana::tag_of_t<decltype(hana::to<To>(t))>, To
    >{}, "");

    static_assert(hana::is_convertible<From, From>{}, "");
    static_assert(hana::is_convertible<To, To>{}, "");

    static_assert(hana::is_embedded<From, From>{}, "");
    static_assert(hana::is_embedded<To, To>{}, "");
}

template <typename X>
void check_variable_template_in_dependent_context(X x) {
    hana::to<int>(x);
}

int main() {
    // Clang used to assert in the code generation when we used variable
    // templates inside a lambda; this is to catch this.
    check_variable_template_in_dependent_context(3);

    check_convert("abcdef", std::string{"abcdef"});
    check_convert(int{1}, double{1});
    check_convert(double{1}, int{1});
    check_convert(std::true_type{}, int{1});
    check_convert(std::false_type{}, int{0});
    check_convert(Datatype{1}, Datatype{1});
    check_convert(Other{1}, Other{1});
    check_convert(specialized_from{1}, specialized_to{1});

    static_assert(!hana::is_convertible<void, int>{}, "");
    static_assert(!hana::is_embedded<void, int>{}, "");

    static_assert(hana::is_convertible<int, void>{}, "");
    static_assert(!hana::is_embedded<int, void>{}, "");
}
