// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/core/when.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename T, typename ExpectedDatatype>
struct test {
    static_assert(std::is_same<hana::tag_of_t<T>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T volatile>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const volatile>, ExpectedDatatype>::value, "");

    static_assert(std::is_same<hana::tag_of_t<T&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T volatile&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const volatile&>, ExpectedDatatype>::value, "");

    static_assert(std::is_same<hana::tag_of_t<T&&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const&&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T volatile&&>, ExpectedDatatype>::value, "");
    static_assert(std::is_same<hana::tag_of_t<T const volatile&&>, ExpectedDatatype>::value, "");
};

struct NestedDatatype;
struct Nested { struct hana_tag; };
template struct test<Nested, Nested::hana_tag>;

struct NoNestedDatatype { };
template struct test<NoNestedDatatype, NoNestedDatatype>;

struct NoNestedHana { };
template struct test<NoNestedHana, NoNestedHana>;


struct FullySpecializedDatatype;
struct FullySpecialized;
namespace boost { namespace hana {
    template <>
    struct tag_of<FullySpecialized> {
        using type = FullySpecializedDatatype;
    };
}}
template struct test<FullySpecialized, FullySpecializedDatatype>;


struct PartiallySpecializedDatatype;
template <typename> struct PartiallySpecialized;
namespace boost { namespace hana {
    template <typename T>
    struct tag_of<PartiallySpecialized<T>> {
        using type = PartiallySpecializedDatatype;
    };
}}
template struct test<PartiallySpecialized<struct anything>, PartiallySpecializedDatatype>;


struct PredicatedDatatype;
struct Predicated { static constexpr bool predicate = true; };
namespace boost { namespace hana {
    template <typename T>
    struct tag_of<T, hana::when<T::predicate>> {
        using type = PredicatedDatatype;
    };
}}
template struct test<Predicated, PredicatedDatatype>;


int main() { }
