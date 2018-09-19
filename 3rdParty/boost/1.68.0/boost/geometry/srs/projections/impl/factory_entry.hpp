// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_FACTORY_ENTRY_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_FACTORY_ENTRY_HPP

#include <string>

#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>

namespace boost { namespace geometry { namespace projections
{

namespace detail
{

// forward declaration needed by some projections
template <typename CT, typename Parameters>
class factory;

template <typename CT, typename P>
class factory_entry
{
public:

    virtual ~factory_entry() {}
    virtual base_v<CT, P>* create_new(P const& par) const = 0;
};

template <typename CT, typename P>
class base_factory
{
public:

    virtual ~base_factory() {}
    virtual void add_to_factory(std::string const& name, factory_entry<CT, P>* sub) = 0;
};

} // namespace detail
}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_FACTORY_ENTRY_HPP
