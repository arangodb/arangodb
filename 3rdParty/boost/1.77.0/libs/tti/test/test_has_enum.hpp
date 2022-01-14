
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_ENUM_HPP)
#define TEST_HAS_ENUM_HPP

#include "test_structs.hpp"
#include <boost/tti/has_enum.hpp>

BOOST_TTI_HAS_ENUM(AnIntType)
BOOST_TTI_TRAIT_HAS_ENUM(NameStruct,AStructType)
BOOST_TTI_HAS_ENUM(AnIntTypeReference)
BOOST_TTI_HAS_ENUM(BType)
BOOST_TTI_TRAIT_HAS_ENUM(TheInteger,AnIntegerType)
BOOST_TTI_HAS_ENUM(CType)
BOOST_TTI_HAS_ENUM(AnotherIntegerType)
BOOST_TTI_TRAIT_HAS_ENUM(SomethingElse,someOtherType)
BOOST_TTI_HAS_ENUM(NoOtherType)

BOOST_TTI_TRAIT_HAS_ENUM(EInB,BTypeEnum)
BOOST_TTI_HAS_ENUM(AnEnumTtype)
BOOST_TTI_TRAIT_HAS_ENUM(AnotherE,AnotherEnumTtype)
BOOST_TTI_TRAIT_HAS_ENUM(EClass,AnEnumClassType)
BOOST_TTI_HAS_ENUM(AnotherEnumClassType)

BOOST_TTI_HAS_ENUM(CTypeUnion)
BOOST_TTI_TRAIT_HAS_ENUM(SimpleUT,AnUnion)
BOOST_TTI_HAS_ENUM(AnotherUnion)

BOOST_TTI_HAS_ENUM(UEnumV)

#endif // TEST_HAS_ENUM_HPP
