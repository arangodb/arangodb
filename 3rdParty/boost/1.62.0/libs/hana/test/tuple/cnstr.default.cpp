// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
#include <type_traits>
namespace hana = boost::hana;


struct DefaultOnly {
    int data_;
    DefaultOnly(DefaultOnly const&) = delete;
    DefaultOnly& operator=(DefaultOnly const&) = delete;

    static int count;

    DefaultOnly() : data_(-1) { ++count; }
    ~DefaultOnly() { data_ = 0; --count; }

    friend bool operator==(DefaultOnly const& x, DefaultOnly const& y)
    { return x.data_ == y.data_; }

    friend bool operator< (DefaultOnly const& x, DefaultOnly const& y)
    { return x.data_ < y.data_; }
};

int DefaultOnly::count = 0;

struct NoDefault {
    NoDefault() = delete;
    explicit NoDefault(int) { }
};

struct IllFormedDefault {
    IllFormedDefault(int x) : value(x) {}
    template <bool Pred = false>
    constexpr IllFormedDefault() {
        static_assert(Pred,
            "The default constructor should not be instantiated");
    }
    int value;
};

int main() {
    {
        hana::tuple<> t; (void)t;
    }
    {
        hana::tuple<int> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
    }
    {
        hana::tuple<int, char*> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
    }
    {
        hana::tuple<int, char*, std::string> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == "");
    }
    {
        hana::tuple<int, char*, std::string, DefaultOnly> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == "");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<3>(t) == DefaultOnly());
    }
    {
        // See LLVM bug #21157.
        static_assert(!std::is_default_constructible<
            hana::tuple<NoDefault>
        >(), "");
        static_assert(!std::is_default_constructible<
            hana::tuple<DefaultOnly, NoDefault>
        >(), "");
        static_assert(!std::is_default_constructible<
            hana::tuple<NoDefault, DefaultOnly, NoDefault>
        >(), "");
    }
    {
        struct T { };
        struct U { };
        struct V { };

        constexpr hana::tuple<> z0;        (void)z0;
        constexpr hana::tuple<T> z1;       (void)z1;
        constexpr hana::tuple<T, U> z2;    (void)z2;
        constexpr hana::tuple<T, U, V> z3; (void)z3;
    }
    {
        constexpr hana::tuple<int> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
    }
    {
        constexpr hana::tuple<int, char*> t;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == nullptr);
    }

    // Make sure we can hold non default-constructible elements, and that
    // it does not trigger an error in the default constructor.
    {
        {
            IllFormedDefault v(0);
            hana::tuple<IllFormedDefault> t1(v);
            hana::tuple<IllFormedDefault> t2{v};
            hana::tuple<IllFormedDefault> t3 = {v};
            (void)t1;(void)t2;(void)t3; // remove spurious unused variable warning on GCC
        }
        {
            hana::tuple<NoDefault> t1(0);
            hana::tuple<NoDefault> t2{0};
            hana::tuple<NoDefault> t3 = {0};
            (void)t1;(void)t2;(void)t3; // remove spurious unused variable warning on GCC
        }
        {
            NoDefault v(0);
            hana::tuple<NoDefault> t1(v);
            hana::tuple<NoDefault> t2{v};
            hana::tuple<NoDefault> t3 = {v};
            (void)t1;(void)t2;(void)t3; // remove spurious unused variable warning on GCC
        }
    }

    // Make sure a tuple_t can be default-constructed
    {
        struct T;
        struct U;

        using Types = decltype(hana::tuple_t<T, U>);
        Types t{}; (void)t;
    }
}
