// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_STATIC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_STATIC_HPP

#if defined(_MSC_VER)
// For CRTP, *this is acceptable in constructor -> turn warning off
#pragma warning( disable : 4355 )
#endif // defined(_MSC_VER)


#include <string>

#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/srs/projections/impl/pj_fwd.hpp>
#include <boost/geometry/srs/projections/impl/pj_inv.hpp>

#include <boost/mpl/assert.hpp>


namespace boost { namespace geometry { namespace projections
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template <typename Prj, typename CSTag, typename BGP, typename CT, typename P>
struct static_projection_type
{
    BOOST_MPL_ASSERT_MSG((false),
        NOT_IMPLEMENTED_FOR_THIS_PROJECTION_OR_CSTAG,
        (Prj, CSTag));
};

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(PROJ, P_SPHERE, P_SPHEROID) \
template <typename BGP, typename CT, typename P> \
struct static_projection_type<PROJ, srs_sphere_tag, BGP, CT, P> \
{ \
    typedef P_SPHERE<CT, P> type; \
}; \
template <typename BGP, typename CT, typename P> \
struct static_projection_type<PROJ, srs_spheroid_tag, BGP, CT, P> \
{ \
    typedef P_SPHEROID<CT, P> type; \
}; \

// Base-template-forward
template <typename Prj, typename CT, typename P>
struct base_t_f
{
public:

    inline base_t_f(Prj const& prj, P const& params)
        : m_par(params), m_prj(prj)
    {}

    inline P const& params() const { return m_par; }

    inline P& mutable_params() { return m_par; }

    template <typename LL, typename XY>
    inline bool forward(LL const& lp, XY& xy) const
    {
        try
        {
            pj_fwd(m_prj, m_par, lp, xy);
            return true;
        }
        catch(...)
        {
            return false;
        }
    }

    template <typename XY, typename LL>
    inline bool inverse(XY const& , LL& ) const
    {
        BOOST_MPL_ASSERT_MSG((false),
                             PROJECTION_IS_NOT_INVERTABLE,
                             (Prj));
        return false;
    }

    inline std::string name() const
    {
        return this->m_par.name;
    }

protected:

    P m_par;
    const Prj& m_prj;
};

// Base-template-forward/inverse
template <typename Prj, typename CT, typename P>
struct base_t_fi : public base_t_f<Prj, CT, P>
{
public :
    inline base_t_fi(Prj const& prj, P const& params)
        : base_t_f<Prj, CT, P>(prj, params)
    {}

    template <typename XY, typename LL>
    inline bool inverse(XY const& xy, LL& lp) const
    {
        try
        {
            pj_inv(this->m_prj, this->m_par, xy, lp);
            return true;
        }
        catch(...)
        {
            return false;
        }
    }
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


}}} // namespace boost::geometry::projections


#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_BASE_STATIC_HPP
