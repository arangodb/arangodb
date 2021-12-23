#ifndef HIDDEN_HPP_INCLUDED
#define HIDDEN_HPP_INCLUDED

//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/exception/info.hpp>

typedef boost::error_info<struct my_info_, int> my_info;

struct BOOST_SYMBOL_VISIBLE my_exception: virtual std::exception, virtual boost::exception { };

#endif
