// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_BUFFER_TEST_SVG_USE_ALTERNATE_BOX_FOR_INPUT
#define BOOST_GEOMETRY_BUFFER_TEST_SVG_ALTERNATE_BOX "BOX(179 4, 180 5)"


#include <test_buffer.hpp>

#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>



const int point_count = 90; // points for a full circle

// Function to let buffer-distance depend on alpha, e.g.:
inline double corrected_distance(double distance, double alpha)
{
    return distance * 1.0 + 0.2 * sin(alpha * 6.0);
}

class buffer_point_strategy_sample
{
public :

    template
    <
        typename Point,
        typename OutputRange,
        typename DistanceStrategy
    >
    void apply(Point const& point,
            DistanceStrategy const& distance_strategy,
            OutputRange& output_range) const
    {
        double const distance = distance_strategy.apply(point, point,
                        bg::strategy::buffer::buffer_side_left);

        double const angle_increment = 2.0 * M_PI / double(point_count);

        double alpha = 0;
        for (std::size_t i = 0; i <= point_count; i++, alpha -= angle_increment)
        {
            double const cd = corrected_distance(distance, alpha);

            typename boost::range_value<OutputRange>::type output_point;
            bg::set<0>(output_point, bg::get<0>(point) + cd * cos(alpha));
            bg::set<1>(output_point, bg::get<1>(point) + cd * sin(alpha));
            output_range.push_back(output_point);
        }
    }
};

class buffer_join_strategy_sample
{
private :
    template
    <
        typename Point,
        typename DistanceType,
        typename RangeOut
    >
    inline void generate_points(Point const& vertex,
                Point const& perp1, Point const& perp2,
                DistanceType const& buffer_distance,
                RangeOut& range_out) const
    {
        double dx1 = bg::get<0>(perp1) - bg::get<0>(vertex);
        double dy1 = bg::get<1>(perp1) - bg::get<1>(vertex);
        double dx2 = bg::get<0>(perp2) - bg::get<0>(vertex);
        double dy2 = bg::get<1>(perp2) - bg::get<1>(vertex);

        // Assuming the corner is convex, angle2 < angle1
        double const angle1 = atan2(dy1, dx1);
        double angle2 = atan2(dy2, dx2);

        while (angle2 > angle1)
        {
            angle2 -= 2 * M_PI;
        }

        double const angle_increment = 2.0 * M_PI / double(point_count);
        double alpha = angle1 - angle_increment;

        for (int i = 0; alpha >= angle2 && i < point_count; i++, alpha -= angle_increment)
        {
            double cd = corrected_distance(buffer_distance, alpha);

            Point p;
            bg::set<0>(p, bg::get<0>(vertex) + cd * cos(alpha));
            bg::set<1>(p, bg::get<1>(vertex) + cd * sin(alpha));
            range_out.push_back(p);
        }
    }

public :

    template <typename Point, typename DistanceType, typename RangeOut>
    inline bool apply(Point const& ip, Point const& vertex,
                Point const& perp1, Point const& perp2,
                DistanceType const& buffer_distance,
                RangeOut& range_out) const
    {
        generate_points(vertex, perp1, perp2, buffer_distance, range_out);
        return true;
    }

    template <typename NumericType>
    static inline NumericType max_distance(NumericType const& distance)
    {
        return distance;
    }

};

class buffer_side_sample
{
public :
    template
    <
        typename Point,
        typename OutputRange,
        typename DistanceStrategy
    >
    static inline void apply(
                Point const& input_p1, Point const& input_p2,
                bg::strategy::buffer::buffer_side_selector side,
                DistanceStrategy const& distance,
                OutputRange& output_range)
    {
        // Generate a block along (left or right of) the segment

        double const dx = bg::get<0>(input_p2) - bg::get<0>(input_p1);
        double const dy = bg::get<1>(input_p2) - bg::get<1>(input_p1);

        // For normalization [0,1] (=dot product d.d, sqrt)
        double const length = bg::math::sqrt(dx * dx + dy * dy);

        if (bg::math::equals(length, 0))
        {
            return;
        }

        // Generate the normalized perpendicular p, to the left (ccw)
        double const px = -dy / length;
        double const py = dx / length;

        // Both vectors perpendicular to input p1 and input p2 have same angle
        double const alpha = atan2(py, px);

        double const d = distance.apply(input_p1, input_p2, side);

        double const cd = corrected_distance(d, alpha);

        output_range.resize(2);

        bg::set<0>(output_range.front(), bg::get<0>(input_p1) + px * cd);
        bg::set<1>(output_range.front(), bg::get<1>(input_p1) + py * cd);
        bg::set<0>(output_range.back(), bg::get<0>(input_p2) + px * cd);
        bg::set<1>(output_range.back(), bg::get<1>(input_p2) + py * cd);
    }
};

#ifdef TEST_WITH_SVG
template <typename Geometry1, typename Geometry2>
void create_svg(std::string const& filename, Geometry1 const& original, Geometry2 const& buffer, std::string const& color)
{
    typedef typename bg::point_type<Geometry1>::type point_type;
    std::ofstream svg(filename.c_str());

    bg::svg_mapper<point_type> mapper(svg, 800, 800);
    mapper.add(buffer);

    mapper.map(original, "fill-opacity:0.3;fill:rgb(255,0,0);stroke:rgb(0,0,0);stroke-width:1");

    std::string style = "fill-opacity:0.3;fill:";
    style += color;
    style += ";stroke:rgb(0,0,0);stroke-width:1";
    mapper.map(buffer, style);
}
#endif


void test_many_rings(int imn, int jmx, int count,
    double expected_area_exterior,
    double expected_area_interior)
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point;
    typedef bg::model::polygon<point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    // Predefined strategies
    bg::strategy::buffer::distance_symmetric<double> distance_strategy(1.3);
    bg::strategy::buffer::end_flat end_strategy; // not effectively used

    // Own strategies
    buffer_join_strategy_sample join_strategy;
    buffer_point_strategy_sample point_strategy;
    buffer_side_sample side_strategy;

    // Declare output

    bg::model::multi_point<point> mp;

    // Use a bit of random disturbance in the otherwise too regular grid
    typedef boost::minstd_rand base_generator_type;
    base_generator_type generator(12345);
    boost::uniform_real<> random_range(0.0, 0.5);
    boost::variate_generator
    <
        base_generator_type&,
        boost::uniform_real<>
    > random(generator, random_range);

    for (int i = 0; i < count; i++)
    {
        for (int j = 0; j < count; j++)
        {
            double x = i * 3.0 + random();
            double y = j * 3.0 + random();
            //if (i > 30 && j < 30)
            if (i > imn && j < jmx)
            {
                point p(x, y);
                mp.push_back(p);
            }
        }
    }

    multi_polygon_type many_rings;
    // Create the buffer of a multi-point
    bg::buffer(mp, many_rings,
                distance_strategy, side_strategy,
                join_strategy, end_strategy, point_strategy);

    bg::model::box<point> envelope;
    bg::envelope(many_rings, envelope);
    bg::buffer(envelope, envelope, 1.0);

    multi_polygon_type many_interiors;
    bg::difference(envelope, many_rings, many_interiors);

#ifdef TEST_WITH_SVG
    create_svg("/tmp/many_interiors.svg", mp, many_interiors, "rgb(51,51,153)");
    create_svg("/tmp/buffer.svg", mp, many_rings, "rgb(51,51,153)");
#endif

    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::end_flat end_flat;

    {
        std::ostringstream out;
        out << "many_rings_" << count;
        out << "_" << imn << "_" << jmx;
        std::ostringstream wkt;
        wkt << std::setprecision(12) << bg::wkt(many_rings);

        boost::timer t;
        test_one<multi_polygon_type, polygon_type>(out.str(), wkt.str(), join_round, end_flat, expected_area_exterior, 0.3);
        std::cout << "Exterior " << count << " " << t.elapsed() << std::endl;
    }

    return;
    {
        std::ostringstream out;
        out << "many_interiors_" << count;
        std::ostringstream wkt;
        wkt << std::setprecision(12) << bg::wkt(many_interiors);

        boost::timer t;
        test_one<multi_polygon_type, polygon_type>(out.str(), wkt.str(), join_round, end_flat, expected_area_interior, 0.3);
        std::cout << "Interior " << count << " " << t.elapsed() << std::endl;
    }
}

int test_main(int, char* [])
{
//    test_many_rings(10, 795.70334, 806.7609);
//    test_many_rings(30, 7136.7098, 6174.4496);
      test_many_rings(30, 30, 70, 38764.2721, 31910.3280);
//    for (int i = 30; i < 60; i++)
//    {
//        for (int j = 5; j <= 30; j++)
//        {
//            test_many_rings(i, j, 70, 38764.2721, 31910.3280);
//        }
//    }
    return 0;
}
