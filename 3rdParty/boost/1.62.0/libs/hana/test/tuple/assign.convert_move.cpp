// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <memory>
#include <utility>
namespace hana = boost::hana;


struct B {
    int id_;
    explicit B(int i = 0) : id_(i) {}
    virtual ~B() {}
};

struct D : B {
    explicit D(int i) : B(i) {}
};

int main() {
    {
        using T0 = hana::tuple<double>;
        using T1 = hana::tuple<int>;
        T0 t0(2.5);
        T1 t1;
        t1 = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
    }
    {
        using T0 = hana::tuple<double, char>;
        using T1 = hana::tuple<int, int>;
        T0 t0(2.5, 'a');
        T1 t1;
        t1 = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
    }
    {
        using T0 = hana::tuple<double, char, D>;
        using T1 = hana::tuple<int, int, B>;
        T0 t0(2.5, 'a', D(3));
        T1 t1;
        t1 = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1).id_ == 3);
    }
    {
        D d(3);
        D d2(2);
        using T0 = hana::tuple<double, char, D&>;
        using T1 = hana::tuple<int, int, B&>;
        T0 t0(2.5, 'a', d2);
        T1 t1(1.5, 'b', d);
        t1 = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1).id_ == 2);
    }
    {
        using T0 = hana::tuple<double, char, std::unique_ptr<D>>;
        using T1 = hana::tuple<int, int, std::unique_ptr<B>>;
        T0 t0(2.5, 'a', std::unique_ptr<D>(new D(3)));
        T1 t1;
        t1 = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t1) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t1) == int('a'));
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t1)->id_ == 3);
    }
}
