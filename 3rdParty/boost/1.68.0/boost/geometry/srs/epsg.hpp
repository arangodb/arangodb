// Boost.Geometry

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_SRS_EPSG_HPP
#define BOOST_GEOMETRY_SRS_EPSG_HPP


#include <boost/geometry/srs/projection.hpp>
#include <boost/geometry/srs/projections/epsg.hpp>
#include <boost/geometry/srs/projections/epsg_params.hpp>
#include <boost/geometry/srs/projections/epsg_traits.hpp>


namespace boost { namespace geometry
{
    
namespace projections
{

template <typename CT>
struct dynamic_parameters<srs::epsg, CT>
{
    static inline projections::parameters<CT> apply(srs::epsg const& params)
    {
        return projections::detail::pj_init_plus<CT>(
                srs::dynamic(),
                projections::detail::epsg_to_string(params.code),
                false);
    }  
};

template <int Code, typename CT>
class proj_wrapper<srs::static_epsg<Code>, CT>
    : public static_proj_wrapper_base
        <
            typename projections::detail::epsg_traits<Code>::static_parameters_type,
            CT
        >
{
    typedef projections::detail::epsg_traits<Code> epsg_traits;
    typedef typename epsg_traits::static_parameters_type static_parameters_type;
    typedef static_proj_wrapper_base<static_parameters_type, CT> base_t;

public:
    proj_wrapper()
        : base_t(epsg_traits::s_par(), epsg_traits::par())
    {}
};


} // namespace projections


namespace srs
{


template <int Code, typename CT>
class projection<srs::static_epsg<Code>, CT>
    : public projections::projection<srs::static_epsg<Code>, CT>
{
    typedef projections::projection<srs::static_epsg<Code>, CT> base_t;

public:
    projection()
    {}
};


} // namespace srs


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_SRS_EPSG_HPP
