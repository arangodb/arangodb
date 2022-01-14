// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_STL_INTERFACES_ILL_FORMED_HPP
#define BOOST_STL_INTERFACES_ILL_FORMED_HPP

#include <boost/stl_interfaces/iterator_interface.hpp>


template<template<class...> class Template, typename... Args>
using ill_formed = std::integral_constant<
    bool,
    !boost::stl_interfaces::detail::detector<void, Template, Args...>::value>;

#endif
