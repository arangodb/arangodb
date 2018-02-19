// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef GEOMETRY_TEST_TEST_GEOMETRIES_COPY_ON_DEREFERENCE_GEOMETRIES_HPP
#define GEOMETRY_TEST_TEST_GEOMETRIES_COPY_ON_DEREFERENCE_GEOMETRIES_HPP

#include <cstddef>
#include <iterator>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_categories.hpp>

#include <boost/geometry/core/tag.hpp>


template <typename RandomAccessIterator>
class copy_on_dereference_iterator
    : public boost::iterator_facade
        <
            copy_on_dereference_iterator<RandomAccessIterator>,
            typename std::iterator_traits<RandomAccessIterator>::value_type,
            boost::random_access_traversal_tag,
            typename std::iterator_traits<RandomAccessIterator>::value_type,
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >
{
private:
    typedef boost::iterator_facade
        <
            copy_on_dereference_iterator<RandomAccessIterator>,
            typename std::iterator_traits<RandomAccessIterator>::value_type,
            boost::random_access_traversal_tag,
            typename std::iterator_traits<RandomAccessIterator>::value_type,
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        > base_type;

public:
    typedef typename base_type::reference reference;
    typedef typename base_type::difference_type difference_type;

    copy_on_dereference_iterator() {}
    copy_on_dereference_iterator(RandomAccessIterator it) : m_it(it) {}

    template <typename OtherRAI>
    copy_on_dereference_iterator
    (copy_on_dereference_iterator<OtherRAI> const& other)
        : m_it(other.m_it)
    {}

private:
    friend class boost::iterator_core_access;

    template <typename OtherRAI>
    friend class copy_on_dereference_iterator;

    inline reference dereference() const { return *m_it; }
    inline void increment() { ++m_it; }
    inline void decrement() { --m_it; }
    inline void advance(difference_type n) { m_it += n; }

    template <typename OtherRAI>
    inline bool equal(copy_on_dereference_iterator<OtherRAI> const& other) const
    {
        return m_it == other.m_it;
    }

    template <typename OtherRAI>
    inline difference_type
    distance_to(copy_on_dereference_iterator<OtherRAI> const& other) const
    {
        return std::distance(m_it, other.m_it);
    }

private:
    RandomAccessIterator m_it;
};


template <typename Value>
class range_copy_on_dereference : private std::vector<Value>
{
private:
    typedef std::vector<Value> base_type;

public:
    typedef typename base_type::size_type size_type;

    typedef copy_on_dereference_iterator
        <
            typename base_type::const_iterator
        > const_iterator;

    typedef const_iterator iterator;

    inline iterator begin()
    {
        return iterator(base_type::begin());
    }

    inline iterator end()
    {
        return iterator(base_type::end());
    }

    inline const_iterator begin() const
    {
        return const_iterator(base_type::begin());
    }

    inline const_iterator end() const
    {
        return const_iterator(base_type::end());
    }

    inline void clear()
    {
        base_type::clear();
    }

    inline void push_back(Value const& value)
    {
        base_type::push_back(value);
    }

    inline void resize(std::size_t n)
    {
        base_type::resize(n);
    }

    inline size_type size() const
    {
        return base_type::size();
    }
};


template <typename Point>
struct multipoint_copy_on_dereference : range_copy_on_dereference<Point>
{};

template <typename Point>
struct linestring_copy_on_dereference : range_copy_on_dereference<Point>
{};


namespace boost { namespace geometry
{

namespace traits
{

template <typename Point>
struct tag< multipoint_copy_on_dereference<Point> >
{
    typedef multi_point_tag type;
};

template <typename Point>
struct tag< linestring_copy_on_dereference<Point> >
{
    typedef linestring_tag type;
};

} // namespace traits

}} // namespace boost::geometry


#endif // GEOMETRY_TEST_TEST_GEOMETRIES_COPY_ON_DEREFERENCE_GEOMETRIES_HPP
