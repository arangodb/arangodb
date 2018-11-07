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
/// @author Andreas Streichardt
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_COMMUNICATOR_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_COMMUNICATOR_H 1

#include <chrono>

#include "curl/curl.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Rest/GeneralRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/Callbacks.h"
#include "SimpleHttpClient/Destination.h"
#include "SimpleHttpClient/Options.h"

namespace arangodb {
namespace communicator {
typedef std::unordered_map<std::string, std::string> HeadersInProgress;
typedef uint64_t Ticket;

struct RequestInProgress {
  RequestInProgress(Destination destination, Callbacks callbacks,
                    Ticket ticketId, Options const& options,
                    std::unique_ptr<GeneralRequest> request)
      : _destination(destination),
        _callbacks(callbacks),
        _request(std::move(request)),
        _ticketId(ticketId),
        _requestHeaders(nullptr),
        _startTime(0.0),
        _responseBody(new basics::StringBuffer(1024, false)),
        _options(options),
        _aborted(false) {
    _errorBuffer[0] = '\0';
  }

  ~RequestInProgress() {
    if (_requestHeaders != nullptr) {
      curl_slist_free_all(_requestHeaders);
    }
  }

  RequestInProgress(RequestInProgress const& other) = delete;
  RequestInProgress& operator=(RequestInProgress const& other) = delete;

  Destination _destination;
  Callbacks _callbacks;
  std::unique_ptr<GeneralRequest> _request;
  Ticket _ticketId;
  struct curl_slist* _requestHeaders;

  HeadersInProgress _responseHeaders;
  double _startTime;
  std::unique_ptr<basics::StringBuffer> _responseBody;
  Options _options;

  char _errorBuffer[CURL_ERROR_SIZE];
  bool _aborted;
};

struct CurlHandle : public std::enable_shared_from_this<CurlHandle> {
  explicit CurlHandle(RequestInProgress* rip) : _handle(nullptr), _rip(rip) {
    _handle = curl_easy_init();
    if (_handle == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    curl_easy_setopt(_handle, CURLOPT_PRIVATE, _rip.get());
    curl_easy_setopt(_handle, CURLOPT_PATH_AS_IS, 1L);
  }
  ~CurlHandle() {
    if (_handle != nullptr) {
      curl_easy_cleanup(_handle);
    }
  }

  std::shared_ptr<CurlHandle> getSharedPtr() {return shared_from_this();}

  CurlHandle(CurlHandle& other) = delete;
  CurlHandle& operator=(CurlHandle& other) = delete;

  CURL* _handle;
  std::unique_ptr<RequestInProgress> _rip;
};


/// @brief ConnectionCount
///
/// libcurl's native connection management has 3 modes based upon how
///  curl_multi_setopt(_curl, CURLMOPT_MAXCONNECTS, xx) is set:
///
///  -1: default, close connections above 4 times the number of active connections,
///       open more as needed
///   0: never close connections, open more as needed
/// int: never open more than "int", never close either
///
///  -1 caused bugs with clients using 64 threads.  The number of open connections
///  would fluctuate wildly, and sometimes the reopening of connections timed out.
///  This code smooths the rate at which connections get closed.
class ConnectionCount {
public:
  ConnectionCount()
    : cursorMinute(0),
    nextMinute(std::chrono::steady_clock::now() + std::chrono::seconds(60))
  {
    for (int loop=0; loop<eMinutesTracked; ++loop) {
      maxInMinute[loop] = 0;
    } //for
  };

  virtual ~ConnectionCount() {};

  int newMaxConnections(int newRequestCount) {
    int ret_val(eMinOpenConnects);

    for (int loop=0; loop<eMinutesTracked; ++loop) {
      if (ret_val < maxInMinute[loop]) {
        ret_val = maxInMinute[loop];
      } // if
    } // for
    ret_val += newRequestCount;

    return ret_val;
  };

  void updateMaxConnections(int openActions) {
    // move to new minute?
    if (nextMinute < std::chrono::steady_clock::now()) {
      advanceCursor();
    } // if

    // current have more active that previously measured?
    if (maxInMinute[cursorMinute] < openActions) {
      maxInMinute[cursorMinute] = openActions;
    } // if
  };

  enum {
    eMinutesTracked = 6,
    eMinOpenConnects = 5
  };

protected:
  void advanceCursor() {
    nextMinute += std::chrono::seconds(60);
    cursorMinute = (cursorMinute + 1) % eMinutesTracked;
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "ConnectionCount::advanceCursor cursorMinute " << cursorMinute
      << ", retired period " << maxInMinute[cursorMinute]
      << ", newMaxConnections " << newMaxConnections(0);
    maxInMinute[cursorMinute] = 0;
  };


  int maxInMinute[eMinutesTracked];
  int cursorMinute;
  std::chrono::steady_clock::time_point nextMinute;

private:
  ConnectionCount(ConnectionCount const &) = delete;
  ConnectionCount(ConnectionCount &&) = delete;
  ConnectionCount & operator=(ConnectionCount const &) = delete;


}; // class ConnectionCount
}
}

namespace arangodb {
namespace communicator {

#ifdef MAINTAINER_MODE
const static double CALLBACK_WARN_TIME = 0.01;
#else
const static double CALLBACK_WARN_TIME = 0.1;
#endif

class Communicator {
 public:
  Communicator();
  ~Communicator();

 public:
  Ticket addRequest(Destination&&, std::unique_ptr<GeneralRequest>, Callbacks,
                    Options);

  int work_once();
  void wait();
  void abortRequest(Ticket ticketId);
  void abortRequests();
  void disable() { _enabled = false; };
  void enable()  { _enabled = true; };

 private:
  struct NewRequest {
    Destination _destination;
    std::unique_ptr<GeneralRequest> _request;
    Callbacks _callbacks;
    Options _options;
    Ticket _ticketId;
  };

  struct CurlData {};

 private:
  Mutex _newRequestsLock;
  std::vector<NewRequest> _newRequests;

  Mutex _handlesLock;
  std::unordered_map<uint64_t, std::shared_ptr<CurlHandle>> _handlesInProgress;

  CURLM* _curl;
  CURLMcode _mc;
  curl_waitfd _wakeup;
#ifdef _WIN32
  SOCKET _socks[2];
#else
  int _fds[2];
#endif
  bool _enabled;
  ConnectionCount connectionCount;

 private:
  void abortRequestInternal(Ticket ticketId);
  std::vector<RequestInProgress const*> requestsInProgress();
  void createRequestInProgress(NewRequest&& newRequest);
  void handleResult(CURL*, CURLcode);
  /// @brief curl will strip standalone ".". ArangoDB allows using . as a key
  /// so this thing will analyse the url and urlencode any unsafe .'s
  std::string createSafeDottedCurlUrl(std::string const& originalUrl);

  // these function are static because they are called by a lambda function
  //  that could execute after Communicator object destroyed.
  static void transformResult(CURL*, HeadersInProgress&&,
                       std::unique_ptr<basics::StringBuffer>, HttpResponse*);
  static void callErrorFn(RequestInProgress*, int const&, std::unique_ptr<GeneralResponse>);
  static void callErrorFn(Ticket const&, Destination const&, Callbacks const&, int const&, std::unique_ptr<GeneralResponse>);
  static void callSuccessFn(Ticket const&, Destination const&, Callbacks const&, std::unique_ptr<GeneralResponse>);

 private:
  static size_t readBody(void*, size_t, size_t, void*);
  static size_t readHeaders(char* buffer, size_t size, size_t nitems,
                            void* userdata);
  static int curlDebug(CURL*, curl_infotype, char*, size_t, void*);
  static int curlProgress(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t);
  static void logHttpHeaders(std::string const&, std::string const&);
  static void logHttpBody(std::string const&, std::string const&);
};
}
}

#endif
