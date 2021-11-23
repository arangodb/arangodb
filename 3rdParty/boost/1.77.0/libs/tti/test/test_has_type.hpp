
//  (C) Copyright Edward Diener 2011
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_TYPE_HPP)
#define TEST_HAS_TYPE_HPP

#include "test_structs.hpp"
#include <boost/tti/has_type.hpp>

BOOST_TTI_HAS_TYPE(AnIntType)
BOOST_TTI_TRAIT_HAS_TYPE(NameStruct,AStructType)
BOOST_TTI_HAS_TYPE(AnIntTypeReference)
BOOST_TTI_HAS_TYPE(BType)
BOOST_TTI_TRAIT_HAS_TYPE(TheInteger,AnIntegerType)
BOOST_TTI_HAS_TYPE(CType)
BOOST_TTI_HAS_TYPE(AnotherIntegerType)
BOOST_TTI_TRAIT_HAS_TYPE(SomethingElse,someOtherType)
BOOST_TTI_HAS_TYPE(NoOtherType)

BOOST_TTI_TRAIT_HAS_TYPE(EInB,BTypeEnum)
BOOST_TTI_HAS_TYPE(AnEnumTtype)
BOOST_TTI_TRAIT_HAS_TYPE(AnotherE,AnotherEnumTtype)
BOOST_TTI_TRAIT_HAS_TYPE(EClass,AnEnumClassType)
BOOST_TTI_HAS_TYPE(AnotherEnumClassType)
BOOST_TTI_HAS_TYPE(CTypeUnion)
BOOST_TTI_TRAIT_HAS_TYPE(SimpleUT,AnUnion)
BOOST_TTI_HAS_TYPE(AnotherUnion)

BOOST_TTI_TRAIT_HAS_TYPE(UnionType,UNStruct)
BOOST_TTI_HAS_TYPE(UEnumV)
BOOST_TTI_HAS_TYPE(InnerUnion)

#endif // TEST_HAS_TYPE_HPP
