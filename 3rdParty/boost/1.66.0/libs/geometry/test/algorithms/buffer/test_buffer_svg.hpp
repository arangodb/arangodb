// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test Helper

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_TEST_BUFFER_SVG_HPP
#define BOOST_GEOMETRY_TEST_BUFFER_SVG_HPP

#include <fstream>
#include <sstream>

// Uncomment next lines if you want to have a zoomed view
//#define BOOST_GEOMETRY_BUFFER_TEST_SVG_USE_ALTERNATE_BOX

// If possible define box before including this unit with the right view
#ifdef BOOST_GEOMETRY_BUFFER_TEST_SVG_USE_ALTERNATE_BOX
#  ifndef BOOST_GEOMETRY_BUFFER_TEST_SVG_ALTERNATE_BOX
#    define BOOST_GEOMETRY_BUFFER_TEST_SVG_ALTERNATE_BOX "BOX(0 0,100 100)"
#  endif
#endif

#include <boost/foreach.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <boost/geometry/algorithms/intersection.hpp>


inline char piece_type_char(bg::strategy::buffer::piece_type const& type)
{
    using namespace bg::strategy::buffer;
    switch(type)
    {
        case buffered_segment : return 's';
        case buffered_join : return 'j';
        case buffered_round_end : return 'r';
        case buffered_flat_end : return 'f';
        case buffered_point : return 'p';
        case buffered_concave : return 'c';
        default : return '?';
    }
}

template <typename SvgMapper, typename Box>
class svg_visitor
{
public :
    svg_visitor(SvgMapper& mapper)
        : m_mapper(mapper)
        , m_zoom(false)
    {
        bg::assign_inverse(m_alternate_box);
    }

    void set_alternate_box(Box const& box)
    {
        m_alternate_box = box;
        m_zoom = true;
    }

    template <typename PieceCollection>
    inline void apply(PieceCollection const& collection, int phase)
    {
        // Comment next return if you want to see pieces, turns, etc.
        return;

        if(phase == 0)
        {
            map_pieces(collection.m_pieces, collection.offsetted_rings, true, true);
        }
        if (phase == 1)
        {
            map_turns(collection.m_turns, true, false);
        }
        if (phase == 2 && ! m_zoom)
        {
//        map_traversed_rings(collection.traversed_rings);
//        map_offsetted_rings(collection.offsetted_rings);
        }
    }

private :
    class si
    {
    private :
        bg::segment_identifier m_id;

    public :
        inline si(bg::segment_identifier const& id)
            : m_id(id)
        {}

        template <typename Char, typename Traits>
        inline friend std::basic_ostream<Char, Traits>& operator<<(
                std::basic_ostream<Char, Traits>& os,
                si const& s)
        {
            os << s.m_id.multi_index << "." << s.m_id.segment_index;
            return os;
        }
    };

    template <typename Turns>
    inline void map_turns(Turns const& turns, bool label_good_turns, bool label_wrong_turns)
    {
        namespace bgdb = boost::geometry::detail::buffer;
        typedef typename boost::range_value<Turns const>::type turn_type;
        typedef typename turn_type::point_type point_type;
        typedef typename turn_type::robust_point_type robust_point_type;

        std::map<robust_point_type, int, bg::less<robust_point_type> > offsets;

        for (typename boost::range_iterator<Turns const>::type it =
            boost::begin(turns); it != boost::end(turns); ++it)
        {
            if (m_zoom && bg::disjoint(it->point, m_alternate_box))
            {
                continue;
            }

            bool is_good = true;
            char color = 'g';
            std::string fill = "fill:rgb(0,255,0);";
            switch(it->location)
            {
                case bgdb::inside_buffer :
                    fill = "fill:rgb(255,0,0);";
                    color = 'r';
                    is_good = false;
                    break;
                case bgdb::location_discard :
                    fill = "fill:rgb(0,0,255);";
                    color = 'b';
                    is_good = false;
                    break;
                default:
                    ; // to avoid "enumeration value not handled" warning
            }
            if (it->blocked())
            {
                fill = "fill:rgb(128,128,128);";
                color = '-';
                is_good = false;
            }

            fill += "fill-opacity:0.7;";

            m_mapper.map(it->point, fill, 4);

            if ((label_good_turns && is_good) || (label_wrong_turns && ! is_good))
            {
                std::ostringstream out;
                out << it->turn_index;
                if (it->cluster_id >= 0)
                {
                   out << " ("  << it->cluster_id << ")";
                }
                out
                    << " " << it->operations[0].piece_index << "/" << it->operations[1].piece_index
                    << " " << si(it->operations[0].seg_id) << "/" << si(it->operations[1].seg_id)

    //              If you want to see travel information
                    << std::endl
                    << " nxt " << it->operations[0].enriched.get_next_turn_index()
                    << "/" << it->operations[1].enriched.get_next_turn_index()
                    //<< " frac " << it->operations[0].fraction

    //                If you want to see (robust)point coordinates (e.g. to find duplicates)
                    << std::endl << std::setprecision(16) << bg::wkt(it->point)
                    << std::endl  << bg::wkt(it->robust_point)

                    << std::endl;
                out << " " << bg::method_char(it->method)
                    << ":" << bg::operation_char(it->operations[0].operation)
                    << "/" << bg::operation_char(it->operations[1].operation);
                out << " "
                    << (it->count_on_offsetted > 0 ? "b" : "") // b: offsetted border
#if ! defined(BOOST_GEOMETRY_BUFFER_USE_SIDE_OF_INTERSECTION)
                    << (it->count_within_near_offsetted > 0 ? "n" : "")
#endif
                    << (it->count_within > 0 ? "w" : "")
                    << (it->count_on_helper > 0 ? "h" : "")
                    << (it->count_on_multi > 0 ? "m" : "")
                    ;

                offsets[it->get_robust_point()] += 10;
                int offset = offsets[it->get_robust_point()];

                m_mapper.text(it->point, out.str(), "fill:rgb(0,0,0);font-family='Arial';font-size:9px;", 5, offset);

                offsets[it->get_robust_point()] += 25;
            }
        }
    }

    template <typename Pieces, typename OffsettedRings>
    inline void map_pieces(Pieces const& pieces,
                OffsettedRings const& offsetted_rings,
                bool do_pieces, bool do_indices)
    {
        typedef typename boost::range_value<Pieces const>::type piece_type;
        typedef typename boost::range_value<OffsettedRings const>::type ring_type;

        for(typename boost::range_iterator<Pieces const>::type it = boost::begin(pieces);
            it != boost::end(pieces);
            ++it)
        {
            const piece_type& piece = *it;
            bg::segment_identifier seg_id = piece.first_seg_id;
            if (seg_id.segment_index < 0)
            {
                continue;
            }

            ring_type corner;


            ring_type const& ring = offsetted_rings[seg_id.multi_index];

            std::copy(boost::begin(ring) + seg_id.segment_index,
                    boost::begin(ring) + piece.last_segment_index,
                    std::back_inserter(corner));
            std::copy(boost::begin(piece.helper_points),
                    boost::end(piece.helper_points),
                    std::back_inserter(corner));

            if (corner.empty())
            {
                continue;
            }
            if (m_zoom && bg::disjoint(corner, m_alternate_box))
            {
                continue;
            }

            if (m_zoom && do_pieces)
            {
                try
                {
                    std::string style = "opacity:0.3;stroke:rgb(0,0,0);stroke-width:1;";
                    typedef typename bg::point_type<Box>::type point_type;
                    bg::model::multi_polygon<bg::model::polygon<point_type> > clipped;
                    bg::intersection(ring, m_alternate_box, clipped);
                    m_mapper.map(clipped,
                        piece.type == bg::strategy::buffer::buffered_segment
                        ? style + "fill:rgb(255,128,0);"
                        : style + "fill:rgb(255,0,0);");
                }
                catch (...)
                {
                    std::cerr << "Error for piece " << piece.index << std::endl;
                }
            }
            else if (do_pieces)
            {
                std::string style = "opacity:0.3;stroke:rgb(0,0,0);stroke-width:1;";
                m_mapper.map(corner,
                    piece.type == bg::strategy::buffer::buffered_segment
                    ? style + "fill:rgb(255,128,0);"
                    : style + "fill:rgb(255,0,0);");
            }

            if (do_indices)
            {
                // Label starting piece_index / segment_index
                typedef typename bg::point_type<ring_type>::type point_type;

                std::ostringstream out;
                out << piece.index
                    << (piece.is_flat_start ? " FS" : "")
                    << (piece.is_flat_end ? " FE" : "")
                    << " (" << piece_type_char(piece.type) << ") "
                    << piece.first_seg_id.segment_index
                    << ".." << piece.last_segment_index - 1;
                point_type label_point = bg::return_centroid<point_type>(corner);

                if ((piece.type == bg::strategy::buffer::buffered_concave
                     || piece.type == bg::strategy::buffer::buffered_flat_end)
                    && corner.size() >= 2u)
                {
                    bg::set<0>(label_point, (bg::get<0>(corner[0]) + bg::get<0>(corner[1])) / 2.0);
                    bg::set<1>(label_point, (bg::get<1>(corner[0]) + bg::get<1>(corner[1])) / 2.0);
                }
                m_mapper.text(label_point, out.str(), "fill:rgb(255,0,0);font-family='Arial';font-size:10px;", 5, 5);
            }
        }
    }

    template <typename TraversedRings>
    inline void map_traversed_rings(TraversedRings const& traversed_rings)
    {
        for(typename boost::range_iterator<TraversedRings const>::type it
                = boost::begin(traversed_rings); it != boost::end(traversed_rings); ++it)
        {
            m_mapper.map(*it, "opacity:0.4;fill:none;stroke:rgb(0,255,0);stroke-width:2");
        }
    }

    template <typename OffsettedRings>
    inline void map_offsetted_rings(OffsettedRings const& offsetted_rings)
    {
        for(typename boost::range_iterator<OffsettedRings const>::type it
                = boost::begin(offsetted_rings); it != boost::end(offsetted_rings); ++it)
        {
            if (it->discarded())
            {
                m_mapper.map(*it, "opacity:0.4;fill:none;stroke:rgb(255,0,0);stroke-width:2");
            }
            else
            {
                m_mapper.map(*it, "opacity:0.4;fill:none;stroke:rgb(0,0,255);stroke-width:2");
            }
        }
    }


    SvgMapper& m_mapper;
    Box m_alternate_box;
    bool m_zoom;

};

template <typename Point>
class buffer_svg_mapper
{
public :

    buffer_svg_mapper(std::string const& casename)
        : m_casename(casename)
        , m_zoom(false)
    {
        bg::assign_inverse(m_alternate_box);
    }

    template <typename Mapper, typename Visitor, typename Envelope>
    void prepare(Mapper& mapper, Visitor& visitor, Envelope const& envelope, double box_buffer_distance)
    {
#ifdef BOOST_GEOMETRY_BUFFER_TEST_SVG_USE_ALTERNATE_BOX
        // Create a zoomed-in view
        bg::model::box<Point> alternate_box;
        bg::read_wkt(BOOST_GEOMETRY_BUFFER_TEST_SVG_ALTERNATE_BOX, alternate_box);
        mapper.add(alternate_box);

        // Take care non-visible elements are skipped
        visitor.set_alternate_box(alternate_box);
        set_alternate_box(alternate_box);
#else
        bg::model::box<Point> box = envelope;
        bg::buffer(box, box, box_buffer_distance);
        mapper.add(box);
#endif

        boost::ignore_unused(visitor);
    }

    void set_alternate_box(bg::model::box<Point> const& box)
    {
        m_alternate_box = box;
        m_zoom = true;
    }

    template <typename Mapper, typename Geometry, typename GeometryBuffer>
    void map_input_output(Mapper& mapper, Geometry const& geometry,
            GeometryBuffer const& buffered, bool negative)
    {
        bool const areal = boost::is_same
            <
                typename bg::tag_cast
                    <
                        typename bg::tag<Geometry>::type,
                        bg::areal_tag
                    >::type, bg::areal_tag
            >::type::value;

        if (m_zoom)
        {
            map_io_zoomed(mapper, geometry, buffered, negative, areal);
        }
        else
        {
            map_io(mapper, geometry, buffered, negative, areal);
        }
    }

    template <typename Mapper, typename Geometry, typename Strategy, typename RescalePolicy>
    void map_self_ips(Mapper& mapper, Geometry const& geometry, Strategy const& strategy, RescalePolicy const& rescale_policy)
    {
        typedef bg::detail::overlay::turn_info
        <
            Point,
            typename bg::segment_ratio_type<Point, RescalePolicy>::type
        > turn_info;

        std::vector<turn_info> turns;

        bg::detail::self_get_turn_points::no_interrupt_policy policy;
        bg::self_turns
            <
                bg::detail::overlay::assign_null_policy
            >(geometry, strategy, rescale_policy, turns, policy);

        BOOST_FOREACH(turn_info const& turn, turns)
        {
            mapper.map(turn.point, "fill:rgb(255,128,0);stroke:rgb(0,0,100);stroke-width:1", 3);
        }
    }

private :

    template <typename Mapper, typename Geometry, typename GeometryBuffer>
    void map_io(Mapper& mapper, Geometry const& geometry,
            GeometryBuffer const& buffered, bool negative, bool areal)
    {
        // Map input geometry in green
        if (areal)
        {
            mapper.map(geometry, "opacity:0.5;fill:rgb(0,128,0);stroke:rgb(0,64,0);stroke-width:2");
        }
        else
        {
            // TODO: clip input points/linestring
            mapper.map(geometry, "opacity:0.5;stroke:rgb(0,128,0);stroke-width:10");
        }

        {
            // Map buffer in yellow (inflate) and with orange-dots (deflate)
            std::string style = negative
                ? "opacity:0.4;fill:rgb(255,255,192);stroke:rgb(255,128,0);stroke-width:3"
                : "opacity:0.4;fill:rgb(255,255,128);stroke:rgb(0,0,0);stroke-width:3";

            mapper.map(buffered, style);
        }
    }

    template <typename Mapper, typename Geometry, typename GeometryBuffer>
    void map_io_zoomed(Mapper& mapper, Geometry const& geometry,
            GeometryBuffer const& buffered, bool negative, bool areal)
    {
        // Map input geometry in green
        if (areal)
        {
            // Assuming input is areal
            GeometryBuffer clipped;
// TODO: the next line does NOT compile for multi-point, TODO: implement that line
//            bg::intersection(geometry, m_alternate_box, clipped);
            mapper.map(clipped, "opacity:0.5;fill:rgb(0,128,0);stroke:rgb(0,64,0);stroke-width:2");
        }
        else
        {
            // TODO: clip input (multi)point/linestring
            mapper.map(geometry, "opacity:0.5;stroke:rgb(0,128,0);stroke-width:10");
        }

        {
            // Map buffer in yellow (inflate) and with orange-dots (deflate)
            std::string style = negative
                ? "opacity:0.4;fill:rgb(255,255,192);stroke:rgb(255,128,0);stroke-width:3"
                : "opacity:0.4;fill:rgb(255,255,128);stroke:rgb(0,0,0);stroke-width:3";

            try
            {
                // Clip output multi-polygon with box
                GeometryBuffer clipped;
                bg::intersection(buffered, m_alternate_box, clipped);
                mapper.map(clipped, style);
            }
            catch (...)
            {
                std::cout << "Error for buffered output " << m_casename << std::endl;
            }
        }
    }

    bg::model::box<Point> m_alternate_box;
    bool m_zoom;
    std::string m_casename;
};


#endif
