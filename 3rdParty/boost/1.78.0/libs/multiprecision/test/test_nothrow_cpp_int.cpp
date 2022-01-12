///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>
#include <type_traits>

//
// Move construct:
//
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Move assign:
//
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Construct:
//
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Copy construct:
//
static_assert(!std::is_nothrow_copy_constructible<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Assign:
//
static_assert(!std::is_nothrow_copy_assignable<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Construct from int:
//
static_assert(noexcept(boost::multiprecision::cpp_int(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int128_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int128_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint128_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(!noexcept(boost::multiprecision::checked_uint128_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int512_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int512_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint512_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
static_assert(!noexcept(boost::multiprecision::checked_uint512_t(std::declval<boost::multiprecision::signed_limb_type>())), "noexcept test");
//
// Construct from unsigned int:
//
static_assert(noexcept(boost::multiprecision::cpp_int(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int128_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int128_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint128_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_uint128_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int512_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int512_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint512_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_uint512_t(std::declval<boost::multiprecision::limb_type>())), "noexcept test");
//
// Assign from int:
//
static_assert(noexcept(std::declval<boost::multiprecision::cpp_int>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int128_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int128_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint128_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(!noexcept(std::declval<boost::multiprecision::checked_uint128_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int512_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int512_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint512_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
static_assert(!noexcept(std::declval<boost::multiprecision::checked_uint512_t>() = std::declval<boost::multiprecision::signed_limb_type>()), "noexcept test");
//
// Assign from unsigned int:
//
static_assert(noexcept(std::declval<boost::multiprecision::cpp_int>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int128_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int128_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint128_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_uint128_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int512_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int512_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint512_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_uint512_t>() = std::declval<boost::multiprecision::limb_type>()), "noexcept test");

#if defined(BOOST_LITTLE_ENDIAN) && !defined(BOOST_MP_TEST_NO_LE)
//
// We can also nothrow construct from a double_limb_type (or smaller obviously) as long as double_limb_type is smaller than the type
// in question (so don't test 128-bit integers in case double_limb_type is __int128).
//
// Construct from int:
//
static_assert(noexcept(boost::multiprecision::cpp_int(std::declval<boost::multiprecision::signed_double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int512_t(std::declval<boost::multiprecision::signed_double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int512_t(std::declval<boost::multiprecision::signed_double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint512_t(std::declval<boost::multiprecision::signed_double_limb_type>())), "noexcept test");
static_assert(!noexcept(boost::multiprecision::checked_uint512_t(std::declval<boost::multiprecision::signed_double_limb_type>())), "noexcept test");
//
// Construct from unsigned int:
//
static_assert(noexcept(boost::multiprecision::cpp_int(std::declval<boost::multiprecision::double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::int512_t(std::declval<boost::multiprecision::double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_int512_t(std::declval<boost::multiprecision::double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::uint512_t(std::declval<boost::multiprecision::double_limb_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::checked_uint512_t(std::declval<boost::multiprecision::double_limb_type>())), "noexcept test");
//
// Assign from int:
//
static_assert(noexcept(std::declval<boost::multiprecision::cpp_int>() = std::declval<boost::multiprecision::signed_double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()), "noexcept test");
static_assert(!noexcept(std::declval<boost::multiprecision::checked_uint512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()), "noexcept test");
//
// Assign from unsigned int:
//
static_assert(noexcept(std::declval<boost::multiprecision::cpp_int>() = std::declval<boost::multiprecision::double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::int512_t>() = std::declval<boost::multiprecision::double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_int512_t>() = std::declval<boost::multiprecision::double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::uint512_t>() = std::declval<boost::multiprecision::double_limb_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::checked_uint512_t>() = std::declval<boost::multiprecision::double_limb_type>()), "noexcept test");

#endif // little endian

typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<32, 32, boost::multiprecision::signed_magnitude, boost::multiprecision::checked, void> >     checked_int32_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<32, 32, boost::multiprecision::unsigned_magnitude, boost::multiprecision::checked, void> >   checked_uint32_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<32, 32, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void> >   unchecked_int32_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<32, 32, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void> > unchecked_uint32_t;

//
// Construct from int:
//
static_assert(noexcept(unchecked_int32_t(std::declval<std::int32_t>())), "noexcept test");
static_assert(noexcept(checked_int32_t(std::declval<std::int32_t>())), "noexcept test");
static_assert(noexcept(unchecked_uint32_t(std::declval<std::int32_t>())), "noexcept test");
static_assert(!noexcept(checked_uint32_t(std::declval<std::int32_t>())), "noexcept test");
//
// Construct from unsigned int:
//
static_assert(noexcept(unchecked_int32_t(std::declval<std::uint32_t>())), "noexcept test");
static_assert(noexcept(checked_int32_t(std::declval<std::uint32_t>())), "noexcept test");
static_assert(noexcept(unchecked_uint32_t(std::declval<std::uint32_t>())), "noexcept test");
static_assert(noexcept(checked_uint32_t(std::declval<std::uint32_t>())), "noexcept test");
//
// Assign from int:
//
static_assert(noexcept(std::declval<unchecked_int32_t>() = std::declval<std::int32_t>()), "noexcept test");
static_assert(noexcept(std::declval<checked_int32_t>() = std::declval<std::int32_t>()), "noexcept test");
static_assert(noexcept(std::declval<unchecked_uint32_t>() = std::declval<std::int32_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_uint32_t>() = std::declval<std::int32_t>()), "noexcept test");
//
// Assign from unsigned int:
//
static_assert(noexcept(std::declval<unchecked_int32_t>() = std::declval<std::uint32_t>()), "noexcept test");
static_assert(noexcept(std::declval<checked_int32_t>() = std::declval<std::uint32_t>()), "noexcept test");
static_assert(noexcept(std::declval<unchecked_uint32_t>() = std::declval<std::uint32_t>()), "noexcept test");
static_assert(noexcept(std::declval<checked_uint32_t>() = std::declval<std::uint32_t>()), "noexcept test");

//
// And finally some things which should *not* be noexcept:
//
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<30, 30, boost::multiprecision::signed_magnitude, boost::multiprecision::checked, void> >     checked_int30_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<30, 30, boost::multiprecision::unsigned_magnitude, boost::multiprecision::checked, void> >   checked_uint30_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<30, 30, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void> >   unchecked_int30_t;
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<30, 30, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void> > unchecked_uint30_t;

//
// Construct from int:
//
static_assert(!noexcept(checked_int30_t(std::declval<std::int32_t>())), "noexcept test");
static_assert(!noexcept(checked_uint30_t(std::declval<std::int32_t>())), "noexcept test");
static_assert(!noexcept(checked_int32_t(std::declval<std::int64_t>())), "noexcept test");
static_assert(!noexcept(checked_uint32_t(std::declval<std::int64_t>())), "noexcept test");
//
// Construct from unsigned int:
//
static_assert(!noexcept(checked_int30_t(std::declval<std::uint32_t>())), "noexcept test");
static_assert(!noexcept(checked_uint30_t(std::declval<std::uint32_t>())), "noexcept test");
static_assert(!noexcept(checked_int32_t(std::declval<std::uint64_t>())), "noexcept test");
static_assert(!noexcept(checked_uint32_t(std::declval<std::uint64_t>())), "noexcept test");
//
// Assign from int:
//
static_assert(!noexcept(std::declval<checked_int30_t>() = std::declval<std::int32_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_uint30_t>() = std::declval<std::int32_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_int32_t>() = std::declval<std::int64_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_uint32_t>() = std::declval<std::int64_t>()), "noexcept test");
//
// Assign from unsigned int:
//
static_assert(!noexcept(std::declval<checked_int30_t>() = std::declval<std::uint32_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_uint30_t>() = std::declval<std::uint32_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_int32_t>() = std::declval<std::uint64_t>()), "noexcept test");
static_assert(!noexcept(std::declval<checked_uint32_t>() = std::declval<std::uint64_t>()), "noexcept test");

//
// Check regular construct/assign as well, see https://github.com/boostorg/multiprecision/issues/383
//
//
// Move assign:
//
static_assert(std::is_move_assignable<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_move_assignable<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Construct:
//
static_assert(std::is_default_constructible<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_default_constructible<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Copy construct:
//
static_assert(std::is_copy_constructible<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_copy_constructible<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
//
// Assign:
//
static_assert(std::is_copy_assignable<boost::multiprecision::cpp_int>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::int128_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::checked_int128_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::uint128_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::checked_uint128_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::int512_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::checked_int512_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::uint512_t>::value, "noexcept test");
static_assert(std::is_copy_assignable<boost::multiprecision::checked_uint512_t>::value, "noexcept test");
