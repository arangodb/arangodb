////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#pragma once

// make sure that IOCP is used on windows
#if defined(_WIN32) && !defined(_WIN32_WINNT)
// #define _WIN32_WINNT_VISTA  0x0600
#define _WIN32_WINNT 0x0600
#endif

#if ARANGODB_STANDALONE_ASIO

#define ASIO_HAS_MOVE 1

#include <asio/buffer.hpp>
#include <asio/connect.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/io_context_strand.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/read.hpp>
#include <asio/signal_set.hpp>
#include <asio/ssl.hpp>
#include <asio/steady_timer.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

namespace asio_ns = asio;

#else

#define ASIO_HAS_MOVE 1

#ifdef __clang__
// Suppress the warning
//   3rdParty\boost\1.78.0\boost/asio/detail/strand_service.hpp(112,15): error :
//     private field 'io_context_' is not used [-Werror,-Wunused-private-field]
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#include <boost/asio.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>

namespace boost {
namespace asio {
using error_code = boost::system::error_code;
using system_error = boost::system::system_error;
}  // namespace asio
}  // namespace boost

namespace asio_ns = boost::asio;

#endif
