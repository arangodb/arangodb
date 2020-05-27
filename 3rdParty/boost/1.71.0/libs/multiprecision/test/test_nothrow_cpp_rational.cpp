///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include <boost/type_traits/is_nothrow_move_assignable.hpp>
#include <boost/type_traits/has_nothrow_constructor.hpp>
#include <boost/type_traits/has_nothrow_assign.hpp>
#include <boost/type_traits/has_nothrow_copy.hpp>
#include <boost/static_assert.hpp>

typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::int128_t::backend_type> > rat128_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::uint128_t::backend_type> > urat128_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::int512_t::backend_type> > rat512_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::uint512_t::backend_type> > urat512_t;

typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_int128_t::backend_type> > checked_rat128_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint128_t::backend_type> > checked_urat128_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_int512_t::backend_type> > checked_rat512_t;
typedef boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint512_t::backend_type> > checked_urat512_t;

#ifndef BOOST_NO_CXX11_NOEXCEPT

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_IS_NOTHROW_MOVE_CONSTRUCT)
//
// Move construct:
//
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<boost::multiprecision::cpp_rational>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<rat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<checked_rat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<urat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<checked_urat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<rat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<checked_rat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<urat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<checked_urat512_t>::value);

#endif

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_IS_NOTHROW_MOVE_ASSIGN)
//
// Move assign:
//
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<boost::multiprecision::cpp_rational>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<rat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<checked_rat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<urat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<checked_urat128_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<rat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<checked_rat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<urat512_t>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<checked_urat512_t>::value);

#endif

#if 0
//
// Everything below could/should be made to work, given modifications to Boost.Rational
//


//
// Construct:
//
#ifdef BOOST_HAS_NOTHROW_CONSTRUCTOR
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<boost::multiprecision::cpp_rational>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<checked_rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<checked_urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<checked_rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<urat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<checked_urat512_t>::value);
#endif
//
// Copy construct:
//
#ifdef BOOST_HAS_NOTHROW_COPY
BOOST_STATIC_ASSERT(!boost::has_nothrow_copy<boost::multiprecision::cpp_rational>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<checked_rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<checked_urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<checked_rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<urat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<checked_urat512_t>::value);
#endif
//
// Assign:
//
#ifdef BOOST_HAS_NOTHROW_ASSIGN
BOOST_STATIC_ASSERT(!boost::has_nothrow_assign<boost::multiprecision::cpp_rational>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<checked_rat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<checked_urat128_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<checked_rat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<urat512_t>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<checked_urat512_t>::value);
#endif
//
// Construct from int:
//
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::cpp_rational(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat128_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat128_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat128_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(!noexcept(checked_urat128_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat512_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat512_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat512_t(std::declval<boost::multiprecision::signed_limb_type>())));
BOOST_STATIC_ASSERT(!noexcept(checked_urat512_t(std::declval<boost::multiprecision::signed_limb_type>())));
//
// Construct from unsigned int:
//
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::cpp_rational(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat128_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat128_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat128_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_urat128_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat512_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat512_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat512_t(std::declval<boost::multiprecision::limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_urat512_t(std::declval<boost::multiprecision::limb_type>())));
//
// Assign from int:
//
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::cpp_rational>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat128_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat128_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat128_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(!noexcept(std::declval<checked_urat128_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat512_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat512_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat512_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
BOOST_STATIC_ASSERT(!noexcept(std::declval<checked_urat512_t>() = std::declval<boost::multiprecision::signed_limb_type>()));
//
// Assign from unsigned int:
//
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::cpp_rational>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat128_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat128_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat128_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_urat128_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat512_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat512_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat512_t>() = std::declval<boost::multiprecision::limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_urat512_t>() = std::declval<boost::multiprecision::limb_type>()));

#if defined(BOOST_LITTLE_ENDIAN)
//
// Construct from int:
//
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::cpp_rational(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat128_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat128_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat128_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(!noexcept(checked_urat128_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat512_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat512_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat512_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
BOOST_STATIC_ASSERT(!noexcept(checked_urat512_t(std::declval<boost::multiprecision::signed_double_limb_type>())));
//
// Construct from unsigned int:
//
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::cpp_rational(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat128_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat128_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat128_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_urat128_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(rat512_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_rat512_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(urat512_t(std::declval<boost::multiprecision::double_limb_type>())));
BOOST_STATIC_ASSERT(noexcept(checked_urat512_t(std::declval<boost::multiprecision::double_limb_type>())));
//
// Assign from int:
//
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::cpp_rational>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat128_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat128_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat128_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(!noexcept(std::declval<checked_urat128_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
BOOST_STATIC_ASSERT(!noexcept(std::declval<checked_urat512_t>() = std::declval<boost::multiprecision::signed_double_limb_type>()));
//
// Assign from unsigned int:
//
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::cpp_rational>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat128_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat128_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat128_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_urat128_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<rat512_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_rat512_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<urat512_t>() = std::declval<boost::multiprecision::double_limb_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<checked_urat512_t>() = std::declval<boost::multiprecision::double_limb_type>()));

#endif
#endif // little endian
#endif // noexcept

