/*
(c) 2014 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef ALIGNED_VECTOR_HPP
#define ALIGNED_VECTOR_HPP

#include <boost/align/aligned_allocator.hpp>
#include <vector>

template<class T, std::size_t Alignment = 1>
using aligned_vector = std::vector<T,
    boost::alignment::aligned_allocator<T, Alignment> >;

#endif
