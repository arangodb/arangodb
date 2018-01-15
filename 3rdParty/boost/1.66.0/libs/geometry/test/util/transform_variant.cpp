// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/geometry/util/transform_variant.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/type_traits.hpp>
#include <boost/variant/variant.hpp>

using boost::mpl::placeholders::_;


template <typename ExpectedTypes, BOOST_VARIANT_ENUM_PARAMS(typename T)>
void check(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>)
{
    BOOST_MPL_ASSERT((
        boost::mpl::equal<
            typename boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>::types,
            ExpectedTypes
        >
    ));
}


int test_main(int, char* [])
{
    // Transform Variant to Variant
    typedef boost::geometry::transform_variant<
        boost::variant<int, float, long>,
        boost::add_pointer<_>
    >::type transformed1;

    check<boost::mpl::vector<int*, float*, long*> >(transformed1());

    // Transform Sequence to Variant (without inserter)
    typedef boost::geometry::transform_variant<
        boost::mpl::vector<int, float, long>,
        boost::add_pointer<_>
    >::type transformed2;

    check<boost::mpl::vector<int*, float*, long*> >(transformed2());

    // Transform Sequence to Variant (with inserter)
    typedef boost::geometry::transform_variant<
        boost::mpl::vector<int, float, long>,
        boost::add_pointer<_>,
        boost::mpl::back_inserter<boost::mpl::vector0<> >
    >::type transformed3;

    check<boost::mpl::vector<int*, float*, long*> >(transformed3());

    return 0;
}
