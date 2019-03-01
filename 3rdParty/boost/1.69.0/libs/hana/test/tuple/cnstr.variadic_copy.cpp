// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;


template <typename ...>
struct never { static constexpr bool value = false; };

struct NoValueCtor {
    NoValueCtor() : id(++count) {}
    NoValueCtor(NoValueCtor const & other) : id(other.id) { ++count; }

    // The constexpr is required to make is_constructible instantiate this
    // template. The explicit is needed to test-around a similar bug with
    // is_convertible.
    template <typename T>
    constexpr explicit NoValueCtor(T)
    { static_assert(never<T>::value, "This should not be instantiated"); }

    static int count;
    int id;
};

int NoValueCtor::count = 0;


struct NoValueCtorEmpty {
    NoValueCtorEmpty() {}
    NoValueCtorEmpty(NoValueCtorEmpty const &) {}

    template <typename T>
    constexpr explicit NoValueCtorEmpty(T)
    { static_assert(never<T>::value, "This should not be instantiated"); }
};

int main() {
    {
        hana::tuple<int> t(2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
    }
    {
        constexpr hana::tuple<int> t(2);
        static_assert(hana::at_c<0>(t) == 2, "");
    }
    {
        constexpr hana::tuple<int> t;
        static_assert(hana::at_c<0>(t) == 0, "");
    }
    {
        constexpr hana::tuple<int, char*> t(2, nullptr);
        static_assert(hana::at_c<0>(t) == 2, "");
        static_assert(hana::at_c<1>(t) == nullptr, "");
    }
    {
        hana::tuple<int, char*> t(2, nullptr);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
    }
    {
        hana::tuple<int, char*, std::string> t(2, nullptr, "text");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == "text");
    }
    {
        hana::tuple<int, NoValueCtor, int, int> t(1, NoValueCtor(), 2, 3);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 1);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t).id == 1);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<3>(t) == 3);
    }
    {
        hana::tuple<int, NoValueCtorEmpty, int, int> t(1, NoValueCtorEmpty(), 2, 3);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 1);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<3>(t) == 3);
    }
    {
        struct T { };
        struct U { };
        struct V { };

        constexpr T t{};
        constexpr U u{};
        constexpr V v{};

        constexpr hana::tuple<T> x1{t};             (void)x1;
        constexpr hana::tuple<T, U> x2{t, u};       (void)x2;
        constexpr hana::tuple<T, U, V> x3{t, u, v}; (void)x3;
    }
    {
        struct T { };
        struct U { };
        struct V { };

        // Check for SFINAE-friendliness
        static_assert(!std::is_constructible<
            hana::tuple<T, U>, T
        >{}, "");

        static_assert(!std::is_constructible<
            hana::tuple<T, U>, U, T
        >{}, "");

        static_assert(!std::is_constructible<
            hana::tuple<T, U>, T, U, V
        >{}, "");
    }

    // Make sure we can initialize elements with the brace-init syntax.
    {
        struct Member { };
        struct Element { Member member; };

        hana::tuple<Element, Element> xs{{Member()}, {Member()}};
        hana::tuple<Element, Element> ys = {{Member()}, {Member()}};
        (void)xs; (void)ys;
    }
}
