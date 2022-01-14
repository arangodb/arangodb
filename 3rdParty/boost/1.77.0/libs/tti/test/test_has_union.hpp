
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_UNION_HPP)
#define TEST_HAS_UNION_HPP

#include "test_structs.hpp"
#include <boost/tti/has_union.hpp>

BOOST_TTI_HAS_UNION(AnIntType)
BOOST_TTI_TRAIT_HAS_UNION(NameStruct,AStructType)
BOOST_TTI_HAS_UNION(AnIntTypeReference)
BOOST_TTI_HAS_UNION(BType)
BOOST_TTI_TRAIT_HAS_UNION(TheInteger,AnIntegerType)
BOOST_TTI_HAS_UNION(CType)
BOOST_TTI_HAS_UNION(AnotherIntegerType)
BOOST_TTI_TRAIT_HAS_UNION(SomethingElse,someOtherType)
BOOST_TTI_HAS_UNION(NoOtherType)

BOOST_TTI_HAS_UNION(CTypeUnion)
BOOST_TTI_TRAIT_HAS_UNION(SimpleUT,AnUnion)
BOOST_TTI_HAS_UNION(AnotherUnion)

BOOST_TTI_TRAIT_HAS_UNION(EInB,BTypeEnum)
BOOST_TTI_HAS_UNION(AnEnumTtype)
BOOST_TTI_TRAIT_HAS_UNION(AnotherE,AnotherEnumTtype)
BOOST_TTI_TRAIT_HAS_UNION(EClass,AnEnumClassType)
BOOST_TTI_HAS_UNION(AnotherEnumClassType)

BOOST_TTI_HAS_UNION(InnerUnion)

#endif // TEST_HAS_UNION_HPP
