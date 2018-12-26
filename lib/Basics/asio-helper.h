////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <boost/asio.hpp>

// these classes are only here in order to shutup compiler warning
// DO NOT USE THEM!

#ifndef ARANGODB_BASICS_ASIO_HELPER_H_1
#define ARANGODB_BASICS_ASIO_HELPER_H_1 1
namespace {
class Unused1 {
 protected:
  const boost::system::error_category& unused1 = boost::asio::error::system_category;

  const boost::system::error_category& unused2 = boost::asio::error::netdb_category;

  const boost::system::error_category& unused3 = boost::asio::error::addrinfo_category;

  const boost::system::error_category& unused4 = boost::asio::error::misc_category;

  const boost::system::error_category& unused5 = boost::system::posix_category;

  const boost::system::error_category& unused6 = boost::system::errno_ecat;

  const boost::system::error_category& unused7 = boost::system::native_ecat;
};
}  // namespace
#endif

#ifdef BOOST_ASIO_SSL_HPP
#ifndef ARANGODB_BASICS_ASIO_HELPER_H_2
#define ARANGODB_BASICS_ASIO_HELPER_H_2 1
namespace {
class Unused2 {
 protected:
  const boost::system::error_category& unused1 = boost::asio::error::ssl_category;
  const boost::system::error_category& unused2 = boost::asio::ssl::error::stream_category;
};
}  // namespace
#endif
#endif
