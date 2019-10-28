// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_UTIL_NUMBER_TYPES_HPP
#define BOOST_GEOMETRY_TEST_UTIL_NUMBER_TYPES_HPP

#include <cmath>


// define a custom number type and its sqrt in own namespace
namespace number_types
{

template <typename T>
struct custom
{
    typedef custom<T> self;

    T m_value;

    custom() : m_value(0) {}
    explicit custom(T const& value) : m_value(value) {}

    bool operator<(self const& other) const
    {
        return m_value < other.m_value;
    }

    self operator-() const
    {
        return self(-m_value);
    }

    self operator-(self const& other) const
    {
        return self(m_value - other.m_value);
    }
};

template <typename T>
inline custom<T> sqrt(custom<T> const& c)
{
    return custom<T>(std::sqrt(c.m_value));
}

template <typename T>
inline custom<T> fabs(custom<T> const& c)
{
    return custom<T>(c.m_value < T(0) ? c.m_value : -c.m_value);
}

} // namespace number_types






// define a custom number type with sqrt in global namespace
namespace number_types
{

template <typename T>
struct custom_with_global_sqrt
{
    typedef custom_with_global_sqrt<T> self;

    T m_value;

    custom_with_global_sqrt() : m_value(0) {}
    explicit custom_with_global_sqrt(T const& value) : m_value(value) {}

    bool operator<(self const& other) const
    {
        return m_value < other.m_value;
    }

    self operator-() const
    {
        return self(-m_value);
    }

    self operator-(self const& other) const
    {
        return self(m_value - other.m_value);
    }
};

} // namespace number_types

template <typename T>
inline number_types::custom_with_global_sqrt<T>
sqrt(number_types::custom_with_global_sqrt<T> const& c)
{
    return number_types::custom_with_global_sqrt<T>(std::sqrt(c.m_value));
}

template <typename T>
inline number_types::custom_with_global_sqrt<T>
fabs(number_types::custom_with_global_sqrt<T> const& c)
{
    return number_types::custom_with_global_sqrt<T>
                (c.m_value < T(0) ? c.m_value : -c.m_value);
}






// define a custom number type and its sqrt in global namespace
template <typename T>
struct custom_global
{
    typedef custom_global<T> self;

    T m_value;

    custom_global() : m_value(0) {}
    explicit custom_global(T const& value) : m_value(value) {}

    bool operator<(self const& other) const
    {
        return m_value < other.m_value;
    }

    self operator-() const
    {
        return self(-m_value);
    }

    self operator-(self const& other) const
    {
        return self(m_value - other.m_value);
    }
};

template <typename T>
inline custom_global<T> sqrt(custom_global<T> const& c)
{
    return custom_global<T>(std::sqrt(c.m_value));
}

template <typename T>
inline custom_global<T> fabs(custom_global<T> const& c)
{
    return custom_global<T>(c.m_value < T(0) ? c.m_value : -c.m_value);
}



// custom number type without functions definition
template <typename T>
struct custom_raw
{
    typedef custom_raw<T> self;

    T m_value;

    custom_raw() : m_value(0) {}
    explicit custom_raw(T const& value) : m_value(value) {}

    bool operator<(self const& other) const
    {
        return m_value < other.m_value;
    }

    self operator-() const
    {
        return self(-m_value);
    }

    self operator-(self const& other) const
    {
        return self(m_value - other.m_value);
    }
};

#endif // BOOST_GEOMETRY_TEST_UTIL_NUMBER_TYPES_HPP
