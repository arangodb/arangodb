//  Copyright (c) 2018 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TESTS_VALUE_WITH_EPSILON_H_INCLUDED_
#define BOOST_ATOMIC_TESTS_VALUE_WITH_EPSILON_H_INCLUDED_

#include <limits>
#include <iosfwd>

template< typename T >
class value_with_epsilon
{
private:
    T m_value;
    T m_epsilon;

public:
    value_with_epsilon(T value, T epsilon) : m_value(value), m_epsilon(epsilon) {}

    T value() const
    {
        return m_value;
    }

    T epsilon() const
    {
        return m_epsilon;
    }

    bool equal(T value) const
    {
        return value >= (m_value - m_epsilon) && value <= (m_value + m_epsilon);
    }

    friend bool operator== (T left, value_with_epsilon< T > const& right)
    {
        return right.equal(left);
    }
    friend bool operator== (value_with_epsilon< T > const& left, T right)
    {
        return left.equal(right);
    }

    friend bool operator!= (T left, value_with_epsilon< T > const& right)
    {
        return !right.equal(left);
    }
    friend bool operator!= (value_with_epsilon< T > const& left, T right)
    {
        return !left.equal(right);
    }
};

template< typename Char, typename Traits, typename T >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, value_with_epsilon< T > const& val)
{
    // Note: libstdc++ does not provide output operators for __float128. There may also be no operators for long double.
    //       We don't use such floating point values in our tests where the cast would matter.
    strm << static_cast< double >(val.value()) << " (+/-" << static_cast< double >(val.epsilon()) << ")";
    return strm;
}

template< typename T, typename U >
inline value_with_epsilon< T > approx(T value, U epsilon)
{
    return value_with_epsilon< T >(value, static_cast< T >(epsilon));
}

template< typename T >
inline value_with_epsilon< T > approx(T value)
{
    return value_with_epsilon< T >(value, static_cast< T >(0.0000001));
}

#endif // BOOST_ATOMIC_TESTS_VALUE_WITH_EPSILON_H_INCLUDED_
