// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2020 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef GEOMETRY_TEST_COUNT_SET_HPP
#define GEOMETRY_TEST_COUNT_SET_HPP

#include <set>
#include <ostream>

// Structure to manage expectations: sometimes the output might have one or
// two rings, both are fine.
struct count_set
{
    count_set()
    {
    }

    count_set(int value)
    {
        if (value >= 0)
        {
            m_values.insert(static_cast<std::size_t>(value));
        }
    }

    count_set(std::size_t value1, std::size_t value2)
    {
        m_values.insert(value1);
        m_values.insert(value2);
    }

    count_set(std::size_t value1, std::size_t value2, std::size_t value3)
    {
        m_values.insert(value1);
        m_values.insert(value2);
        m_values.insert(value3);
    }

    bool empty() const { return m_values.empty(); }

    bool has(std::size_t value) const
    {
        return m_values.count(value) > 0;
    }

    friend std::ostream &operator<<(std::ostream &os, const count_set& s)
    {
       os << "{";
       for (std::size_t const& value : s.m_values)
       {
           os << " " << value;
       }
       os << "}";
       return os;
    }

    count_set operator+(const count_set& a) const
    {
        count_set result;
        result.m_values = combine(this->m_values, a.m_values);
        return result;
    }

private :
    typedef std::set<std::size_t> set_type;
    set_type m_values;

    set_type combine(const set_type& a, const set_type& b) const
    {
        set_type result;
        if (a.size() == 1 && b.size() == 1)
        {
            // The common scenario, both have one value
            result.insert(*a.begin() + *b.begin());
        }
        else if (a.size() > 1 && b.size() == 1)
        {
            // One of them is optional, add the second
            for (std::size_t const& value : a)
            {
                result.insert(value + *b.begin());
            }
        }
        else if (b.size() > 1 && a.size() == 1)
        {
            return combine(b, a);
        }
        // Else either is empty, or both have values and should be specified
        return result;
    }
};

inline count_set ignore_count()
{
    return count_set();
}

inline count_set optional()
{
    return count_set(0, 1);
}

#endif // GEOMETRY_TEST_COUNT_SET_HPP
