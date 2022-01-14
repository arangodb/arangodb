// Boost.Geometry Index
// Additional tests

// Copyright (c) 2011-2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <vector>
#include <algorithm>

#include <boost/chrono.hpp>
#include <boost/foreach.hpp>
#include <boost/random.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/index/detail/rtree/utilities/are_boxes_ok.hpp>
#include <boost/geometry/index/detail/rtree/utilities/are_counts_ok.hpp>
#include <boost/geometry/index/detail/rtree/utilities/are_levels_ok.hpp>

namespace bg = boost::geometry;
namespace bgi = bg::index;

typedef bg::model::point<double, 2, bg::cs::cartesian> P;
typedef bg::model::box<P> B;
typedef bg::model::segment<P> S;
typedef P V;
//typedef B V;
//typedef S V;

template <typename V>
struct generate_value {};

template <>
struct generate_value<B>
{
    static inline B apply(float x, float y) { return B(P(x - 0.5f, y - 0.5f), P(x + 0.5f, y + 0.5f)); }
};

template <>
struct generate_value<S>
{
    static inline S apply(float x, float y) { return S(P(x - 0.5f, y - 0.5f), P(x + 0.5f, y + 0.5f)); }
};

template <>
struct generate_value<P>
{
    static inline P apply(float x, float y) { return P(x, y); }
};

template <typename RT>
void test_queries(RT const& t, std::vector< std::pair<float, float> > const& coords, size_t queries_count)
{
    typedef boost::chrono::thread_clock clock_t;
    typedef boost::chrono::duration<float> dur_t;

    std::vector<V> result;
    result.reserve(100);

    clock_t::time_point start = clock_t::now();
    size_t temp = 0;
    for (size_t i = 0 ; i < queries_count ; ++i )
    {
        float x = coords[i].first;
        float y = coords[i].second;
        result.clear();
        t.query(bgi::intersects(B(P(x - 10, y - 10), P(x + 10, y + 10))), std::back_inserter(result));
        temp += result.size();
    }
    dur_t time = clock_t::now() - start;
    std::cout << time.count() << " " << temp << '\n';
}

//#define BOOST_GEOMETRY_INDEX_BENCHMARK_DEBUG

int main()
{
    //typedef bgi::rtree<V, bgi::linear<4, 2> > RT;
    //typedef bgi::rtree<V, bgi::linear<16, 4> > RT;
    //typedef bgi::rtree<V, bgi::quadratic<4, 2> > RT;
    typedef bgi::rtree<V, bgi::rstar<8, 2> > RT;

    typedef boost::chrono::thread_clock clock_t;
    typedef boost::chrono::duration<float> dur_t;

#ifndef BOOST_GEOMETRY_INDEX_BENCHMARK_DEBUG
    size_t values_count = 1000000;
    size_t queries_count = 100000;
    size_t nearest_queries_count = 20000;
    unsigned neighbours_count = 10;
    size_t max_range_inserts = 10;
#else
    size_t values_count = 10000;
    size_t queries_count = 1000;
    size_t nearest_queries_count = 100;
    unsigned neighbours_count = 10;
    size_t max_range_inserts = 10;
#endif

    float max_val = static_cast<float>(values_count / 2);
    std::vector< std::pair<float, float> > coords;
    std::vector<V> values;

    //randomize values
    {
        boost::mt19937 rng;
        //rng.seed(static_cast<unsigned int>(std::time(0)));
        boost::uniform_real<float> range(-max_val, max_val);
        boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > rnd(rng, range);

        coords.reserve(values_count);

        std::cout << "randomizing data\n";
        for ( size_t i = 0 ; i < values_count ; ++i )
        {
            float x = rnd();
            float y = rnd();
            coords.push_back(std::make_pair(x, y));
            values.push_back(generate_value<V>::apply(x, y));
        }
        std::cout << "randomized\n";
    }

    for (;;)
    {
        // packing test
        {
            clock_t::time_point start = clock_t::now();

            RT t(values.begin(), values.end());

            BOOST_ASSERT(bgi::detail::rtree::utilities::are_boxes_ok(t));
            BOOST_ASSERT(bgi::detail::rtree::utilities::are_counts_ok(t));
            BOOST_ASSERT(bgi::detail::rtree::utilities::are_levels_ok(t));

            dur_t time = clock_t::now() - start;
            std::cout << "pack(" << values_count << ") - " << time.count() << ", ";

            test_queries(t, coords, queries_count);
        }

        {
            size_t n_per_max = values_count / max_range_inserts;

            for ( size_t j = 0 ; j < max_range_inserts ; ++j )
            {
                clock_t::time_point start = clock_t::now();

                RT t;

                // perform j range-inserts
                for ( size_t i = 0 ; i < j ; ++i )
                {
                    t.insert(values.begin() + n_per_max * i,
                             values.begin() + (std::min)(n_per_max * (i + 1), values_count));
                }

                if ( !t.empty() )
                {
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_boxes_ok(t));
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_counts_ok(t));
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_levels_ok(t));
                }

                // perform n-n/max_inserts*j inserts
                size_t inserted_count = (std::min)(n_per_max*j, values_count);
                for ( size_t i = inserted_count ; i < values_count ; ++i )
                {
                    t.insert(values[i]);
                }

                if ( !t.empty() )
                {
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_boxes_ok(t));
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_counts_ok(t));
                    BOOST_ASSERT(bgi::detail::rtree::utilities::are_levels_ok(t));
                }

                dur_t time = clock_t::now() - start;
                std::cout << j << "*insert(N/" << max_range_inserts << ")+insert(" << (values_count - inserted_count) << ") - " << time.count() << ", ";

                test_queries(t, coords, queries_count);
            }
        }

        std::cout << "------------------------------------------------\n";
    }

    return 0;
}
