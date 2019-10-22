// Boost.Geometry Index
// OpenGL visualization

// Copyright (c) 2011-2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <GL/glut.h>

#include <boost/foreach.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/index/detail/rtree/utilities/gl_draw.hpp>
#include <boost/geometry/index/detail/rtree/utilities/print.hpp>
#include <boost/geometry/index/detail/rtree/utilities/are_boxes_ok.hpp>
#include <boost/geometry/index/detail/rtree/utilities/are_levels_ok.hpp>
#include <boost/geometry/index/detail/rtree/utilities/statistics.hpp>

#include <boost/variant.hpp>

#define ENABLE_POINTS_AND_SEGMENTS

namespace bg = boost::geometry;
namespace bgi = bg::index;

// used types

typedef bg::model::point<float, 2, boost::geometry::cs::cartesian> P;
typedef bg::model::box<P> B;
typedef bg::model::linestring<P> LS;
typedef bg::model::segment<P> S;
typedef bg::model::ring<P> R;
typedef bg::model::polygon<P> Poly;
typedef bg::model::multi_polygon<Poly> MPoly;

// containers variant

template <typename V>
struct containers
{
    containers & operator=(containers const& c)
    {
        tree = c.tree;
        values = c.values;
        result = c.result;
        return *this;
    }

    bgi::rtree< V, bgi::rstar<4, 2> > tree;
    std::vector<V> values;
    std::vector<V> result;
};

boost::variant<
    containers<B>
#ifdef ENABLE_POINTS_AND_SEGMENTS
  , containers<P>
  , containers<S>
#endif
> cont;

// visitors

template <typename Pred>
struct query_v : boost::static_visitor<size_t>
{
    Pred m_pred;
    query_v(Pred const& pred) : m_pred(pred) {}

    template <typename C>
    size_t operator()(C & c) const
    {
        c.result.clear();
        return c.tree.query(m_pred, std::back_inserter(c.result));
    }
};
template <typename Cont, typename Pred>
inline size_t query(Cont & cont, Pred const& pred)
{
    return boost::apply_visitor(query_v<Pred>(pred), cont);
}

struct print_result_v : boost::static_visitor<>
{
    template <typename C>
    void operator()(C & c) const
    {
        for ( size_t i = 0 ; i < c.result.size() ; ++i )
        {
            bgi::detail::utilities::print_indexable(std::cout, c.result[i]);
            std::cout << '\n';
        }
    }
};
template <typename Cont>
inline void print_result(Cont const& cont)
{
    boost::apply_visitor(print_result_v(), cont);
}

struct bounds_v : boost::static_visitor<B>
{
    template <typename C>
    B operator()(C & c) const
    {
        return c.tree.bounds();
    }
};
template <typename Cont>
inline B bounds(Cont const& cont)
{
    return boost::apply_visitor(bounds_v(), cont);
}

struct depth_v : boost::static_visitor<size_t>
{
    template <typename C>
    size_t operator()(C & c) const
    {
        return get(c.tree);
    }
    template <typename RTree>
    static size_t get(RTree const& t)
    {
        return bgi::detail::rtree::utilities::view<RTree>(t).depth();
    }
};
template <typename Cont>
inline size_t depth(Cont const& cont)
{
    return boost::apply_visitor(depth_v(), cont);
}

struct draw_tree_v : boost::static_visitor<>
{
    template <typename C>
    void operator()(C & c) const
    {
        bgi::detail::rtree::utilities::gl_draw(c.tree);
    }
};
template <typename Cont>
inline void draw_tree(Cont const& cont)
{
    return boost::apply_visitor(draw_tree_v(), cont);
}

struct draw_result_v : boost::static_visitor<>
{
    template <typename C>
    void operator()(C & c) const
    {
        for ( size_t i = 0 ; i < c.result.size() ; ++i )
        {
            bgi::detail::utilities::gl_draw_indexable(c.result[i], depth_v::get(c.tree));
        }
    }
};
template <typename Cont>
inline void draw_result(Cont const& cont)
{
    return boost::apply_visitor(draw_result_v(), cont);
}

struct print_tree_v : boost::static_visitor<>
{
    template <typename C>
    void operator()(C & c) const
    {
        bgi::detail::rtree::utilities::print(std::cout, c.tree);        
    }
};
template <typename Cont>
inline void print_tree(Cont const& cont)
{
    return boost::apply_visitor(print_tree_v(), cont);
}

// globals used in querying

size_t found_count = 0;
size_t count = 5;

P search_point;
B search_box;
R search_ring;
Poly search_poly;
MPoly search_multi_poly;
S search_segment;
LS search_linestring;
LS search_path;

enum query_mode_type {
    qm_knn, qm_knnb, qm_knns, qm_c, qm_d, qm_i, qm_o, qm_w, qm_nc, qm_nd, qm_ni, qm_no, qm_nw, qm_all, qm_ri, qm_pi, qm_mpi, qm_si, qm_lsi, qm_path
} query_mode = qm_knn;

bool search_valid = false;

// various queries

void query_knn()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;

    if ( query_mode == qm_knn )
    {
        search_point = P(x, y);
        found_count = query(cont, bgi::nearest(search_point, count));
    }
    else if ( query_mode == qm_knnb )
    {
        float w = 2 + ( rand() % 1000 ) / 500.0f;
        float h = 2 + ( rand() % 1000 ) / 500.0f;
        search_box = B(P(x - w, y - h), P(x + w, y + h));
        found_count = query(cont, bgi::nearest(search_box, count));
    }
    else if ( query_mode == qm_knns )
    {
        int signx = rand() % 2 ? 1 : -1;
        int signy = rand() % 2 ? 1 : -1;
        float w = (10 + ( rand() % 1000 ) / 100.0f) * signx;
        float h = (10 + ( rand() % 1000 ) / 100.0f) * signy;
        search_segment = S(P(x - w, y - h), P(x + w, y + h));
        found_count = query(cont, bgi::nearest(search_segment, count));
    }
    else
    {
        BOOST_ASSERT(false);
    }

    if ( found_count > 0 )
    {
        if ( query_mode == qm_knn )
        {
            std::cout << "search point: ";
            bgi::detail::utilities::print_indexable(std::cout, search_point);
        }
        else if ( query_mode == qm_knnb )
        {
            std::cout << "search box: ";
            bgi::detail::utilities::print_indexable(std::cout, search_box);
        }
        else if ( query_mode == qm_knns )
        {
            std::cout << "search segment: ";
            bgi::detail::utilities::print_indexable(std::cout, search_segment);
        }
        else
        {
            BOOST_ASSERT(false);
        }
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "nearest not found\n";
}

#ifndef ENABLE_POINTS_AND_SEGMENTS
void query_path()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 20 + ( rand() % 1000 ) / 100.0f;
    float h = 20 + ( rand() % 1000 ) / 100.0f;

    search_path.resize(10);
    float yy = y-h;
    for ( int i = 0 ; i < 5 ; ++i, yy += h / 2 )
    {
        search_path[2 * i] = P(x-w, yy);
        search_path[2 * i + 1] = P(x+w, yy);
    }
        
    found_count = query(cont, bgi::detail::path<LS>(search_path, count));

    if ( found_count > 0 )
    {
        std::cout << "search path: ";
        BOOST_FOREACH(P const& p, search_path)
            bgi::detail::utilities::print_indexable(std::cout, p);
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "values on path not found\n";
}
#endif

template <typename Predicate>
void query()
{
    if ( query_mode != qm_all )
    {
        float x = ( rand() % 1000 ) / 10.0f;
        float y = ( rand() % 1000 ) / 10.0f;
        float w = 10 + ( rand() % 1000 ) / 100.0f;
        float h = 10 + ( rand() % 1000 ) / 100.0f;

        search_box = B(P(x - w, y - h), P(x + w, y + h));
    }
    else
    {
        search_box = bounds(cont);
    }

    found_count = query(cont, Predicate(search_box));

    if ( found_count > 0 )
    {
        std::cout << "search box: ";
        bgi::detail::utilities::print_indexable(std::cout, search_box);
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

template <typename Predicate>
void query_ring()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 10 + ( rand() % 1000 ) / 100.0f;
    float h = 10 + ( rand() % 1000 ) / 100.0f;

    search_ring.clear();
    search_ring.push_back(P(x - w, y - h));
    search_ring.push_back(P(x - w/2, y - h));
    search_ring.push_back(P(x, y - 3*h/2));
    search_ring.push_back(P(x + w/2, y - h));
    search_ring.push_back(P(x + w, y - h));
    search_ring.push_back(P(x + w, y - h/2));
    search_ring.push_back(P(x + 3*w/2, y));
    search_ring.push_back(P(x + w, y + h/2));
    search_ring.push_back(P(x + w, y + h));
    search_ring.push_back(P(x + w/2, y + h));
    search_ring.push_back(P(x, y + 3*h/2));
    search_ring.push_back(P(x - w/2, y + h));
    search_ring.push_back(P(x - w, y + h));
    search_ring.push_back(P(x - w, y + h/2));
    search_ring.push_back(P(x - 3*w/2, y));
    search_ring.push_back(P(x - w, y - h/2));
    search_ring.push_back(P(x - w, y - h));
        
    found_count = query(cont, Predicate(search_ring));
    
    if ( found_count > 0 )
    {
        std::cout << "search ring: ";
        BOOST_FOREACH(P const& p, search_ring)
        {
            bgi::detail::utilities::print_indexable(std::cout, p);
            std::cout << ' ';
        }
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

template <typename Predicate>
void query_poly()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 10 + ( rand() % 1000 ) / 100.0f;
    float h = 10 + ( rand() % 1000 ) / 100.0f;

    search_poly.clear();
    search_poly.outer().push_back(P(x - w, y - h));
    search_poly.outer().push_back(P(x - w/2, y - h));
    search_poly.outer().push_back(P(x, y - 3*h/2));
    search_poly.outer().push_back(P(x + w/2, y - h));
    search_poly.outer().push_back(P(x + w, y - h));
    search_poly.outer().push_back(P(x + w, y - h/2));
    search_poly.outer().push_back(P(x + 3*w/2, y));
    search_poly.outer().push_back(P(x + w, y + h/2));
    search_poly.outer().push_back(P(x + w, y + h));
    search_poly.outer().push_back(P(x + w/2, y + h));
    search_poly.outer().push_back(P(x, y + 3*h/2));
    search_poly.outer().push_back(P(x - w/2, y + h));
    search_poly.outer().push_back(P(x - w, y + h));
    search_poly.outer().push_back(P(x - w, y + h/2));
    search_poly.outer().push_back(P(x - 3*w/2, y));
    search_poly.outer().push_back(P(x - w, y - h/2));
    search_poly.outer().push_back(P(x - w, y - h));

    search_poly.inners().push_back(Poly::ring_type());
    search_poly.inners()[0].push_back(P(x - w/2, y - h/2));
    search_poly.inners()[0].push_back(P(x + w/2, y - h/2));
    search_poly.inners()[0].push_back(P(x + w/2, y + h/2));
    search_poly.inners()[0].push_back(P(x - w/2, y + h/2));
    search_poly.inners()[0].push_back(P(x - w/2, y - h/2));

    found_count = query(cont, Predicate(search_poly));

    if ( found_count > 0 )
    {
        std::cout << "search poly outer: ";
        BOOST_FOREACH(P const& p, search_poly.outer())
        {
            bgi::detail::utilities::print_indexable(std::cout, p);
            std::cout << ' ';
        }
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

template <typename Predicate>
void query_multi_poly()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 10 + ( rand() % 1000 ) / 100.0f;
    float h = 10 + ( rand() % 1000 ) / 100.0f;

    search_multi_poly.clear();

    search_multi_poly.push_back(Poly());
    search_multi_poly[0].outer().push_back(P(x - w, y - h));
    search_multi_poly[0].outer().push_back(P(x - w/2, y - h));
    search_multi_poly[0].outer().push_back(P(x, y - 3*h/2));
    search_multi_poly[0].outer().push_back(P(x + w/2, y - h));
    search_multi_poly[0].outer().push_back(P(x + w, y - h));
    search_multi_poly[0].outer().push_back(P(x + w, y - h/2));
    search_multi_poly[0].outer().push_back(P(x + 3*w/2, y));
    search_multi_poly[0].outer().push_back(P(x + w, y + h/2));
    search_multi_poly[0].outer().push_back(P(x + w, y + h));
    search_multi_poly[0].outer().push_back(P(x + w/2, y + h));
    search_multi_poly[0].outer().push_back(P(x, y + 3*h/2));
    search_multi_poly[0].outer().push_back(P(x - w/2, y + h));
    search_multi_poly[0].outer().push_back(P(x - w, y + h));
    search_multi_poly[0].outer().push_back(P(x - w, y + h/2));
    search_multi_poly[0].outer().push_back(P(x - 3*w/2, y));
    search_multi_poly[0].outer().push_back(P(x - w, y - h/2));
    search_multi_poly[0].outer().push_back(P(x - w, y - h));

    search_multi_poly[0].inners().push_back(Poly::ring_type());
    search_multi_poly[0].inners()[0].push_back(P(x - w/2, y - h/2));
    search_multi_poly[0].inners()[0].push_back(P(x + w/2, y - h/2));
    search_multi_poly[0].inners()[0].push_back(P(x + w/2, y + h/2));
    search_multi_poly[0].inners()[0].push_back(P(x - w/2, y + h/2));
    search_multi_poly[0].inners()[0].push_back(P(x - w/2, y - h/2));

    search_multi_poly.push_back(Poly());
    search_multi_poly[1].outer().push_back(P(x - 2*w, y - 2*h));
    search_multi_poly[1].outer().push_back(P(x - 6*w/5, y - 2*h));
    search_multi_poly[1].outer().push_back(P(x - 6*w/5, y - 6*h/5));
    search_multi_poly[1].outer().push_back(P(x - 2*w, y - 6*h/5));
    search_multi_poly[1].outer().push_back(P(x - 2*w, y - 2*h));

    search_multi_poly.push_back(Poly());
    search_multi_poly[2].outer().push_back(P(x + 6*w/5, y + 6*h/5));
    search_multi_poly[2].outer().push_back(P(x + 2*w, y + 6*h/5));
    search_multi_poly[2].outer().push_back(P(x + 2*w, y + 2*h));
    search_multi_poly[2].outer().push_back(P(x + 6*w/5, y + 2*h));
    search_multi_poly[2].outer().push_back(P(x + 6*w/5, y + 6*h/5));

    found_count = query(cont, Predicate(search_multi_poly));
    
    if ( found_count > 0 )
    {
        std::cout << "search multi_poly[0] outer: ";
        BOOST_FOREACH(P const& p, search_multi_poly[0].outer())
        {
            bgi::detail::utilities::print_indexable(std::cout, p);
            std::cout << ' ';
        }
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

template <typename Predicate>
void query_segment()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 10.0f - ( rand() % 1000 ) / 50.0f;
    float h = 10.0f - ( rand() % 1000 ) / 50.0f;
    w += 0 <= w ? 10 : -10;
    h += 0 <= h ? 10 : -10;

    boost::geometry::set<0, 0>(search_segment, x - w);
    boost::geometry::set<0, 1>(search_segment, y - h);
    boost::geometry::set<1, 0>(search_segment, x + w);
    boost::geometry::set<1, 1>(search_segment, y + h);

    found_count = query(cont, Predicate(search_segment));
    
    if ( found_count > 0 )
    {
        std::cout << "search segment: ";
        bgi::detail::utilities::print_indexable(std::cout, P(x-w, y-h));
        bgi::detail::utilities::print_indexable(std::cout, P(x+w, y+h));

        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

template <typename Predicate>
void query_linestring()
{
    float x = ( rand() % 1000 ) / 10.0f;
    float y = ( rand() % 1000 ) / 10.0f;
    float w = 10 + ( rand() % 1000 ) / 100.0f;
    float h = 10 + ( rand() % 1000 ) / 100.0f;

    search_linestring.clear();
    float a = 0;
    float d = 0;
    for ( size_t i = 0 ; i < 300 ; ++i, a += 0.05, d += 0.005 )
    {
        float xx = x + w * d * ::cos(a);
        float yy = y + h * d * ::sin(a);
        search_linestring.push_back(P(xx, yy));
    }

    found_count = query(cont, Predicate(search_linestring));
    
    if ( found_count > 0 )
    {
        std::cout << "search linestring: ";
        BOOST_FOREACH(P const& p, search_linestring)
        {
            bgi::detail::utilities::print_indexable(std::cout, p);
            std::cout << ' ';
        }
        std::cout << "\nfound: ";
        print_result(cont);
    }
    else
        std::cout << "boxes not found\n";
}

// the function running the correct query based on the query_mode

void search()
{
    namespace d = bgi::detail;

    if ( query_mode == qm_knn || query_mode == qm_knnb || query_mode == qm_knns )
        query_knn();
    else if ( query_mode == qm_d )
        query< d::spatial_predicate<B, d::disjoint_tag, false> >();
    else if ( query_mode == qm_i )
        query< d::spatial_predicate<B, d::intersects_tag, false> >();
    else if ( query_mode == qm_nd )
        query< d::spatial_predicate<B, d::disjoint_tag, true> >();
    else if ( query_mode == qm_ni )
        query< d::spatial_predicate<B, d::intersects_tag, true> >();
    else if ( query_mode == qm_all )
        query< d::spatial_predicate<B, d::intersects_tag, false> >();
#ifdef ENABLE_POINTS_AND_SEGMENTS
    else
        std::cout << "query disabled\n";
#else
    else if ( query_mode == qm_c )
        query< d::spatial_predicate<B, d::covered_by_tag, false> >();
    else if ( query_mode == qm_o )
        query< d::spatial_predicate<B, d::overlaps_tag, false> >();
    else if ( query_mode == qm_w )
        query< d::spatial_predicate<B, d::within_tag, false> >();
    else if ( query_mode == qm_nc )
        query< d::spatial_predicate<B, d::covered_by_tag, true> >();
    else if ( query_mode == qm_no )
        query< d::spatial_predicate<B, d::overlaps_tag, true> >();
    else if ( query_mode == qm_nw )
        query< d::spatial_predicate<B, d::within_tag, true> >();
    else if ( query_mode == qm_ri )
        query_ring< d::spatial_predicate<R, d::intersects_tag, false> >();
    else if ( query_mode == qm_pi )
        query_poly< d::spatial_predicate<Poly, d::intersects_tag, false> >();
    else if ( query_mode == qm_mpi )
        query_multi_poly< d::spatial_predicate<MPoly, d::intersects_tag, false> >();
    else if ( query_mode == qm_si )
        query_segment< d::spatial_predicate<S, d::intersects_tag, false> >();
    else if ( query_mode == qm_lsi )
        query_linestring< d::spatial_predicate<LS, d::intersects_tag, false> >();
    else if ( query_mode == qm_path )
        query_path();
#endif

    search_valid = true;
}

// various drawing functions

void draw_point(P const& p)
{
    float x = boost::geometry::get<0>(p);
    float y = boost::geometry::get<1>(p);
    float z = depth(cont);

    glBegin(GL_QUADS);
    glVertex3f(x+1, y, z);
    glVertex3f(x, y+1, z);
    glVertex3f(x-1, y, z);
    glVertex3f(x, y-1, z);
    glEnd();
}

void draw_knn_area(float min_distance, float max_distance)
{
    float x = boost::geometry::get<0>(search_point);
    float y = boost::geometry::get<1>(search_point);
    float z = depth(cont);

    draw_point(search_point);

    // search min circle

    glBegin(GL_LINE_LOOP);
    for(float a = 0 ; a < 3.14158f * 2 ; a += 3.14158f / 180)
        glVertex3f(x + min_distance * ::cos(a), y + min_distance * ::sin(a), z);
    glEnd();

    // search max circle

    glBegin(GL_LINE_LOOP);
    for(float a = 0 ; a < 3.14158f * 2 ; a += 3.14158f / 180)
        glVertex3f(x + max_distance * ::cos(a), y + max_distance * ::sin(a), z);
    glEnd();
}

void draw_linestring(LS const& ls)
{
    glBegin(GL_LINE_STRIP);

    BOOST_FOREACH(P const& p, ls)
    {
        float x = boost::geometry::get<0>(p);
        float y = boost::geometry::get<1>(p);
        float z = depth(cont);
        glVertex3f(x, y, z);
    }

    glEnd();
}

void draw_segment(S const& s)
{
    float x1 = boost::geometry::get<0, 0>(s);
    float y1 = boost::geometry::get<0, 1>(s);
    float x2 = boost::geometry::get<1, 0>(s);
    float y2 = boost::geometry::get<1, 1>(s);
    float z = depth(cont);

    glBegin(GL_LINES);
    glVertex3f(x1, y1, z);
    glVertex3f(x2, y2, z);
    glEnd();
}

template <typename Box>
void draw_box(Box const& box)
{
    float x1 = boost::geometry::get<bg::min_corner, 0>(box);
    float y1 = boost::geometry::get<bg::min_corner, 1>(box);
    float x2 = boost::geometry::get<bg::max_corner, 0>(box);
    float y2 = boost::geometry::get<bg::max_corner, 1>(box);
    float z = depth(cont);

    // search box
    glBegin(GL_LINE_LOOP);
        glVertex3f(x1, y1, z);
        glVertex3f(x2, y1, z);
        glVertex3f(x2, y2, z);
        glVertex3f(x1, y2, z);
    glEnd();
}

template <typename Range>
void draw_ring(Range const& range)
{
    float z = depth(cont);

    // search box
    glBegin(GL_LINE_LOOP);
    
    BOOST_FOREACH(P const& p, range)
    {
        float x = boost::geometry::get<0>(p);
        float y = boost::geometry::get<1>(p);

        glVertex3f(x, y, z);
    }
    glEnd();
}

template <typename Polygon>
void draw_polygon(Polygon const& polygon)
{
    draw_ring(polygon.outer());
    BOOST_FOREACH(Poly::ring_type const& r, polygon.inners())
        draw_ring(r);
}

template <typename MultiPolygon>
void draw_multi_polygon(MultiPolygon const& multi_polygon)
{
    BOOST_FOREACH(Poly const& p, multi_polygon)
        draw_polygon(p);
}

// render the scene -> tree, if searching data available also the query geometry and result

void render_scene(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    draw_tree(cont);

    if ( search_valid )
    {
        glColor3f(1.0f, 0.25f, 0.0f);

        if ( query_mode == qm_knn )
            draw_knn_area(0, 0);
        else if ( query_mode == qm_knnb )
            draw_box(search_box);
        else if ( query_mode == qm_knns )
            draw_segment(search_segment);
        else if ( query_mode == qm_ri )
            draw_ring(search_ring);
        else if ( query_mode == qm_pi )
            draw_polygon(search_poly);
        else if ( query_mode == qm_mpi )
            draw_multi_polygon(search_multi_poly);
        else if ( query_mode == qm_si )
            draw_segment(search_segment);
        else if ( query_mode == qm_lsi )
            draw_linestring(search_linestring);
        else if ( query_mode == qm_path )
            draw_linestring(search_path);
        else
            draw_box(search_box);

        glColor3f(1.0f, 0.5f, 0.0f);

        draw_result(cont);
    }

    glFlush();
}

void resize(int w, int h)
{
    if ( h == 0 )
        h = 1;

    //float ratio = float(w) / h;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glViewport(0, 0, w, h);

    //gluPerspective(45, ratio, 1, 1000);
    glOrtho(-150, 150, -150, 150, -150, 150);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    /*gluLookAt(
        120.0f, 120.0f, 120.0f, 
        50.0f, 50.0f, -1.0f,
        0.0f, 1.0f, 0.0f);*/
    gluLookAt(
        50.0f, 50.0f, 75.0f, 
        50.0f, 50.0f, -1.0f,
        0.0f, 1.0f, 0.0f);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glLineWidth(1.5f);

    srand(1);
}

// randomize various indexables

inline void rand_val(B & b)
{
    float x = ( rand() % 100 );
    float y = ( rand() % 100 );
    float w = ( rand() % 2 ) + 1;
    float h = ( rand() % 2 ) + 1;
    b = B(P(x - w, y - h),P(x + w, y + h));
}
inline void rand_val(P & p)
{
    float x = ( rand() % 100 );
    float y = ( rand() % 100 );
    p = P(x, y);
}
inline void rand_val(S & s)
{
    float x = ( rand() % 100 );
    float y = ( rand() % 100 );
    float w = ( rand() % 2 + 1) * (rand() % 2 ? 1.0f : -1.0f);
    float h = ( rand() % 2 + 1) * (rand() % 2 ? 1.0f : -1.0f);
    s = S(P(x - w, y - h),P(x + w, y + h));
}

// more higher-level visitors

struct insert_random_value_v : boost::static_visitor<>
{
    template <typename V>
    void operator()(containers<V> & c) const
    {
        V v;
        rand_val(v);
        
        boost::geometry::index::insert(c.tree, v);
        c.values.push_back(v);

        std::cout << "inserted: ";
        bgi::detail::utilities::print_indexable(std::cout, v);
        std::cout << '\n';

        std::cout << ( bgi::detail::rtree::utilities::are_boxes_ok(c.tree) ? "boxes OK\n" : "WRONG BOXES!\n" );
        std::cout << ( bgi::detail::rtree::utilities::are_levels_ok(c.tree) ? "levels OK\n" : "WRONG LEVELS!\n" );
        std::cout << "\n";
    }
};
template <typename Cont>
inline void insert_random_value(Cont & cont)
{
    return boost::apply_visitor(insert_random_value_v(), cont);
}

struct remove_random_value_v : boost::static_visitor<>
{
    template <typename V>
    void operator()(containers<V> & c) const
    {
        if ( c.values.empty() )
            return;

        size_t i = rand() % c.values.size();
        V v = c.values[i];

        c.tree.remove(v);
        c.values.erase(c.values.begin() + i);

        std::cout << "removed: ";
        bgi::detail::utilities::print_indexable(std::cout, v);
        std::cout << '\n';

        std::cout << ( bgi::detail::rtree::utilities::are_boxes_ok(c.tree) ? "boxes OK\n" : "WRONG BOXES!\n" );
        std::cout << ( bgi::detail::rtree::utilities::are_levels_ok(c.tree) ? "levels OK\n" : "WRONG LEVELS!\n" );
        std::cout << "\n";
    }
};
template <typename Cont>
inline void remove_random_value(Cont & cont)
{
    return boost::apply_visitor(remove_random_value_v(), cont);
}

// handle mouse input

void mouse(int button, int state, int /*x*/, int /*y*/)
{
    if ( button == GLUT_LEFT_BUTTON && state == GLUT_DOWN )
    {
        insert_random_value(cont);
        search_valid = false;
    }
    else if ( button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN )
    {
        remove_random_value(cont);
        search_valid = false;
    }
    else if ( button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN )
    {
        search();
    }

    glutPostRedisplay();
}

// more higher-level visitors

struct insert_random_values_v : boost::static_visitor<>
{
    template <typename V>
    void operator()(containers<V> & c) const
    {
        for ( size_t i = 0 ; i < 35 ; ++i )
        {
            V v;
            rand_val(v);

            c.tree.insert(v);
            c.values.push_back(v);

            std::cout << "inserted: ";
            bgi::detail::utilities::print_indexable(std::cout, v);
            std::cout << '\n';
        }

        std::cout << ( bgi::detail::rtree::utilities::are_boxes_ok(c.tree) ? "boxes OK\n" : "WRONG BOXES!\n" );
        std::cout << ( bgi::detail::rtree::utilities::are_levels_ok(c.tree) ? "levels OK\n" : "WRONG LEVELS!\n" );
        std::cout << "\n";
    }
};
template <typename Cont>
inline void insert_random_values(Cont & cont)
{
    return boost::apply_visitor(insert_random_values_v(), cont);
}

struct bulk_insert_random_values_v : boost::static_visitor<>
{
    template <typename V>
    void operator()(containers<V> & c) const
    {
        c.values.clear();

        for ( size_t i = 0 ; i < 35 ; ++i )
        {
            V v;
            rand_val(v);

            c.values.push_back(v);

            std::cout << "inserted: ";
            bgi::detail::utilities::print_indexable(std::cout, v);
            std::cout << '\n';
        }

        create(c.tree, c.values);

        std::cout << ( bgi::detail::rtree::utilities::are_boxes_ok(c.tree) ? "boxes OK\n" : "WRONG BOXES!\n" );
        std::cout << ( bgi::detail::rtree::utilities::are_levels_ok(c.tree) ? "levels OK\n" : "WRONG LEVELS!\n" );
        std::cout << "\n";
    }

    template <typename Tree, typename Values>
    void create(Tree & tree, Values const& values) const
    {
        Tree t(values);
        tree = boost::move(t);
    }
};
template <typename Cont>
inline void bulk_insert_random_values(Cont & cont)
{
    return boost::apply_visitor(bulk_insert_random_values_v(), cont);
}

// handle keyboard input

std::string current_line;

void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    if ( key == '\r' || key == '\n' )
    {
        if ( current_line == "storeb" )
        {
            cont = containers<B>();
            glutPostRedisplay();
        }
#ifdef ENABLE_POINTS_AND_SEGMENTS
        else if ( current_line == "storep" )
        {
            cont = containers<P>();
            glutPostRedisplay();
        }
        else if ( current_line == "stores" )
        {
            cont = containers<S>();
            glutPostRedisplay();
        }
#endif
        else if ( current_line == "t" )
        {
            std::cout << "\n";
            print_tree(cont);
            std::cout << "\n";
        }
        else if ( current_line == "rand" )
        {
            insert_random_values(cont);
            search_valid = false;

            glutPostRedisplay();
        }
        else if ( current_line == "bulk" )
        {
            bulk_insert_random_values(cont);
            search_valid = false;

            glutPostRedisplay();
        }
        else
        {
            if ( current_line == "knn" )
                query_mode = qm_knn;
            else if ( current_line == "knnb" )
                query_mode = qm_knnb;
            else if ( current_line == "knns" )
                query_mode = qm_knns;
            else if ( current_line == "c" )
                query_mode = qm_c;
            else if ( current_line == "d" )
                query_mode = qm_d;
            else if ( current_line == "i" )
                query_mode = qm_i;
            else if ( current_line == "o" )
                query_mode = qm_o;
            else if ( current_line == "w" )
                query_mode = qm_w;
            else if ( current_line == "nc" )
                query_mode = qm_nc;
            else if ( current_line == "nd" )
                query_mode = qm_nd;
            else if ( current_line == "ni" )
                query_mode = qm_ni;
            else if ( current_line == "no" )
                query_mode = qm_no;
            else if ( current_line == "nw" )
                query_mode = qm_nw;
            else if ( current_line == "all" )
                query_mode = qm_all;
            else if ( current_line == "ri" )
                query_mode = qm_ri;
            else if ( current_line == "pi" )
                query_mode = qm_pi;
            else if ( current_line == "mpi" )
                query_mode = qm_mpi;
            else if ( current_line == "si" )
                query_mode = qm_si;
            else if ( current_line == "lsi" )
                query_mode = qm_lsi;
            else if ( current_line == "path" )
                query_mode = qm_path;
            
            search();
            glutPostRedisplay();
        }

        current_line.clear();
        std::cout << '\n';
    }
    else
    {
        current_line += key;
        std::cout << key;
    }
}

// main function

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(600, 600);
    glutCreateWindow("boost::geometry::index::rtree GLUT test");

    glutDisplayFunc(render_scene);
    glutReshapeFunc(resize);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);

    glutMainLoop();

    return 0;
}
