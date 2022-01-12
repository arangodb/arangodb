
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_CLASS_HPP)
#define TEST_HAS_CLASS_HPP

#include "test_structs.hpp"
#include <boost/tti/has_class.hpp>

BOOST_TTI_HAS_CLASS(AnIntType)
BOOST_TTI_TRAIT_HAS_CLASS(NameStruct,AStructType)
BOOST_TTI_HAS_CLASS(AnIntTypeReference)
BOOST_TTI_HAS_CLASS(BType)
BOOST_TTI_TRAIT_HAS_CLASS(TheInteger,AnIntegerType)
BOOST_TTI_HAS_CLASS(CType)
BOOST_TTI_HAS_CLASS(AnotherIntegerType)
BOOST_TTI_TRAIT_HAS_CLASS(SomethingElse,someOtherType)
BOOST_TTI_HAS_CLASS(NoOtherType)

BOOST_TTI_TRAIT_HAS_CLASS(EInB,BTypeEnum)
BOOST_TTI_HAS_CLASS(AnEnumTtype)
BOOST_TTI_TRAIT_HAS_CLASS(AnotherE,AnotherEnumTtype)
BOOST_TTI_TRAIT_HAS_CLASS(EClass,AnEnumClassType)
BOOST_TTI_HAS_CLASS(AnotherEnumClassType)
BOOST_TTI_HAS_CLASS(CTypeUnion)
BOOST_TTI_TRAIT_HAS_CLASS(SimpleUT,AnUnion)
BOOST_TTI_HAS_CLASS(AnotherUnion)

BOOST_TTI_TRAIT_HAS_CLASS(UnionType,UNStruct)

#endif // TEST_HAS_CLASS_HPP
