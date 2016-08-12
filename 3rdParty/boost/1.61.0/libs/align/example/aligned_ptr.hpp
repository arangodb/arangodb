/*
(c) 2014 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef ALIGNED_PTR_HPP
#define ALIGNED_PTR_HPP

#include <boost/align/aligned_delete.hpp>
#include <memory>

template<class T>
using aligned_ptr = std::unique_ptr<T,
    boost::alignment::aligned_delete>;

#endif
