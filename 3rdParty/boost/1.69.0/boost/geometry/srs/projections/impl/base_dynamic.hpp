// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_DYNAMIC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_DYNAMIC_HPP

#include <string>

#include <boost/geometry/srs/projections/exception.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>

namespace boost { namespace geometry { namespace projections
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

/*!
    \brief projection virtual base class
    \details class containing virtual methods
    \ingroup projection
    \tparam CT calculation type
    \tparam P parameters type
*/
template <typename CT, typename P>
class base_v
{
public :
    /// Forward projection, from Latitude-Longitude to Cartesian
    template <typename LL, typename XY>
    inline bool forward(LL const& lp, XY& xy) const
    {
        try
        {
            pj_fwd(*this, this->params(), lp, xy);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    /// Inverse projection, from Cartesian to Latitude-Longitude
    template <typename LL, typename XY>
    inline bool inverse(XY const& xy, LL& lp) const
    {
        try
        {
            pj_inv(*this, this->params(), xy, lp);
            return true;
        }
        catch (projection_not_invertible_exception &)
        {
            BOOST_RETHROW
        }
        catch (...)
        {
            return false;
        }
    }

    /// Forward projection using lon / lat and x / y separately
    virtual void fwd(CT const& lp_lon, CT const& lp_lat, CT& xy_x, CT& xy_y) const = 0;

    /// Inverse projection using x / y and lon / lat
    virtual void inv(CT const& xy_x, CT const& xy_y, CT& lp_lon, CT& lp_lat) const = 0;

    /// Returns name of projection
    virtual std::string name() const = 0;

    /// Returns parameters of projection
    virtual P const& params() const = 0;

    /// Returns mutable parameters of projection
    virtual P& mutable_params() = 0;

    virtual ~base_v() {}
};

// Base-virtual-forward
template <typename Prj, typename CT, typename P>
class base_v_f : public base_v<CT, P>
{
public:
    explicit base_v_f(P const& p)
        : m_proj(p)
    {}

    template <typename P1, typename P2>
    base_v_f(P1 const& p1, P2 const& p2)
        : m_proj(p1, p2)
    {}

    template <typename P1, typename P2, typename P3>
    base_v_f(P1 const& p1, P2 const& p2, P3 const& p3)
        : m_proj(p1, p2, p3)
    {}

    virtual void fwd(CT const& lp_lon, CT const& lp_lat, CT& xy_x, CT& xy_y) const
    {
        m_proj.fwd(lp_lon, lp_lat, xy_x, xy_y);
    }

    virtual void inv(CT const& , CT const& , CT& , CT& ) const
    {
        BOOST_THROW_EXCEPTION(projection_not_invertible_exception(params().id.name));
    }

    virtual std::string name() const { return m_proj.name(); }

    virtual P const& params() const { return m_proj.params(); }

    virtual P& mutable_params() { return m_proj.mutable_params(); }

protected:
    Prj m_proj;
};

// Base-virtual-forward/inverse
template <typename Prj, typename CT, typename P>
class base_v_fi : public base_v_f<Prj, CT, P>
{
    typedef base_v_f<Prj, CT, P> base_t;

public:
    explicit base_v_fi(P const& p)
        : base_t(p)
    {}

    template <typename P1, typename P2>
    base_v_fi(P1 const& p1, P2 const& p2)
        : base_t(p1, p2)
    {}

    template <typename P1, typename P2, typename P3>
    base_v_fi(P1 const& p1, P2 const& p2, P3 const& p3)
        : base_t(p1, p2, p3)
    {}

    virtual void inv(CT const& xy_x, CT const& xy_y, CT& lp_lon, CT& lp_lat) const
    {
        this->m_proj.inv(xy_x, xy_y, lp_lon, lp_lat);
    }
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_DYNAMIC_HPP
