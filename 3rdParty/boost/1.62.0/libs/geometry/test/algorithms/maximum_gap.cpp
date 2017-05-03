// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_maximum_gap
#endif

#include <boost/test/included/unit_test.hpp>

#include <cstddef>

#include <iostream>
#include <sstream>
#include <vector>

#include <boost/geometry/algorithms/detail/max_interval_gap.hpp>

namespace bg = boost::geometry;

class uint_interval
{
public:
    typedef unsigned value_type;
    typedef int difference_type;

    uint_interval(unsigned left, unsigned right)
        : m_left(left)
        , m_right(right)
    {}

    template <std::size_t Index>
    value_type get() const
    {
        return (Index == 0) ? m_left : m_right;
    }

    difference_type length() const
    {
        return static_cast<int>(m_right) - static_cast<int>(m_left);
    }

private:
    unsigned m_left, m_right;
};

struct uint_intervals
    : public std::vector<uint_interval>
{
    uint_intervals()
    {}

    uint_intervals(unsigned left, unsigned right)
    {
        this->push_back(uint_interval(left, right));
    }

    uint_intervals & operator()(unsigned left, unsigned right)
    {
        this->push_back(uint_interval(left, right));
        return *this;
    }
};

std::ostream& operator<<(std::ostream& os, uint_interval const& interval)
{
    os << "[" << interval.get<0>() << ", " << interval.get<1>() << "]";
    return os;
}


template <typename RangeOfIntervals>
inline void test_one(std::string const& case_id,
                     RangeOfIntervals const& intervals,
                     typename boost::range_value
                         <
                             RangeOfIntervals
                         >::type::difference_type expected_gap)
{
    typedef typename boost::range_value<RangeOfIntervals>::type interval_type;
    typedef typename interval_type::difference_type gap_type;

    gap_type gap = bg::maximum_gap(intervals);

    std::ostringstream stream;
    for (typename boost::range_const_iterator<RangeOfIntervals>::type
            it = boost::const_begin(intervals);
            it != boost::const_end(intervals);
            ++it)
    {
        stream << " " << *it;
    }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "intervals:" << stream.str() << std::endl;
    std::cout << "gap found? " << ((gap > 0) ? "yes" : "no") << std::endl;
    std::cout << "max gap length: " << gap << std::endl;
    std::cout << std::endl << std::endl;
#endif

    BOOST_CHECK_MESSAGE(gap == expected_gap,
                        case_id << "; intervals: "
                        << stream.str()
                        << "; expected: " << expected_gap
                        << ", detected: " << gap);
}

BOOST_AUTO_TEST_CASE( test_maximum_gap )
{
    uint_intervals intervals;

    intervals = uint_intervals(3,4)(1,10)(5,11)(20,35)(12,14)(36,40)(39,41)(35,36)(37,37)(50,50)(50,51);
    test_one("case_01", intervals, 9);

    intervals = uint_intervals(3,4)(1,10)(5,11)(20,35)(52,60)(12,14)(36,40)(39,41)(35,36)(37,37)(55,56);
    test_one("case_02", intervals, 11);

    intervals = uint_intervals(3,4);
    test_one("case_03", intervals, 0);

    intervals = uint_intervals(3,4)(15,15);
    test_one("case_04", intervals, 11);

    intervals = uint_intervals(3,14)(5,5)(5,6);
    test_one("case_05", intervals, 0);

    intervals = uint_intervals(3,10)(15,15)(15,18)(15,16);
    test_one("case_06", intervals, 5);

    intervals = uint_intervals(38,41)(3,10)(15,15)(15,18)(15,16)(20,30)(22,30)(23,30);
    test_one("case_07", intervals, 8);
}
