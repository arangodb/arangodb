// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/optional.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// This test makes sure that an optional holding a hana::type has a
// nested ::type alias.


template <typename ...>
using void_t = void;

template <typename T, typename = void>
struct has_type : std::false_type { };

template <typename T>
struct has_type<T, void_t<typename T::type>>
    : std::true_type
{ };


struct T;

static_assert(std::is_same<decltype(hana::just(hana::type_c<T>))::type, T>{}, "");
static_assert(!has_type<decltype(hana::nothing)>{}, "");

int main() { }
