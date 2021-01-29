////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "IoContext.h"

#include <thread>

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

IoContext::IoThread::IoThread(application_features::ApplicationServer& server, IoContext& iocontext)
    : Thread(server, "Io"), _iocontext(iocontext) {}

IoContext::IoThread::IoThread(IoThread const& other)
    : Thread(other._server, "Io"), _iocontext(other._iocontext) {}

IoContext::IoThread::~IoThread() { shutdown(); }

void IoContext::IoThread::run() {
  // run the asio io context
  try {
    _iocontext.io_context.run();
  } catch (std::exception const& ex) {
    LOG_TOPIC("6794f", WARN, Logger::THREADS)
      << "caught exception in IO thread: " << ex.what();
  }
}

IoContext::IoContext(application_features::ApplicationServer& server)
    : io_context(1),  // only a single thread per context
      _server(server),
      _thread(server, *this),
      _work(io_context.get_executor()),
      _clients(0) {
  _thread.start();
}

IoContext::IoContext(IoContext const& other)
    : io_context(1),
      _server(other._server),
      _thread(other._server, *this),
      _work(io_context.get_executor()),
      _clients(0) {
  _thread.start();
}

IoContext::~IoContext() { stop(); }

void IoContext::stop() {
  _work.reset();
  io_context.stop();
  while(_thread.isRunning()) {
    std::this_thread::yield();
  }
}
