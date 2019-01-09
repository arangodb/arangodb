// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
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
        double x = bg::get_as_radian<0>(in);
        double y = bg::get_as_radian<1>(in);
    
        projUV p1;
        projUV p2;

        p1.u = x;
        p1.v = y;

        p2 = pj_fwd(p1, m_ptr.get());

        bg::set_from_radian<0>(out, p2.u);
        bg::set_from_radian<1>(out, p2.v);
    }

    template <typename In, typename Out>
    void inverse(In const& in, Out & out) const
    {
        double lon = bg::get_as_radian<0>(in);
        double lat = bg::get_as_radian<1>(in);
    
        projUV p1;
        projUV p2;

        p1.u = lon;
        p1.v = lat;

        p2 = pj_inv(p1, pj_prj.get());

        bg::set_from_radian<0>(out, p2.u);
        bg::set_from_radian<1>(out, p2.v);
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
        double x = bg::get_as_radian<0>(in);
        double y = bg::get_as_radian<1>(in);
    
        pj_transform(m_from.get(), m_to.get(), 1, 0, &x, &y, NULL);

        bg::set_from_radian<0>(out, x);
        bg::set_from_radian<1>(out, y);
    }

    void forward(std::vector<double> in_x,
                 std::vector<double> in_y,
                 std::vector<double> & out_x,
                 std::vector<double> & out_y) const
    {
        assert(in_x.size() == in_y.size());
        pj_transform(m_from.get(), m_to.get(), in_x.size(), 1, &in_x[0], &in_y[0], NULL);
        out_x = in_x;
        out_y = in_y;
    }

private:
    pj_ptr m_from;
    pj_ptr m_to;
};

#endif // TEST_WITH_PROJ4

#endif // BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP
