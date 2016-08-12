//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_MEMORY_SVM_PTR_HPP
#define BOOST_COMPUTE_MEMORY_SVM_PTR_HPP

#include <boost/compute/cl.hpp>
#include <boost/compute/type_traits/is_device_iterator.hpp>

namespace boost {
namespace compute {

template<class T>
class svm_ptr
{
public:
    typedef T value_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef T& reference;
    typedef std::random_access_iterator_tag iterator_category;

    svm_ptr()
        : m_ptr(0)
    {
    }

    explicit svm_ptr(void *ptr)
        : m_ptr(static_cast<T*>(ptr))
    {
    }

    svm_ptr(const svm_ptr<T> &other)
        : m_ptr(other.m_ptr)
    {
    }

    svm_ptr& operator=(const svm_ptr<T> &other)
    {
        m_ptr = other.m_ptr;
        return *this;
    }

    ~svm_ptr()
    {
    }

    void* get() const
    {
        return m_ptr;
    }

    svm_ptr<T> operator+(difference_type n)
    {
        return svm_ptr<T>(m_ptr + n);
    }

    difference_type operator-(svm_ptr<T> other)
    {
        return m_ptr - other.m_ptr;
    }

private:
    T *m_ptr;
};

/// \internal_ (is_device_iterator specialization for svm_ptr)
template<class T>
struct is_device_iterator<svm_ptr<T> > : boost::true_type {};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_MEMORY_SVM_PTR_HPP
