// Boost.Geometry

// Copyright (c) 2017-2019, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP
#define BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP

#ifdef TEST_WITH_PROJ4

#include <string>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/radian_access.hpp>

#include <proj_api.h>

struct pj_ptr
{
    explicit pj_ptr(projPJ ptr)
        : m_ptr(ptr)
    {
        if (ptr == NULL)
            throw std::runtime_error("bleh");
    }

    projPJ get() const
    {
        return m_ptr;
    }

    ~pj_ptr()
    {
        if (m_ptr)
            pj_free(m_ptr);
    }

private:
    projPJ m_ptr;
};

struct pj_projection
{
    pj_projection(std::string const& prj)
        : m_ptr(pj_init_plus(prj.c_str()))
    {}

    template <typename In, typename Out>
    void forward(In const& in, Out & out) const
    {
        double x = boost::geometry::get_as_radian<0>(in);
        double y = boost::geometry::get_as_radian<1>(in);
    
        projUV p1;
        projUV p2;

        p1.u = x;
        p1.v = y;

        p2 = pj_fwd(p1, m_ptr.get());

        boost::geometry::set_from_radian<0>(out, p2.u);
        boost::geometry::set_from_radian<1>(out, p2.v);
    }

    template <typename In, typename Out>
    void inverse(In const& in, Out & out) const
    {
        double lon = boost::geometry::get_as_radian<0>(in);
        double lat = boost::geometry::get_as_radian<1>(in);
    
        projUV p1;
        projUV p2;

        p1.u = lon;
        p1.v = lat;

        p2 = pj_inv(p1, m_ptr.get());

        boost::geometry::set_from_radian<0>(out, p2.u);
        boost::geometry::set_from_radian<1>(out, p2.v);
    }

private:
    pj_ptr m_ptr;
};

struct pj_transformation
{
    pj_transformation(std::string const& from, std::string const& to)
        : m_from(pj_init_plus(from.c_str()))
        , m_to(pj_init_plus(to.c_str()))
    {}

    template <typename In, typename Out>
    void forward(In const& in, Out & out) const
    {
        double x = boost::geometry::get_as_radian<0>(in);
        double y = boost::geometry::get_as_radian<1>(in);
    
        pj_transform(m_from.get(), m_to.get(), 1, 0, &x, &y, NULL);

        boost::geometry::set_from_radian<0>(out, x);
        boost::geometry::set_from_radian<1>(out, y);
    }

    void forward(std::vector<double> in_x,
                 std::vector<double> in_y,
                 std::vector<double> & out_x,
                 std::vector<double> & out_y) const
    {
        assert(in_x.size() == in_y.size());
        pj_transform(m_from.get(), m_to.get(), in_x.size(), 1, &in_x[0], &in_y[0], NULL);
        out_x = std::move(in_x);
        out_y = std::move(in_y);
    }

    void forward(std::vector<double> in_xy,
                 std::vector<double> & out_xy) const
    {
        assert(in_xy.size() % 2 == 0);
        pj_transform(m_from.get(), m_to.get(), in_xy.size() / 2, 2, &in_xy[0], &in_xy[1], NULL);
        out_xy = std::move(in_xy);
    }

    void forward(std::vector<std::vector<double> > const& in_xy,
                 std::vector<std::vector<double> > & out_xy) const
    {
        out_xy.resize(in_xy.size());
        for (size_t i = 0 ; i < in_xy.size(); ++i)
            forward(in_xy[i], out_xy[i]);
    }

private:
    pj_ptr m_from;
    pj_ptr m_to;
};

#endif // TEST_WITH_PROJ4

#endif // BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP
