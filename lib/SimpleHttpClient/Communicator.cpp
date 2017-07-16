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

#include "Communicator.h"

#include <iostream>

#include "Basics/MutexLocker.h"
#include "Basics/socket-utils.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::communicator;

namespace {

#ifdef _WIN32
/* socketpair.c
Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
The name of the author must not be used to endorse or promote products
derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
static int dumb_socketpair(SOCKET socks[2], int make_overlapped) {
  union {
    struct sockaddr_in inaddr;
    struct sockaddr addr;
  } a;
  memset(&a, 0, sizeof(a)); // clear memory before using the struct!

  SOCKET listener;
  int e;
  socklen_t addrlen = sizeof(a.inaddr);
  DWORD flags = (make_overlapped ? WSA_FLAG_OVERLAPPED : 0);
  int reuse = 1;

  if (socks == 0) {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  socks[0] = socks[1] = -1;

  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) return SOCKET_ERROR;

  memset(&a, 0, sizeof(a));
  a.inaddr.sin_family = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port = 0;

  for (;;) {
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
                   (socklen_t)sizeof(reuse)) == -1)
      break;
    if (bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR) break;

    memset(&a, 0, sizeof(a));
    if (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR) break;
    // win32 getsockname may only set the port number, p=0.0005.
    // ( http://msdn.microsoft.com/library/ms738543.aspx ):
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;

    if (listen(listener, 1) == SOCKET_ERROR) break;

    socks[0] = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
    if (socks[0] == -1) break;
    if (connect(socks[0], &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR) break;

    socks[1] = accept(listener, NULL, NULL);
    if (socks[1] == -1) break;

    closesocket(listener);

    u_long mode = 1;
    int res = ioctlsocket(socks[0], FIONBIO, &mode);
    if (res != NO_ERROR) break;

    return 0;
  }

  e = WSAGetLastError();
  closesocket(listener);
  closesocket(socks[0]);
  closesocket(socks[1]);
  WSASetLastError(e);
  socks[0] = socks[1] = -1;
  return SOCKET_ERROR;
}
#endif

std::atomic_uint_fast64_t NEXT_TICKET_ID(static_cast<uint64_t>(0));
std::vector<char> urlDotSeparators{'/', '#', '?'};
}

Communicator::Communicator() : _curl(nullptr), _mc(CURLM_OK), _enabled(true) {
  curl_global_init(CURL_GLOBAL_ALL);
  _curl = curl_multi_init();

#ifdef _WIN32
  int err = dumb_socketpair(_socks, 0);
  if (err != 0) {
    throw std::runtime_error("Couldn't setup sockets. Error was: " +
                             std::to_string(err));
  }
  _wakeup.fd = _socks[0];
#else
  int result = pipe(_fds);
  if (result != 0) {
    throw std::runtime_error("Couldn't setup pipe. Return code was: " +
                             std::to_string(result));
  }

  TRI_socket_t socket = {.fileDescriptor = _fds[0]};
  TRI_SetNonBlockingSocket(socket);
  _wakeup.fd = _fds[0];
#endif

  _wakeup.events = CURL_WAIT_POLLIN | CURL_WAIT_POLLPRI;
  // TODO: does _wakeup.revents has to be initialized here?
}

Communicator::~Communicator() {
  ::curl_multi_cleanup(_curl);
  ::curl_global_cleanup();
}

Ticket Communicator::addRequest(Destination destination,
                                std::unique_ptr<GeneralRequest> request,
                                Callbacks callbacks, Options options) {
  uint64_t id = NEXT_TICKET_ID.fetch_add(1, std::memory_order_seq_cst);

  {
    TRI_ASSERT(request != nullptr);
    MUTEX_LOCKER(guard, _newRequestsLock);
    _newRequests.emplace_back(
        NewRequest{destination, std::move(request), callbacks, options, id});
  }

  LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "request to " << destination.url() << " has been put onto queue";
  // mop: just send \0 terminated empty string to wake up worker thread
#ifdef _WIN32
  ssize_t numBytes = send(_socks[1], "", 1, 0);
#else
  ssize_t numBytes = write(_fds[1], "", 1);
#endif

  if (numBytes != 1) {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "Couldn't wake up pipe. numBytes was " + std::to_string(numBytes);
  }

  return Ticket{id};
}

int Communicator::work_once() {
  std::vector<NewRequest> newRequests;

  {
    MUTEX_LOCKER(guard, _newRequestsLock);
    newRequests.swap(_newRequests);
  }

  for (auto const& newRequest : newRequests) {
    createRequestInProgress(newRequest);
  }

  int stillRunning;
  _mc = curl_multi_perform(_curl, &stillRunning);
  if (_mc != CURLM_OK) {
    throw std::runtime_error(
        "Invalid curl multi result while performing! Result was " +
        std::to_string(_mc));
  }

  // handle all messages received
  CURLMsg* msg = nullptr;
  int msgsLeft = 0;

  while ((msg = curl_multi_info_read(_curl, &msgsLeft))) {
    if (msg->msg == CURLMSG_DONE) {
      CURL* handle = msg->easy_handle;

      handleResult(handle, msg->data.result);
    }
  }
  return stillRunning;
}

void Communicator::wait() {
  static int const MAX_WAIT_MSECS = 1000;  // wait max. 1 seconds

  int numFds;  // not used here
  int res = curl_multi_wait(_curl, &_wakeup, 1, MAX_WAIT_MSECS, &numFds);
  if (res != CURLM_OK) {
    throw std::runtime_error(
        "Invalid curl multi result while waiting! Result was " +
        std::to_string(res));
  }

  // drain the pipe
  char a[16];
#ifdef _WIN32
  while (0 < recv(_socks[0], a, sizeof(a), 0)) {
  }
#else
  while (0 < read(_fds[0], a, sizeof(a))) {
  }
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void Communicator::createRequestInProgress(NewRequest const& newRequest) {
  if (!_enabled) {
    LOG_TOPIC(DEBUG, arangodb::Logger::COMMUNICATION) << "Request to  '" << newRequest._destination.url() << "' was not even started because communication is disabled";
    newRequest._callbacks._onError(TRI_COMMUNICATOR_DISABLED, {nullptr});
    return;
  }

  auto request = (HttpRequest*)newRequest._request.get();
  TRI_ASSERT(request != nullptr);

  // mop: the curl handle will be managed safely via unique_ptr and hold
  // ownership for rip
  auto rip = new RequestInProgress(
      newRequest._destination, newRequest._callbacks, newRequest._ticketId,
      std::string(request->body().c_str(), request->body().length()),
      newRequest._options);

  auto handleInProgress = std::make_unique<CurlHandle>(rip);

  CURL* handle = handleInProgress->_handle;
  struct curl_slist* requestHeaders = nullptr;

  switch (request->contentType()) {
    case ContentType::UNSET:
    case ContentType::CUSTOM:
    case ContentType::VPACK:
    case ContentType::DUMP:
      break;
    case ContentType::JSON:
      requestHeaders =
          curl_slist_append(requestHeaders, "Content-Type: application/json");
      break;
    case ContentType::HTML:
      requestHeaders =
          curl_slist_append(requestHeaders, "Content-Type: text/html");
      break;
    case ContentType::TEXT:
      requestHeaders =
          curl_slist_append(requestHeaders, "Content-Type: text/plain");
      break;
  }
  for (auto const& header : request->headers()) {
    std::string thisHeader(header.first + ": " + header.second);
    requestHeaders = curl_slist_append(requestHeaders, thisHeader.c_str());
  }

  std::string url = createSafeDottedCurlUrl(newRequest._destination.url());
  handleInProgress->_rip->_requestHeaders = requestHeaders;
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, requestHeaders);
  curl_easy_setopt(handle, CURLOPT_HEADER, 0L);
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(handle, CURLOPT_PROXY, "");
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, Communicator::readBody);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, handleInProgress->_rip.get());
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, Communicator::readHeaders);
  curl_easy_setopt(handle, CURLOPT_HEADERDATA, handleInProgress->_rip.get());
  curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, Communicator::curlDebug);
  curl_easy_setopt(handle, CURLOPT_DEBUGDATA, handleInProgress->_rip.get());
  curl_easy_setopt(handle, CURLOPT_ERRORBUFFER,
                   handleInProgress->_rip.get()->_errorBuffer);

  // mop: XXX :S CURLE 51 and 60...
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

  long connectTimeout =
      static_cast<long>(newRequest._options.connectionTimeout);
  // mop: although curl is offering a MS scale connecttimeout this gets ignored
  // in at least 7.50.3
  // in doubt change the timeout to _MS below and hardcode it to 999 and see if
  // the requests immediately fail
  // if not this hack can go away
  if (connectTimeout <= 0) {
    connectTimeout = 1;
  }

  curl_easy_setopt(
      handle, CURLOPT_TIMEOUT_MS,
      static_cast<long>(newRequest._options.requestTimeout * 1000));
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, connectTimeout);

  switch (request->requestType()) {
    // mop: hmmm...why is this stuff in GeneralRequest? we are interested in
    // HTTP only :S
    case RequestType::POST:
      curl_easy_setopt(handle, CURLOPT_POST, 1);
      break;
    case RequestType::PUT:
      // mop: apparently CURLOPT_PUT implies more stuff in curl
      // (for example it adds an expect 100 header)
      // this is not what we want so we make it a custom request
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
      break;
    case RequestType::DELETE_REQ:
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case RequestType::HEAD:
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "HEAD");
      break;
    case RequestType::PATCH:
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PATCH");
      break;
    case RequestType::OPTIONS:
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
      break;
    case RequestType::GET:
      break;
    case RequestType::VSTREAM_CRED:
    case RequestType::VSTREAM_REGISTER:
    case RequestType::VSTREAM_STATUS:
    case RequestType::ILLEGAL:
      throw std::runtime_error(
          "Invalid request type " +
          GeneralRequest::translateMethod(request->requestType()));
      break;
  }

  if (request->body().length() > 0) {
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                     handleInProgress->_rip->_requestBody.length());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                     handleInProgress->_rip->_requestBody.c_str());
  }

  handleInProgress->_rip->_startTime = TRI_microtime();
 
  { 
    MUTEX_LOCKER(guard, _handlesLock);
    _handlesInProgress.emplace(newRequest._ticketId, std::move(handleInProgress));
  }
  curl_multi_add_handle(_curl, handle);
}

void Communicator::handleResult(CURL* handle, CURLcode rc) {
  // remove request in progress
  curl_multi_remove_handle(_curl, handle);

  RequestInProgress* rip = nullptr;
  curl_easy_getinfo(handle, CURLINFO_PRIVATE, &rip);
  if (rip == nullptr) {
    return;
  }
  std::string prefix("Communicator(" + std::to_string(rip->_ticketId) +
                     ") // ");
  LOG_TOPIC(TRACE, Logger::COMMUNICATION)
      << prefix << "Curl rc is : " << rc << " after "
      << Logger::FIXED(TRI_microtime() - rip->_startTime) << " s";
  if (strlen(rip->_errorBuffer) != 0) {
    LOG_TOPIC(TRACE, Logger::COMMUNICATION)
        << prefix << "Curl error details: " << rip->_errorBuffer;
  }

  MUTEX_LOCKER(guard, _handlesLock);
  switch (rc) {
    case CURLE_OK: {
      long httpStatusCode = 200;
      curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpStatusCode);

      std::unique_ptr<GeneralResponse> response(
          new HttpResponse(static_cast<ResponseCode>(httpStatusCode)));

      transformResult(handle, std::move(rip->_responseHeaders),
                      std::move(rip->_responseBody),
                      dynamic_cast<HttpResponse*>(response.get()));

      if (httpStatusCode < 400) {
        rip->_callbacks._onSuccess(std::move(response));
      } else {
        rip->_callbacks._onError(httpStatusCode, std::move(response));
      }
      break;
    }
    case CURLE_COULDNT_CONNECT:
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_URL_MALFORMAT:
    case CURLE_SEND_ERROR:
      rip->_callbacks._onError(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT, {nullptr});
      break;
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_RECV_ERROR:
    case CURLE_GOT_NOTHING:
      rip->_callbacks._onError(TRI_ERROR_CLUSTER_TIMEOUT, {nullptr});
      break;
    default:
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Curl return " << rc;
      rip->_callbacks._onError(TRI_ERROR_INTERNAL, {nullptr});
      break;
  } 
    
  _handlesInProgress.erase(rip->_ticketId);
}

void Communicator::transformResult(CURL* handle,
                                   HeadersInProgress&& responseHeaders,
                                   std::unique_ptr<StringBuffer> responseBody,
                                   HttpResponse* response) {
  response->body().swap(responseBody.get());
  response->setHeaders(std::move(responseHeaders));
}

size_t Communicator::readBody(void* data, size_t size, size_t nitems,
                              void* userp) {
  size_t realsize = size * nitems;

  RequestInProgress* rip = (struct RequestInProgress*)userp;
  try {
    rip->_responseBody->appendText((char*)data, realsize);
    return realsize;
  } catch (std::bad_alloc&) {
    return 0;
  }
}

void Communicator::logHttpBody(std::string const& prefix,
                               std::string const& data) {
  std::string::size_type n = 0;
  while (n < data.length()) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << prefix << " "
                                            << data.substr(n, 80);
    n += 80;
  }
}

void Communicator::logHttpHeaders(std::string const& prefix,
                                  std::string const& headerData) {
  std::string::size_type last = 0;
  std::string::size_type n;
  while (true) {
    n = headerData.find("\r\n", last);
    if (n == std::string::npos) {
      break;
    }
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << prefix << " " << headerData.substr(last, n - last);
    last = n + 2;
  }
}

int Communicator::curlDebug(CURL* handle, curl_infotype type, char* data,
                            size_t size, void* userptr) {
  arangodb::communicator::RequestInProgress* request = nullptr;
  curl_easy_getinfo(handle, CURLINFO_PRIVATE, &request);
  TRI_ASSERT(request != nullptr);
  TRI_ASSERT(data != nullptr);

  std::string dataStr(data, size);
  std::string prefix("Communicator(" + std::to_string(request->_ticketId) +
                     ") // ");

  switch (type) {
    case CURLINFO_TEXT:
      LOG_TOPIC(TRACE, Logger::COMMUNICATION) << prefix << "Text: " << dataStr;
      break;
    case CURLINFO_HEADER_OUT:
      logHttpHeaders(prefix + "Header >>", dataStr);
      break;
    case CURLINFO_HEADER_IN:
      logHttpHeaders(prefix + "Header <<", dataStr);
      break;
    case CURLINFO_DATA_OUT:
    case CURLINFO_SSL_DATA_OUT:
      logHttpBody(prefix + "Body >>", dataStr);
      break;
    case CURLINFO_DATA_IN:
    case CURLINFO_SSL_DATA_IN:
      logHttpBody(prefix + "Body <<", dataStr);
      break;
    case CURLINFO_END:
      break;
  }
  return 0;
}

size_t Communicator::readHeaders(char* buffer, size_t size, size_t nitems,
                                 void* userptr) {
  size_t realsize = size * nitems;
  RequestInProgress* rip = (struct RequestInProgress*)userptr;

  std::string const header(buffer, realsize);
  size_t pivot = header.find_first_of(':');
  if (pivot != std::string::npos) {
    // mop: hmm response needs lowercased headers
    std::string headerKey =
        basics::StringUtils::tolower(std::string(header.c_str(), pivot));
    rip->_responseHeaders.emplace(
        headerKey, header.substr(pivot + 2, header.length() - pivot - 4));
  }
  return realsize;
}

std::string Communicator::createSafeDottedCurlUrl(
    std::string const& originalUrl) {
  std::string url;
  url.reserve(originalUrl.length());

  size_t length = originalUrl.length();
  size_t currentFind = 0;
  std::size_t found;
  std::vector<char> urlDotSeparators{'/', '#', '?'};

  while ((found = originalUrl.find("/.", currentFind)) != std::string::npos) {
    if (found + 2 == length) {
      url += originalUrl.substr(currentFind, found - currentFind) + "/%2E";
    } else if (std::find(urlDotSeparators.begin(), urlDotSeparators.end(),
                         originalUrl.at(found + 2)) != urlDotSeparators.end()) {
      url += originalUrl.substr(currentFind, found - currentFind) + "/%2E";
    } else {
      url += originalUrl.substr(currentFind, found - currentFind) + "/.";
    }
    currentFind = found + 2;
  }
  url += originalUrl.substr(currentFind);
  return url;
}

void Communicator::abortRequest(Ticket ticketId) {
  MUTEX_LOCKER(guard, _handlesLock);

  abortRequestInternal(ticketId);
}

void Communicator::abortRequests() {
  MUTEX_LOCKER(guard, _handlesLock);

  for (auto& request : requestsInProgress()) {
    abortRequestInternal(request->_ticketId);
  }
}

// needs _handlesLock! 
std::vector<RequestInProgress const*> Communicator::requestsInProgress() {
  _handlesLock.assertLockedByCurrentThread();

  std::vector<RequestInProgress const*> vec;
    
  vec.reserve(_handlesInProgress.size());

  for (auto& handle : _handlesInProgress) {
    RequestInProgress* rip = nullptr;
    curl_easy_getinfo(handle.second->_handle, CURLINFO_PRIVATE, &rip);
    TRI_ASSERT(rip != nullptr);
    vec.push_back(rip);
  }
  return vec;
}

// needs _handlesLock! 
void Communicator::abortRequestInternal(Ticket ticketId) {
  _handlesLock.assertLockedByCurrentThread();

  auto handle = _handlesInProgress.find(ticketId);
  if (handle == _handlesInProgress.end()) {
    return;
  }
  std::string prefix("Communicator(" + std::to_string(handle->second->_rip->_ticketId) +
                     ") // ");
  LOG_TOPIC(WARN, Logger::REQUESTS) << prefix << "aborting request to " << handle->second->_rip->_destination.url();
  handle->second->_rip->_callbacks._onError(TRI_COMMUNICATOR_REQUEST_ABORTED,
                                            {nullptr});
  _handlesInProgress.erase(ticketId);
}

