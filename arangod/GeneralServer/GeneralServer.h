////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTP_SERVER_H
#define ARANGOD_HTTP_SERVER_HTTP_SERVER_H 1

#include "Basics/Common.h"
#include "Basics/Thread.h"
#include "Basics/asio_ns.h"
#include "Endpoint/Endpoint.h"

namespace arangodb {
class EndpointList;

namespace rest {

class GeneralServer {
  GeneralServer(GeneralServer const&) = delete;
  GeneralServer const& operator=(GeneralServer const&) = delete;

 public:
  explicit GeneralServer(uint64_t numIoThreads);

 public:
  void setEndpointList(EndpointList const* list);
  void startListening();
  void stopListening();

  class IoContext;

 private:
  class IoThread final : public Thread {
   public:
    explicit IoThread(IoContext& iocontext);
    ~IoThread();
    void run() override;

   private:
    IoContext& _iocontext;
  };

 public:
  class IoContext {
    friend class IoThread;
    friend class GeneralServer;

   public:
    std::atomic<uint64_t> _clients;

   private:
    IoThread _thread;
    asio_ns::io_context _asioIoContext;
    asio_ns::io_context::work _asioWork;

   public:
    IoContext();
    ~IoContext();

    template <typename T>
    asio_ns::deadline_timer* newDeadlineTimer(T timeout) {
      return new asio_ns::deadline_timer(_asioIoContext, timeout);
    }

    asio_ns::steady_timer* newSteadyTimer() {
      return new asio_ns::steady_timer(_asioIoContext);
    }

    asio_ns::io_context::strand* newStrand() {
      return new asio_ns::io_context::strand(_asioIoContext);
    }

    asio_ns::ip::tcp::acceptor* newAcceptor() {
      return new asio_ns::ip::tcp::acceptor(_asioIoContext);
    }

#ifndef _WIN32
    asio_ns::local::stream_protocol::acceptor* newDomainAcceptor() {
      return new asio_ns::local::stream_protocol::acceptor(_asioIoContext);
    }
#endif

    asio_ns::ip::tcp::socket* newSocket() {
      return new asio_ns::ip::tcp::socket(_asioIoContext);
    }

#ifndef _WIN32
    asio_ns::local::stream_protocol::socket* newDomainSocket() {
      return new asio_ns::local::stream_protocol::socket(_asioIoContext);
    }
#endif

    asio_ns::ssl::stream<asio_ns::ip::tcp::socket>* newSslSocket(asio_ns::ssl::context& sslContext) {
      return new asio_ns::ssl::stream<asio_ns::ip::tcp::socket>(_asioIoContext, sslContext);
    }

    asio_ns::ip::tcp::resolver* newResolver() {
      return new asio_ns::ip::tcp::resolver(_asioIoContext);
    }

    void post(std::function<void()>&& handler) {
      _asioIoContext.post(std::move(handler));
    }

    void start();
    void stop();
    bool runningInThisThread() const { return _thread.runningInThisThread(); }
  };

  GeneralServer::IoContext& selectIoContext();

 protected:
  bool openEndpoint(IoContext& ioContext, Endpoint* endpoint);

 private:
  friend class IoThread;
  friend class IoContext;

  uint64_t _numIoThreads;
  std::vector<IoContext> _contexts;
  EndpointList const* _endpointList = nullptr;
};
}  // namespace rest
}  // namespace arangodb

#endif
