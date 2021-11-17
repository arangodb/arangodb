// Copyright (c) 2018 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/system/api_config.hpp>
#if defined(BOOST_WINDOWS_API)
#define BOOST_NO_ANSI_APIS 1
#endif
#include <boost/process.hpp>