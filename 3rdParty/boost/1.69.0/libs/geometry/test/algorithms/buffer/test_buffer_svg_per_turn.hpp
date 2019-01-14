// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test Helper

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_TEST_BUFFER_SVG_PER_TURN_HPP
#define BOOST_GEOMETRY_TEST_BUFFER_SVG_PER_TURN_HPP

#include <fstream>
#include <vector>

#include <test_buffer_svg.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

template <typename Point>
class save_turns_visitor
{
public :
    typedef std::vector<std::pair<Point, int> > vector_type;

    template <typename Turns>
    inline void get_turns(Turns const& turns)
    {
        for (typename boost::range_iterator<Turns const>::type it =
            boost::begin(turns); it != boost::end(turns); ++it)
        {
            m_points.push_back(std::make_pair(it->point, it->turn_index));
        }
    }

    template <typename PieceCollection>
    inline void apply(PieceCollection const& collection, int phase)
    {
        if (phase == 0)
        {
            get_turns(collection.m_turns);
        }
    }

    vector_type const& get_points() { return m_points; }

private :
    vector_type m_points;
};



template <typename Point>
class mapper_visitor
{
public :
    mapper_visitor(std::string const& complete, int index, Point const& point)
        : m_filename(get_filename(complete, index))
        , m_svg(m_filename.c_str())
        , m_mapper(m_svg, 1000, 800)
        , m_visitor(m_mapper)
        , m_buffer_mapper(m_filename)
    {
        box_type box;
        double half_size = 75.0; // specific for multi_point buffer
        bg::set<bg::min_corner, 0>(box, bg::get<0>(point) - half_size);
        bg::set<bg::min_corner, 1>(box, bg::get<1>(point) - half_size);
        bg::set<bg::max_corner, 0>(box, bg::get<0>(point) + half_size);
        bg::set<bg::max_corner, 1>(box, bg::get<1>(point) + half_size);

        m_mapper.add(box);
        m_visitor.set_alternate_box(box);
        m_buffer_mapper.set_alternate_box(box);
    }

    // It is used in a ptr vector
    virtual ~mapper_visitor()
    {
    }

    template <typename PieceCollection>
    inline void apply(PieceCollection const& collection, int phase)
    {
        m_visitor.apply(collection, phase);
    }

    template <typename Geometry, typename GeometryBuffer>
    void map_input_output(Geometry const& geometry,
            GeometryBuffer const& buffered, bool negative)
    {
        m_buffer_mapper.map_input_output(m_mapper, geometry, buffered, negative);
    }

private :
    std::string get_filename(std::string const& complete, int index)
    {
        std::ostringstream filename;
        filename << "buffer_per_turn_" << complete << "_" << index << ".svg";
        return filename.str();
    }

    typedef bg::svg_mapper<Point> mapper_type;
    typedef bg::model::box<Point> box_type;

    std::string m_filename;
    std::ofstream m_svg;
    mapper_type m_mapper;
    svg_visitor<mapper_type, box_type> m_visitor;
    buffer_svg_mapper<Point> m_buffer_mapper;
};

template <typename Point>
class per_turn_visitor
{
    // Both fstreams and svg mappers are non-copyable,
    // therefore we need to use dynamic memory
    typedef boost::ptr_vector<mapper_visitor<Point> > container_type;
    container_type mappers;

public :

    typedef std::pair<Point, int> pair_type;
    typedef std::vector<pair_type> vector_type;

    per_turn_visitor(std::string const& complete_caseid,
            vector_type const& points)
    {
        namespace bg = boost::geometry;

        if (points.size() > 100u)
        {
            // Defensive check. Too much intersections. Don't create anything
            return;
        }

        BOOST_FOREACH(pair_type const& p, points)
        {
            mappers.push_back(new mapper_visitor<Point>(complete_caseid, p.second, p.first));
        }
    }

    template <typename PieceCollection>
    inline void apply(PieceCollection const& collection, int phase)
    {
        for(typename container_type::iterator it = mappers.begin();
            it != mappers.end(); ++it)
        {
            it->apply(collection, phase);
        }
    }

    template <typename Geometry, typename GeometryBuffer>
    void map_input_output(Geometry const& geometry,
            GeometryBuffer const& buffered, bool negative)
    {
        for(typename container_type::iterator it = mappers.begin();
            it != mappers.end(); ++it)
        {
           it->map_input_output(geometry, buffered, negative);
        }
    }
};


#endif // BOOST_GEOMETRY_TEST_BUFFER_SVG_PER_TURN_HPP
