// Boost.Range library
//
// Copyright Neil Groves 2014. Use, modification and distribution is subject
// to the Boost Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range
//
#ifndef BOOST_RANGE_UNIT_TEST_ADAPTOR_MOCK_ITERATOR_HPP_INCLUDED
#define BOOST_RANGE_UNIT_TEST_ADAPTOR_MOCK_ITERATOR_HPP_INCLUDED

#include <boost/iterator/iterator_facade.hpp>

namespace boost
{
    namespace range
    {
        namespace unit_test
        {

template<typename TraversalTag>
class mock_iterator
        : public boost::iterator_facade<
                    mock_iterator<TraversalTag>,
                    int,
                    TraversalTag,
                    const int&
        >
{
public:
    mock_iterator()
        : m_value(0)
    {
    }

    explicit mock_iterator(int value)
        : m_value(value)
    {
    }

private:

    void increment()
    {
        ++m_value;
    }

    void decrement()
    {
        --m_value;
    }

    bool equal(const mock_iterator& other) const
    {
        return m_value == other.m_value;
    }

    void advance(std::ptrdiff_t offset)
    {
        m_value += offset;
    }

    std::ptrdiff_t distance_to(const mock_iterator& other) const
    {
        return other.m_value - m_value;
    }

    const int& dereference() const
    {
        return m_value;
    }

    int m_value;
    friend class boost::iterator_core_access;
};

        } // namespace unit_test
    } // namespace range
} // namespace boost

#endif // include guard
