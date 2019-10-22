// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_NO_EXCEPTIONS

#include <cstdlib> // std::abort
#include <exception>
#include <iostream>

namespace boost {
// dummy implementation for user-defined function from boost/throw_exception.hpp
inline void throw_exception(std::exception const& e) {
  std::cerr << e.what() << std::endl;
  std::abort();
}
} // namespace boost

#endif
