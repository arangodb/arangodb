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
                    Ticket ticketId, std::string const& requestBody,
                    Options const& options)
      : _destination(destination),
        _callbacks(callbacks),
        _ticketId(ticketId),
        _requestBody(requestBody),
        _requestHeaders(nullptr),
        _startTime(0.0),
        _responseBody(new basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE, false)),
        _options(options) {
    _errorBuffer[0] = '\0';
  }

  ~RequestInProgress() {
    if (_requestHeaders != nullptr) {
      curl_slist_free_all(_requestHeaders);
    }
  }

  RequestInProgress(RequestInProgress const& other) = delete;
  RequestInProgress& operator=(RequestInProgress const& other) = delete;

  // mop: i think we should just hold the full request here later
  Destination _destination;
  Callbacks _callbacks;
  Ticket _ticketId;
  std::string _requestBody;
  struct curl_slist* _requestHeaders;

  HeadersInProgress _responseHeaders;
  double _startTime;
  std::unique_ptr<basics::StringBuffer> _responseBody;
  Options _options;

  char _errorBuffer[CURL_ERROR_SIZE];
};

struct CurlHandle {
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

  CurlHandle(CurlHandle& other) = delete;
  CurlHandle& operator=(CurlHandle& other) = delete;

  CURL* _handle;
  std::unique_ptr<RequestInProgress> _rip;
};
}
}

namespace arangodb {
namespace communicator {

class Communicator {
 public:
  Communicator();
  ~Communicator();

 public:
  Ticket addRequest(Destination, std::unique_ptr<GeneralRequest>, Callbacks,
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
  std::unordered_map<uint64_t, std::unique_ptr<CurlHandle>> _handlesInProgress;
  
  CURLM* _curl;
  CURLMcode _mc;
  curl_waitfd _wakeup;
#ifdef _WIN32
  SOCKET _socks[2];
#else
  int _fds[2];
#endif
  bool _enabled;

 private:
  void abortRequestInternal(Ticket ticketId);
  std::vector<RequestInProgress const*> requestsInProgress();
  void createRequestInProgress(NewRequest const& newRequest);
  void handleResult(CURL*, CURLcode);
  void transformResult(CURL*, HeadersInProgress&&,
                       std::unique_ptr<basics::StringBuffer>, HttpResponse*);
  /// @brief curl will strip standalone ".". ArangoDB allows using . as a key
  /// so this thing will analyse the url and urlencode any unsafe .'s
  std::string createSafeDottedCurlUrl(std::string const& originalUrl);

 private:
  static size_t readBody(void*, size_t, size_t, void*);
  static size_t readHeaders(char* buffer, size_t size, size_t nitems,
                            void* userdata);
  static int curlDebug(CURL*, curl_infotype, char*, size_t, void*);
  static void logHttpHeaders(std::string const&, std::string const&);
  static void logHttpBody(std::string const&, std::string const&);
};
}
}

#endif
