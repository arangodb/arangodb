// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2020 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef GEOMETRY_TEST_EXPECTATION_LIMITS_HPP
#define GEOMETRY_TEST_EXPECTATION_LIMITS_HPP

#include <boost/geometry/util/math.hpp>

#include <ostream>

// Structure to manage expectations: there might be small variations in area, for different
// types or options, which are all acceptable. With tolerance this is inconvenient.
// The values are stored as doubles, but the member functions accept any type,
// also for example Boost.MultiPrecision types
struct expectation_limits
{
    expectation_limits(double expectation)
        : m_lower_limit(expectation)
        , m_upper_limit(expectation)
    {
    }

    expectation_limits(double lower_limit, double upper_limit)
        : m_lower_limit(lower_limit)
        , m_upper_limit(upper_limit)
    {
    }

    double get() const { return m_lower_limit; }

    bool is_zero() const { return m_lower_limit < 1.0e-8; }

    bool has_two_limits() const { return m_lower_limit < m_upper_limit; }

    template<typename T>
    bool contains_logarithmic(const T& value, double tolerance) const
    {
        using std::abs;
        using std::log;
        return abs(log(value) - std::log(m_lower_limit)) < tolerance;
    }

    template<typename T>
    bool contains(const T& value, double percentage, bool logarithmic = false) const
    {
        if (m_upper_limit < 1.0e-8)
        {
            return value < 1.0e-8;
        }
        if (logarithmic)
        {
            return contains_logarithmic(value, percentage);
        }

        // Note the > and <= and percentages, this is to make it exactly equivalent to
        // BOOST_CHECK_CLOSE(m_lower_limit, value, percentage) (if lower == upper)
        // But for two limits and optional slivers, >= is needed (for 0.00)
        double const fraction = percentage / 100.0;
        double const lower_limit = m_lower_limit * (1.0 - fraction);
        double const upper_limit = m_upper_limit * (1.0 + fraction);
        return has_two_limits()
                ? value >= lower_limit && value <= upper_limit
                : value > lower_limit && value <= upper_limit;
    }

    expectation_limits operator+(const expectation_limits& a) const
    {
        return this->has_two_limits() || a.has_two_limits()
                ? expectation_limits(this->m_lower_limit + a.m_lower_limit,
                                     this->m_upper_limit + a.m_upper_limit)
                : expectation_limits(this->m_lower_limit + a.m_lower_limit);
    }

    friend std::ostream &operator<<(std::ostream &os, const expectation_limits& lim)
    {
        if (lim.has_two_limits())
        {
            os << "[" << lim.m_lower_limit << " .. " << lim.m_upper_limit << "]";
        }
        else
        {
            os << lim.m_lower_limit;
        }
        return os;
    }

private :
    double const m_lower_limit;
    double const m_upper_limit;
};

inline expectation_limits optional_sliver(double upper_limit = 1.0e-4)
{
    return expectation_limits(0, upper_limit);
}

#endif // GEOMETRY_TEST_EXPECTATION_LIMITS_HPP
