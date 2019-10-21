// Boost.Geometry

// Copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_FACTORY_KEY_HPP
#define BOOST_GEOMETRY_PROJECTIONS_FACTORY_KEY_HPP

#include <string>

#include <boost/geometry/srs/projections/dpar.hpp>
#include <boost/geometry/srs/projections/proj4.hpp>

namespace boost { namespace geometry { namespace projections
{

namespace detail
{

template <typename Params>
struct factory_key_util
{
    BOOST_MPL_ASSERT_MSG((false), INVALID_PARAMETERS_TYPE, (Params));
};

template <>
struct factory_key_util<srs::detail::proj4_parameters>
{
    typedef std::string type;
    template <typename ProjParams>
    static type const& get(ProjParams const& par)
    {
        return par.id.name;
    }
};

template <typename T>
struct factory_key_util<srs::dpar::parameters<T> >
{
    typedef srs::dpar::value_proj type;
    template <typename ProjParams>
    static type const& get(ProjParams const& par)
    {
        return par.id.id;
    }
};

struct factory_key
{
    factory_key(const char* name, srs::dpar::value_proj id)
        : m_name(name), m_id(id)
    {}

    operator const char*() const
    {
        return m_name;
    }

    operator std::string() const
    {
        return std::string(m_name);
    }

    operator srs::dpar::value_proj() const
    {
        return m_id;
    }

private:
    const char* m_name;
    srs::dpar::value_proj m_id;
};


} // namespace detail

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_FACTORY_KEY_HPP
