// Boost.Geometry

// Copyright (c) 2017-2019, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP
#define BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP

#include <string>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/radian_access.hpp>

#if defined(TEST_WITH_PROJ6)
#define TEST_WITH_PROJ5
#endif

#if defined(TEST_WITH_PROJ5)
#define TEST_WITH_PROJ4

#include <proj.h>

#endif

#if defined(TEST_WITH_PROJ4)
#define ACCEPT_USE_OF_DEPRECATED_PROJ_API_H

#include <proj_api.h>

struct pj_ptr
{
    explicit pj_ptr(projPJ ptr = NULL)
        : m_ptr(ptr)
    {}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    pj_ptr(pj_ptr && other)
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = NULL;
    }

    pj_ptr & operator=(pj_ptr && other)
    {
        if (m_ptr)
            pj_free(m_ptr);
        m_ptr = other.m_ptr;
        other.m_ptr = NULL;
        return *this;
    }
#endif

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
    pj_ptr(pj_ptr const&);
    pj_ptr & operator=(pj_ptr const&);

    projPJ m_ptr;
};
/*
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
};*/

struct pj_transformation
{
    pj_transformation(std::string const& from, std::string const& to)
        : m_from(pj_init_plus(from.c_str()))
        , m_to(pj_init_plus(to.c_str()))
    {}

    void forward(std::vector<double> in_x,
                 std::vector<double> in_y,
                 std::vector<double> & out_x,
                 std::vector<double> & out_y) const
    {
        assert(in_x.size() == in_y.size());
        pj_transform(m_from.get(), m_to.get(), (long)in_x.size(), 1, &in_x[0], &in_y[0], NULL);
        out_x = std::move(in_x);
        out_y = std::move(in_y);
    }

    void forward(std::vector<double> in_xy,
                 std::vector<double> & out_xy) const
    {
        assert(in_xy.size() % 2 == 0);
        pj_transform(m_from.get(), m_to.get(), (long)in_xy.size() / 2, 2, &in_xy[0], &in_xy[1], NULL);
        out_xy = std::move(in_xy);
    }

    void forward(std::vector<std::vector<double> > const& in_xy,
                 std::vector<std::vector<double> > & out_xy) const
    {
        out_xy.resize(in_xy.size());
        for (size_t i = 0 ; i < in_xy.size(); ++i)
            forward(in_xy[i], out_xy[i]);
    }

    template <typename In, typename Out>
    void forward(In const& in, Out & out,
                 typename boost::enable_if_c
                    <
                        boost::is_same
                            <
                                typename boost::geometry::tag<In>::type,
                                boost::geometry::point_tag
                            >::value
                    >::type* dummy = 0) const
    {
        transform_point(in, out, m_from, m_to);
    }

    template <typename In, typename Out>
    void inverse(In const& in, Out & out,
                 typename boost::enable_if_c
                    <
                        boost::is_same
                            <
                                typename boost::geometry::tag<In>::type,
                                boost::geometry::point_tag
                            >::value
                    >::type* dummy = 0) const
    {
        transform_point(in, out, m_to, m_from);
    }

private:
    template <typename In, typename Out>
    static void transform_point(In const& in, Out & out,
                                pj_ptr const& from, pj_ptr const& to)
    {
        double x = boost::geometry::get_as_radian<0>(in);
        double y = boost::geometry::get_as_radian<1>(in);

        pj_transform(from.get(), to.get(), 1, 0, &x, &y, NULL);

        boost::geometry::set_from_radian<0>(out, x);
        boost::geometry::set_from_radian<1>(out, y);
    }

    pj_ptr m_from;
    pj_ptr m_to;
};

#endif // TEST_WITH_PROJ4

#if defined(TEST_WITH_PROJ5)

struct proj5_ptr
{
    explicit proj5_ptr(PJ *ptr = NULL)
        : m_ptr(ptr)
    {}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    proj5_ptr(proj5_ptr && other)
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = NULL;
    }

    proj5_ptr & operator=(proj5_ptr && other)
    {
        if (m_ptr)
            proj_destroy(m_ptr);
        m_ptr = other.m_ptr;
        other.m_ptr = NULL;
        return *this;
    }
#endif

    PJ *get() const
    {
        return m_ptr;
    }

    ~proj5_ptr()
    {
        if (m_ptr)
            proj_destroy(m_ptr);
    }

private:
    proj5_ptr(proj5_ptr const&);
    proj5_ptr & operator=(proj5_ptr const&);

    PJ *m_ptr;
};

struct proj5_transformation
{
    proj5_transformation(std::string const& to)
        : m_proj(proj_create(PJ_DEFAULT_CTX, to.c_str()))
    {}

    void forward(std::vector<PJ_COORD> in,
                 std::vector<PJ_COORD> & out) const
    {
        proj_trans_array(m_proj.get(), PJ_FWD, in.size(), &in[0]);
        out = std::move(in);
    }

    template <typename In, typename Out>
    void forward(In const& in, Out & out,
                 typename boost::enable_if_c
                    <
                        boost::is_same
                            <
                                typename boost::geometry::tag<In>::type,
                                boost::geometry::point_tag
                            >::value
                    >::type* dummy = 0) const
    {
        PJ_COORD c;
        c.lp.lam = boost::geometry::get_as_radian<0>(in);
        c.lp.phi = boost::geometry::get_as_radian<1>(in);

        c = proj_trans(m_proj.get(), PJ_FWD, c);

        boost::geometry::set_from_radian<0>(out, c.xy.x);
        boost::geometry::set_from_radian<1>(out, c.xy.y);
    }

private:
    proj5_ptr m_proj;
};

#endif // TEST_WITH_PROJ5

#if defined(TEST_WITH_PROJ6)

struct proj6_transformation
{
    proj6_transformation(std::string const& from, std::string const& to)
        : m_proj(proj_create_crs_to_crs(PJ_DEFAULT_CTX, from.c_str(), to.c_str(), NULL))
    {
        //proj_normalize_for_visualization(0, m_proj.get());
    }

    void forward(std::vector<PJ_COORD> in,
                 std::vector<PJ_COORD> & out) const
    {
        proj_trans_array(m_proj.get(), PJ_FWD, in.size(), &in[0]);
        out = std::move(in);
    }

    template <typename In, typename Out>
    void forward(In const& in, Out & out,
                 typename boost::enable_if_c
                    <
                        boost::is_same
                            <
                                typename boost::geometry::tag<In>::type,
                                boost::geometry::point_tag
                            >::value
                    >::type* dummy = 0) const
    {
        PJ_COORD c;
        c.lp.lam = boost::geometry::get_as_radian<0>(in);
        c.lp.phi = boost::geometry::get_as_radian<1>(in);

        c = proj_trans(m_proj.get(), PJ_FWD, c);

        boost::geometry::set_from_radian<0>(out, c.xy.x);
        boost::geometry::set_from_radian<1>(out, c.xy.y);
    }

private:
    proj5_ptr m_proj;
};

#endif // TEST_WITH_PROJ6

#endif // BOOST_GEOMETRY_TEST_SRS_PROJ4_HPP
