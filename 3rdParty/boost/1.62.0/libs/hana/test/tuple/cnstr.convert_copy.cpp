// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct A {
    int id_;
    constexpr A(int i) : id_(i) {}
    friend constexpr bool operator==(const A& x, const A& y)
    { return x.id_ == y.id_; }
};

struct B {
    int id_;
    explicit B(int i) : id_(i) {}
};

struct C {
    int id_;
    constexpr explicit C(int i) : id_(i) {}
    friend constexpr bool operator==(const C& x, const C& y)
    { return x.id_ == y.id_; }
};

struct D : B {
    explicit D(int i) : B(i) {}
};


int main() {
    {
        using T0 = hana::tuple<double>;
        using T1 = hana::tuple<int>;
        T0 t0(2.5);
        T1 t1 = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
    }
    {
        using T0 = hana::tuple<double>;
        using T1 = hana::tuple<A>;
        constexpr T0 t0(2.5);
        constexpr T1 t1 = t0;
        static_assert(hana::at_c<0>(t1) == 2, "");
    }
    {
        using T0 = hana::tuple<int>;
        using T1 = hana::tuple<C>;
        constexpr T0 t0(2);
        constexpr T1 t1{t0};
        static_assert(hana::at_c<0>(t1) == C(2), "");
    }
    {
        using T0 = hana::tuple<double, char>;
        using T1 = hana::tuple<int, int>;
        T0 t0(2.5, 'a');
        T1 t1 = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
    }
    {
        using T0 = hana::tuple<double, char, D>;
        using T1 = hana::tuple<int, int, B>;
        T0 t0(2.5, 'a', D(3));
        T1 t1 = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1).id_ == 3);
    }
    {
        D d(3);
        using T0 = hana::tuple<double, char, D&>;
        using T1 = hana::tuple<int, int, B&>;
        T0 t0(2.5, 'a', d);
        T1 t1 = t0;
        d.id_ = 2;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1).id_ == 2);
    }
    {
        using T0 = hana::tuple<double, char, int>;
        using T1 = hana::tuple<int, int, B>;
        T0 t0(2.5, 'a', 3);
        T1 t1(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1).id_ == 3);
    }
}
