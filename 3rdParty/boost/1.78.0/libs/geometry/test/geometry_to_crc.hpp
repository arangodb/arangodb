// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef GEOMETRY_TEST_GEOMETRY_TO_CRC_HPP
#define GEOMETRY_TEST_GEOMETRY_TO_CRC_HPP

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/crc.hpp>

#include <iomanip>
#include <string>

// Convenience function for tests, it generates a 4 character CRC of a geometry
// which can be used in filenames and/or testcases such as 'nores_37f6'
template <typename Geometry>
inline std::string geometry_to_crc(Geometry const& geometry)
{
  std::ostringstream out1;
  out1 << bg::wkt(geometry);

  const std::string str = out1.str();

  boost::crc_ccitt_type result;
  result.process_bytes(str.data(), str.length());

  std::ostringstream out2;
  out2 << std::setw(4) << std::setfill('0') << std::hex << result.checksum();
  return out2.str();
}

#endif // GEOMETRY_TEST_GEOMETRY_TO_CRC_HPP
