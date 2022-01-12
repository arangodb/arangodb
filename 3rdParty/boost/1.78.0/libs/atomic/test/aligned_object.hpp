//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_
#define BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_

#include <cstddef>
#include <new>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>

//! A wrapper that creates an object that has at least the specified alignment
template< typename T, std::size_t Alignment >
class aligned_object
{
private:
    T* m_p;
    unsigned char m_storage[Alignment + sizeof(T)];

public:
    aligned_object()
    {
        m_p = new (get_aligned_storage()) T;
    }

    explicit aligned_object(T const& value)
    {
        m_p = new (get_aligned_storage()) T(value);
    }

    ~aligned_object() BOOST_NOEXCEPT
    {
        m_p->~T();
    }

    T& get() const BOOST_NOEXCEPT
    {
        return *m_p;
    }

    BOOST_DELETED_FUNCTION(aligned_object(aligned_object const&))
    BOOST_DELETED_FUNCTION(aligned_object& operator= (aligned_object const&))

private:
    unsigned char* get_aligned_storage()
    {
#if defined(BOOST_HAS_INTPTR_T)
        typedef boost::uintptr_t uintptr_type;
#else
        typedef std::size_t uintptr_type;
#endif
        unsigned char* p = m_storage;
        uintptr_type misalignment = ((uintptr_type)p) & (Alignment - 1u);
        if (misalignment > 0)
            p += Alignment - misalignment;
        return p;
    }
};

#endif // BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_
