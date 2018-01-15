// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2012-2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/assign/list_of.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/algorithms/detail/get_left_turns.hpp>

#if defined(TEST_WITH_SVG)
# include <boost/geometry/io/svg/svg_mapper.hpp>
#endif


NOTE: this unit test is out of date.
get_left_turns is used by buffer and might be used in the future by solving self-tangencies in overlays.
it is currently being changed by buffer.

namespace bglt = boost::geometry::detail::left_turns;

#if defined(TEST_WITH_SVG)
template <typename Point>
inline Point further_than(Point const& p, Point const& origin, int mul, int div)
{
    typedef Point vector_type;

    vector_type v = p;
    bg::subtract_point(v, origin);

    bg::divide_value(v, div);
    bg::multiply_value(v, mul);
    Point result = origin;
    bg::add_point(result, v);
    return result;
}

inline std::string get_color(int index)
{
    switch (index)
    {
        case 0 : return "rgb(0,192,0)";
        case 1 : return "rgb(0,0,255)";
        case 2 : return "rgb(255,0,0)";
        case 3 : return "rgb(255,255,0)";
    }
    return "rgb(128,128,128)";
}
#endif


template <typename Point>
void test_one(std::string const& caseid,
                Point const& p,
                std::vector<bglt::turn_angle_info<Point> > const& angles,
                std::string const& expected_sorted_indices,
                std::string const& expected_left_indices)
{
    typedef Point vector_type;

    std::vector<bglt::angle_info<Point> > sorted;
    for (typename std::vector<bglt::turn_angle_info<Point> >::const_iterator it =
        angles.begin(); it != angles.end(); ++it)
    {
        for (int i = 0; i < 2; i++)
        {
            bglt::angle_info<Point> info(it->seg_id, i == 0, it->points[i]);
            sorted.push_back(info);
        }
    }

    // Sort on angle
    std::sort(sorted.begin(), sorted.end(), bglt::angle_less<Point>(p));

    // Block all turns on the right side of any turn
    bglt::block_turns_on_right_sides(angles, sorted);

    // Check the sorting
    {
        std::ostringstream out;
        out << std::boolalpha;
        for (typename std::vector<bglt::angle_info<Point> >::const_iterator it =
            sorted.begin(); it != sorted.end(); ++it)
        {
            out << " " << it->seg_id.segment_index
                << "-" << it->incoming;
        }
        std::string detected = boost::trim_copy(out.str());
        BOOST_CHECK_EQUAL(expected_sorted_indices, detected);
    }


    // Check outgoing lines
    std::vector<bglt::left_turn> seg_ids;
    bglt::get_left_turns(sorted, p, seg_ids);
    {
        std::ostringstream out;
        out << std::boolalpha;
        for (std::vector<bglt::left_turn>::const_iterator it =
            seg_ids.begin(); it != seg_ids.end(); ++it)
        {
            out
                << " " << it->from.segment_index
                << "->" << it->to.segment_index
                ;
        }
        std::string detected = boost::trim_copy(out.str());
        BOOST_CHECK_EQUAL(expected_left_indices, detected);
    }

#if defined(TEST_WITH_SVG)
    {
        std::ostringstream filename;
        filename << "get_left_turns_" << caseid
            << "_" << string_from_type<typename bg::coordinate_type<Point>::type>::name()
            << ".svg";

        std::ofstream svg(filename.str().c_str());

        bg::svg_mapper<Point> mapper(svg, 500, 500);
        mapper.add(p);
        for (typename std::vector<bglt::turn_angle_info<Point> >::const_iterator it =
            angles.begin(); it != angles.end(); ++it)
        {
            // Add a point further then it->to_point, just for the mapping
            for (int i = 0; i < 2; i++)
            {
                mapper.add(further_than(it->points[i], p, 12, 10));
            }
        }

        int color_index = 0;
        typedef bg::model::referring_segment<Point const> segment_type;
        for (typename std::vector<bglt::turn_angle_info<Point> >::const_iterator it =
            angles.begin(); it != angles.end(); ++it, color_index++)
        {
            for (int i = 0; i < 2; i++)
            {
                std::string style = "opacity:0.5;stroke-width:1;stroke:rgb(0,0,0);fill:" + get_color(color_index);

                bool const incoming = i == 0;
                Point const& pf = incoming ? it->points[i] : p;
                Point const& pt = incoming ? p : it->points[i];
                vector_type v = pt;
                bg::subtract_point(v, pf);

                bg::divide_value(v, 10.0);

                // Generate perpendicular vector to right-side
                vector_type perpendicular;
                bg::set<0>(perpendicular, bg::get<1>(v));
                bg::set<1>(perpendicular, -bg::get<0>(v));

                bg::model::ring<Point> ring;
                ring.push_back(pf);
                ring.push_back(pt);

                // Extra point at 9/10
                {
                    Point pe = pt;
                    bg::add_point(pe, perpendicular);
                    ring.push_back(pe);
                }
                {
                    Point pe = pf;
                    bg::add_point(pe, perpendicular);
                    ring.push_back(pe);
                }
                ring.push_back(pf);

                mapper.map(ring, style);

                segment_type s(pf, pt);
                mapper.map(s, "opacity:0.9;stroke-width:4;stroke:rgb(0,0,0);");
            }
        }

        // Output angles for left-turns
        for (std::vector<bglt::left_turn>::const_iterator ltit =
            seg_ids.begin(); ltit != seg_ids.end(); ++ltit)
        {
            for (typename std::vector<bglt::angle_info<Point> >::const_iterator sit =
                sorted.begin(); sit != sorted.end(); ++sit, color_index++)
            {
                Point pf, pt;
                int factor = 0;
                if (sit->seg_id == ltit->from && sit->incoming)
                {
                    pf = sit->point;
                    pt = p;
                    factor = -1; // left side
                }
                else if (sit->seg_id == ltit->to && ! sit->incoming)
                {
                    pf = p;
                    pt = sit->point;
                    factor = -1; // left side
                }
                if (factor != 0)
                {
                    vector_type v = pt;
                    bg::subtract_point(v, pf);
                    bg::divide_value(v, 10.0);

                    // Generate perpendicular vector to right-side
                    vector_type perpendicular;
                    bg::set<0>(perpendicular, factor * bg::get<1>(v));
                    bg::set<1>(perpendicular, -factor * bg::get<0>(v));

                    bg::add_point(pf, v);
                    bg::subtract_point(pt, v);

                    bg::add_point(pf, perpendicular);
                    bg::add_point(pt, perpendicular);

                    segment_type s(pf, pt);
                    mapper.map(s, "opacity:0.9;stroke-width:4;stroke:rgb(255,0,0);");
                }
            }
        }

        // Output texts with info about sorted/blocked
        int index = 0;
        for (typename std::vector<bglt::angle_info<Point> >::const_iterator it =
            sorted.begin(); it != sorted.end(); ++it, ++index)
        {
            std::ostringstream out;
            out << std::boolalpha;
            out << " seg:" << it->seg_id.segment_index
                << " " << (it->incoming ? "in" : "out")
                << " idx:" << index
                << (it->blocked ? " blocked" : "")
                ;
            mapper.text(further_than(it->point, p, 11, 10), out.str(), "fill:rgb(0,0,0);font-family='Verdana'");
        }

        mapper.map(p, "fill:rgb(255,0,0)");

    }

#endif

}


template <typename P>
void test_all()
{
    using bglt::turn_angle_info;

    test_one<P>("cross",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(100, 100), bg::make<P>(0, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 0), bg::make<P>(0, 100)))
            , "1-true 2-true 1-false 2-false"
            , "2->1"
        );

    test_one<P>("occupied",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(100, 100), bg::make<P>(0, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 0), bg::make<P>(0, 100)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 3), bg::make<P>(0, 30), bg::make<P>(100, 70)))
            , "1-true 3-false 2-true 1-false 3-true 2-false"
            , ""
        );

    test_one<P>("uu",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(0, 0), bg::make<P>(100, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 100), bg::make<P>(0, 100)))
            , "2-true 1-false 1-true 2-false"
            , "2->1 1->2"
        );

    test_one<P>("uu2",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(0, 0), bg::make<P>(100, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 100), bg::make<P>(0, 100)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 3), bg::make<P>(0, 50), bg::make<P>(100, 50)))
            , "2-true 3-false 1-false 1-true 3-true 2-false"
            , "2->3 3->2"
        );

    test_one<P>("uu3",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(0, 0), bg::make<P>(100, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 100), bg::make<P>(0, 100)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 3), bg::make<P>(50, 0), bg::make<P>(50, 100)))
            , "3-false 2-true 1-false 3-true 1-true 2-false"
            , "1->2"
        );

    test_one<P>("longer",
        bg::make<P>(50, 50),   // ip
        boost::assign::list_of
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 1), bg::make<P>(100, 100), bg::make<P>(0, 0)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 2), bg::make<P>(100, 0), bg::make<P>(0, 100)))
            (turn_angle_info<P>(bg::segment_identifier(0, -1, -1, 3), bg::make<P>(90, 10), bg::make<P>(10, 10)))
            , "1-true 2-true 3-true 1-false 3-false 2-false"
            , "3->1"
        );
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<int> >();

    return 0;
}
