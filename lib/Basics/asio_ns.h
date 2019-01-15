////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_LIB_ASIO_NS_H
#define ARANGOD_LIB_ASIO_NS_H 1

#if ARANGODB_STANDALONE_ASIO

#define ASIO_HAS_MOVE 1

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/io_context_strand.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/signal_set.hpp>
#include <asio/ssl.hpp>
#include <asio/steady_timer.hpp>

namespace asio_ns = asio;

#else

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>

namespace boost {
namespace asio {
using error_code = boost::system::error_code;
using io_context = boost::asio::io_service;
using system_error = boost::system::system_error;
}  // namespace asio
}  // namespace boost

namespace asio_ns = boost::asio;

#endif

#endif
