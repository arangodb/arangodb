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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H 1

#include "Basics/Common.h"

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

namespace arangodb {
namespace httpclient {

class SimpleHttpResult;
class GeneralClientConnection;

////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
////////////////////////////////////////////////////////////////////////////////

class SimpleHttpClient {
 private:
  SimpleHttpClient(SimpleHttpClient const&) = delete;
  SimpleHttpClient& operator=(SimpleHttpClient const&) = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief state of the connection
  //////////////////////////////////////////////////////////////////////////////

  enum request_state {
    IN_CONNECT,
    IN_WRITE,
    IN_READ_HEADER,
    IN_READ_BODY,
    IN_READ_CHUNKED_HEADER,
    IN_READ_CHUNKED_BODY,
    FINISHED,
    DEAD
  };

 public:
  SimpleHttpClient(std::unique_ptr<GeneralClientConnection>&, double, bool);
  SimpleHttpClient(GeneralClientConnection*, double, bool);
  ~SimpleHttpClient();

 public:
  void setInterrupted(bool value);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidates the connection used by the client
  /// this may be called from other objects that are responsible for managing
  /// connections. after this method has been called, the client must not be
  /// used for any further HTTP operations, but should be destroyed instantly.
  //////////////////////////////////////////////////////////////////////////////

  void invalidateConnection() { _connection = nullptr; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if the connection is open
  //////////////////////////////////////////////////////////////////////////////

  bool isConnected();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if the connection is open
  //////////////////////////////////////////////////////////////////////////////

  void disconnect();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string representation of the connection endpoint
  //////////////////////////////////////////////////////////////////////////////

  std::string getEndpointSpecification() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief close connection, go to state IN_CONNECT and clear the input
  /// buffer. This is used to organise a retry of the connection.
  //////////////////////////////////////////////////////////////////////////////

  void close();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief leave connection open on destruction
  //////////////////////////////////////////////////////////////////////////////

  void keepConnectionOnDestruction(bool b) { _keepConnectionOnDestruction = b; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief make an http request, creating a new HttpResult object
  /// the caller has to delete the result object
  /// this version does not allow specifying custom headers
  /// if the request fails because of connection problems, the request will be
  /// retried until it either succeeds (at least no connection problem) or there
  /// have been _maxRetries retries
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* retryRequest(rest::RequestType,
                                 std::string const&, char const*, size_t,
                                 std::unordered_map<std::string, std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief make an http request, creating a new HttpResult object
  /// the caller has to delete the result object
  /// this version does not allow specifying custom headers
  /// if the request fails because of connection problems, the request will be
  /// retried until it either succeeds (at least no connection problem) or there
  /// have been _maxRetries retries
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* retryRequest(rest::RequestType,
                                 std::string const&, char const*, size_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief make an http request, creating a new HttpResult object
  /// the caller has to delete the result object
  /// this version does not allow specifying custom headers
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* request(rest::RequestType,
                            std::string const&, char const*, size_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief make an http request, actual worker function
  /// the caller has to delete the result object
  /// this version allows specifying custom headers
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* request(rest::RequestType,
                            std::string const&, char const*, size_t,
                            std::unordered_map<std::string, std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets username and password
  ///
  /// @param prefix                         prefix for sending username and
  /// password
  /// @param username                       username
  /// @param password                       password
  //////////////////////////////////////////////////////////////////////////////

  void setUserNamePassword(std::string const& prefix,
                           std::string const& username,
                           std::string const& password);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief allows rewriting locations
  //////////////////////////////////////////////////////////////////////////////

  void setLocationRewriter(void* data,
                           std::string (*func)(void*, std::string const&)) {
    _locationRewriter.data = data;
    _locationRewriter.func = func;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the value for max packet size
  //////////////////////////////////////////////////////////////////////////////

  static void setMaxPacketSize(size_t value) {
    MaxPacketSize = value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief enable or disable keep-alive
  //////////////////////////////////////////////////////////////////////////////

  void setKeepAlive(bool value) { _keepAlive = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief expose ArangoDB via user-agent?
  //////////////////////////////////////////////////////////////////////////////

  void setExposeArangoDB(bool value) { _exposeArangoDB = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief advertise support for deflate?
  //////////////////////////////////////////////////////////////////////////////

  void setSupportDeflate(bool value) { _supportDeflate = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the current error message
  //////////////////////////////////////////////////////////////////////////////

  std::string const& getErrorMessage() const { return _errorMessage; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register and dump an error message
  //////////////////////////////////////////////////////////////////////////////

  void setErrorMessage(std::string const& message, bool forceWarn = false) {
    _errorMessage = message;

    if (_warn || forceWarn) {
      LOG(WARN) << "" << _errorMessage;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register an error message
  //////////////////////////////////////////////////////////////////////////////

  void setErrorMessage(std::string const& message, int error) {
    if (error != TRI_ERROR_NO_ERROR) {
      _errorMessage = message + ": " + strerror(error);
    } else {
      setErrorMessage(message);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether an error message is already there
  //////////////////////////////////////////////////////////////////////////////

  bool haveErrorMessage() const { return _errorMessage.size() > 0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fetch the version from the server
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerVersion(int* errorCode = nullptr);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract an error message from a response
  //////////////////////////////////////////////////////////////////////////////

  std::string getHttpErrorMessage(SimpleHttpResult const*, int* errorCode = nullptr);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief make a http request, creating a new HttpResult object
  /// the caller has to delete the result object
  /// this version allows specifying custom headers
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* doRequest(rest::RequestType,
                              std::string const&, char const*, size_t,
                              std::unordered_map<std::string, std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the connection
  //////////////////////////////////////////////////////////////////////////////

  void handleConnect();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clearReadBuffer, clears the read buffer as well as the result
  //////////////////////////////////////////////////////////////////////////////

  void clearReadBuffer();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief rewrite a location URL
  //////////////////////////////////////////////////////////////////////////////

  std::string rewriteLocation(std::string const& location) {
    if (_locationRewriter.func != nullptr) {
      return _locationRewriter.func(_locationRewriter.data, location);
    }

    return location;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the result
  /// the caller has to delete the result object
  //////////////////////////////////////////////////////////////////////////////

  SimpleHttpResult* getResult();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the request
  ///
  /// @param method                         request method
  /// @param location                       request uri
  /// @param body                           request body
  /// @param bodyLength                     size of body
  /// @param headerFields                   list of header fields
  //////////////////////////////////////////////////////////////////////////////

  void setRequest(rest::RequestType method,
                  std::string const& location, char const* body,
                  size_t bodyLength,
                  std::unordered_map<std::string, std::string> const& headerFields);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process (a part of) the http header, the data is
  /// found in _readBuffer starting at _readBufferOffset until
  /// _readBuffer.length().
  //////////////////////////////////////////////////////////////////////////////

  void processHeader();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process (a part of) the body, read the http body by content length
  /// Note that when this is called, the content length of the body has always
  /// been set, either by finding a value in the HTTP header or by reading
  /// from the network until nothing more is found. The data is found in
  /// _readBuffer starting at _readBufferOffset until _readBuffer.length().
  //////////////////////////////////////////////////////////////////////////////

  void processBody();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process the chunk size of the next chunk (i.e. the chunk header),
  /// this is called when processing the body of a chunked transfer. The
  /// data is found in _readBuffer at position _readBufferOffset until
  /// _readBuffer.length().
  /// Note that this method and processChunkedBody() call each other when
  /// they complete, counting on the fact that in a single transfer the
  /// number of chunks found is not so large to run into deep recursion
  /// problems.
  //////////////////////////////////////////////////////////////////////////////

  void processChunkedHeader();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process the next chunk (i.e. the chunk body), this is called when
  /// processing the body of a chunked transfer. The data is found in
  /// _readBuffer at position _readBufferOffset until _readBuffer.length().
  /// Note that this method and processChunkedHeader() call each other when
  /// they complete, counting on the fact that in a single transfer the
  /// number of chunks found is not so large to run into deep recursion
  /// problems.
  //////////////////////////////////////////////////////////////////////////////

  void processChunkedBody();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief connection used (TCP or SSL connection)
  //////////////////////////////////////////////////////////////////////////////

  GeneralClientConnection* _connection;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief write buffer
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::StringBuffer _writeBuffer;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief read buffer
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::StringBuffer _readBuffer;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief read buffer offset
  ///
  /// _state == IN_READ_BODY:
  ///     points to the beginning of the body
  ///
  /// _state == IN_READ_HEADER:
  ///     points to the beginning of the next header line
  ///
  /// _state == FINISHED:
  ///     points to the beginning of the next request
  ///
  /// _state == IN_READ_CHUNKED_HEADER:
  ///     points to the beginning of the next size line
  ///
  /// _state == IN_READ_CHUNKED_BODY:
  ///     points to the beginning of the next body
  //////////////////////////////////////////////////////////////////////////////

  size_t _readBufferOffset;

  double _requestTimeout;

  request_state _state;

  size_t _written;

  std::string _errorMessage;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief struct for rewriting location URLs
  //////////////////////////////////////////////////////////////////////////////

  struct {
    void* data;
    std::string (*func)(void*, std::string const&);
  } _locationRewriter;

  uint32_t _nextChunkedSize;

  rest::RequestType _method;

  SimpleHttpResult* _result;

  std::vector<std::pair<std::string, std::string>> _pathToBasicAuth;

  size_t _maxPacketSize;

 public:
  size_t _maxRetries;

  uint64_t _retryWaitTime;

  std::string _retryMessage;

 private:
  // flag whether or not to delete the connection on destruction
  bool _deleteConnectionOnDestruction;

  // flag whether or not we keep the connection on destruction
  bool _keepConnectionOnDestruction;

  bool _warn;

  bool _keepAlive;

  bool _exposeArangoDB;

  bool _supportDeflate;

  // empty map, used for headers
  static std::unordered_map<std::string, std::string> const NO_HEADERS;

  // default value for max packet size
  static size_t MaxPacketSize;
};
}
}

#endif
