// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/traits.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


enum Enumeration { };
struct Structure { };
constexpr auto e = hana::type_c<Enumeration>;
constexpr auto s = hana::type_c<Structure>;

int main() {
    // We just make sure that they compile. If the forwarding to `std::` is
    // well done, it is the job of `std::` to return the right thing.

    ///////////////////////
    // Type properties
    ///////////////////////
    // Primary type categories
    static_assert(!hana::traits::is_void(s), "the traits should be compile-time checkable");
    hana::traits::is_null_pointer(s);
    hana::traits::is_integral(s);
    hana::traits::is_floating_point(s);
    hana::traits::is_array(s);
    hana::traits::is_enum(s);
    hana::traits::is_union(s);
    hana::traits::is_class(s);
    hana::traits::is_function(s);
    hana::traits::is_pointer(s);
    hana::traits::is_lvalue_reference(s);
    hana::traits::is_rvalue_reference(s);
    hana::traits::is_member_object_pointer(s);
    hana::traits::is_member_function_pointer(s);

    // Composite type categories
    hana::traits::is_fundamental(s);
    hana::traits::is_arithmetic(s);
    hana::traits::is_scalar(s);
    hana::traits::is_object(s);
    hana::traits::is_compound(s);
    hana::traits::is_reference(s);
    hana::traits::is_member_pointer(s);

    // Type properties
    hana::traits::is_const(s);
    hana::traits::is_volatile(s);
    hana::traits::is_trivial(s);
    hana::traits::is_trivially_copyable(s);
    hana::traits::is_standard_layout(s);
    hana::traits::is_pod(s);
    hana::traits::is_literal_type(s);
    hana::traits::is_empty(s);
    hana::traits::is_polymorphic(s);
    hana::traits::is_abstract(s);
    hana::traits::is_signed(s);
    hana::traits::is_unsigned(s);

    // Supported operations
    hana::traits::is_constructible(s, s);
    hana::traits::is_trivially_constructible(s, s);
    hana::traits::is_nothrow_constructible(s, s);

    hana::traits::is_default_constructible(s);
    hana::traits::is_trivially_default_constructible(s);
    hana::traits::is_nothrow_default_constructible(s);

    hana::traits::is_copy_constructible(s);
    hana::traits::is_trivially_copy_constructible(s);
    hana::traits::is_nothrow_copy_constructible(s);

    hana::traits::is_move_constructible(s);
    hana::traits::is_trivially_move_constructible(s);
    hana::traits::is_nothrow_move_constructible(s);

    hana::traits::is_assignable(s, s);
    hana::traits::is_trivially_assignable(s, s);
    hana::traits::is_nothrow_assignable(s, s);

    hana::traits::is_copy_assignable(s);
    hana::traits::is_trivially_copy_assignable(s);
    hana::traits::is_nothrow_copy_assignable(s);

    hana::traits::is_move_assignable(s);
    hana::traits::is_trivially_move_assignable(s);
    hana::traits::is_nothrow_move_assignable(s);

    hana::traits::is_destructible(s);
    hana::traits::is_trivially_destructible(s);
    hana::traits::is_nothrow_destructible(s);

    hana::traits::has_virtual_destructor(s);

    // Property queries
    hana::traits::alignment_of(s);
    hana::traits::rank(s);
    hana::traits::extent(s);
    hana::traits::extent(hana::type_c<int[2][3]>, hana::uint_c<1>);

    // Type relationships
    hana::traits::is_same(s, s);
    hana::traits::is_base_of(s, s);
    hana::traits::is_convertible(s, s);

    ///////////////////////
    // Type modifications
    ///////////////////////
    // Const-volatility specifiers
    hana::traits::remove_cv(s);
    hana::traits::remove_const(s);
    hana::traits::remove_volatile(s);

    hana::traits::add_cv(s);
    hana::traits::add_const(s);
    hana::traits::add_volatile(s);

    // References
    hana::traits::remove_reference(s);
    hana::traits::add_lvalue_reference(s);
    hana::traits::add_rvalue_reference(s);

    // Pointers
    hana::traits::remove_pointer(s);
    hana::traits::add_pointer(s);

    // Sign modifiers
    hana::traits::make_signed(hana::type_c<unsigned>);
    hana::traits::make_unsigned(hana::type_c<signed>);

    // Arrays
    hana::traits::remove_extent(s);
    hana::traits::remove_all_extents(s);

    // Miscellaneous transformations
    hana::traits::aligned_storage(hana::size_c<1>);
    hana::traits::aligned_storage(hana::size_c<1>, hana::size_c<1>);
    hana::traits::aligned_union(hana::size_c<0>, s);
    hana::traits::decay(s);

    hana::traits::common_type(s, s);
    hana::traits::underlying_type(e);
    using FunctionPointer = void(*)();
    hana::traits::result_of(hana::type_c<FunctionPointer(void)>);

    ///////////////////////
    // Utilities
    ///////////////////////
    using Z = decltype(hana::traits::declval(hana::type_c<Structure>));
}
