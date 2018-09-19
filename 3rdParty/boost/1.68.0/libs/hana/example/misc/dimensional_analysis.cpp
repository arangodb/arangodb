// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/zip_with.hpp>

#include <functional>
namespace hana = boost::hana;


//
// Example of implementing basic dimensional analysis using Hana
//


// base dimensions                              M  L  T  I  K  J  N
using mass        = decltype(hana::tuple_c<int, 1, 0, 0, 0, 0, 0, 0>);
using length      = decltype(hana::tuple_c<int, 0, 1, 0, 0, 0, 0, 0>);
using time_       = decltype(hana::tuple_c<int, 0, 0, 1, 0, 0, 0, 0>);
using charge      = decltype(hana::tuple_c<int, 0, 0, 0, 1, 0, 0, 0>);
using temperature = decltype(hana::tuple_c<int, 0, 0, 0, 0, 1, 0, 0>);
using intensity   = decltype(hana::tuple_c<int, 0, 0, 0, 0, 0, 1, 0>);
using amount      = decltype(hana::tuple_c<int, 0, 0, 0, 0, 0, 0, 1>);

// composite dimensions
using velocity     = decltype(hana::tuple_c<int, 0, 1, -1, 0, 0, 0, 0>); // M/T
using acceleration = decltype(hana::tuple_c<int, 0, 1, -2, 0, 0, 0, 0>); // M/T^2
using force        = decltype(hana::tuple_c<int, 1, 1, -2, 0, 0, 0, 0>); // ML/T^2


template <typename Dimensions>
struct quantity {
    double value_;

    explicit quantity(double v) : value_(v) { }

    template <typename OtherDimensions>
    explicit quantity(quantity<OtherDimensions> other)
      : value_(other.value_)
    {
      static_assert(Dimensions{} == OtherDimensions{},
        "Constructing quantities with incompatible dimensions!");
    }

    explicit operator double() const { return value_; }
};

template <typename D1, typename D2>
auto operator*(quantity<D1> a, quantity<D2> b) {
    using D = decltype(hana::zip_with(std::plus<>{}, D1{}, D2{}));
    return quantity<D>{static_cast<double>(a) * static_cast<double>(b)};
}

template <typename D1, typename D2>
auto operator/(quantity<D1> a, quantity<D2> b) {
    using D = decltype(hana::zip_with(std::minus<>{}, D1{}, D2{}));
    return quantity<D>{static_cast<double>(a) / static_cast<double>(b)};
}

int main() {
    quantity<mass>         m{10.3};
    quantity<length>       d{3.6};
    quantity<time_>        t{2.4};
    quantity<velocity>     v{d / t};
    quantity<acceleration> a{3.9};
    quantity<force>        f{m * a};
}
