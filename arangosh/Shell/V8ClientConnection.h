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

#ifndef ARANGODB_SHELL_V8CLIENT_CONNECTION_H
#define ARANGODB_SHELL_V8CLIENT_CONNECTION_H 1

#include "Basics/Common.h"
#include "Basics/StringRef.h"

#include <fuerte/connection.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <v8.h>

namespace arangodb {
class ClientFeature;

namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

namespace fuerte {
inline namespace v1 { class Connection; }
}  // namespace fuerte

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

class V8ClientConnection {
  V8ClientConnection(V8ClientConnection const&) = delete;
  V8ClientConnection& operator=(V8ClientConnection const&) = delete;

 public:
  V8ClientConnection();
  ~V8ClientConnection();

 public:
  void setInterrupted(bool interrupted);
  bool isConnected();

  void connect(ClientFeature*);
  void reconnect(ClientFeature*);

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& value) { _databaseName = value; }
  std::string username() const { return _builder.user(); }
  std::string password() const { return _builder.password(); }
  int lastHttpReturnCode() const { return _lastHttpReturnCode; }
  std::string lastErrorMessage() const { return _lastErrorMessage; }
  std::string const& version() const { return _version; }
  std::string const& mode() const { return _mode; }
  std::string endpointSpecification() const;

  v8::Handle<v8::Value> getData(v8::Isolate* isolate, StringRef const& location,
                                std::unordered_map<std::string, std::string> const& headerFields,
                                bool raw);

  v8::Handle<v8::Value> headData(v8::Isolate* isolate, StringRef const& location,
                                 std::unordered_map<std::string, std::string> const& headerFields,
                                 bool raw);

  v8::Handle<v8::Value> deleteData(v8::Isolate* isolate, StringRef const& location,
                                   v8::Local<v8::Value> const& body,
                                   std::unordered_map<std::string, std::string> const& headerFields,
                                   bool raw);

  v8::Handle<v8::Value> optionsData(v8::Isolate* isolate, StringRef const& location,
                                    v8::Local<v8::Value> const& body,
                                    std::unordered_map<std::string, std::string> const& headerFields,
                                    bool raw);

  v8::Handle<v8::Value> postData(v8::Isolate* isolate, StringRef const& location,
                                 v8::Local<v8::Value> const& body,
                                 std::unordered_map<std::string, std::string> const& headerFields,
                                 bool raw = false, bool isFile = false);

  v8::Handle<v8::Value> putData(v8::Isolate* isolate, StringRef const& location,
                                v8::Local<v8::Value> const& body,
                                std::unordered_map<std::string, std::string> const& headerFields,
                                bool raw);

  v8::Handle<v8::Value> patchData(v8::Isolate* isolate, StringRef const& location,
                                  v8::Local<v8::Value> const& body,
                                  std::unordered_map<std::string, std::string> const& headerFields,
                                  bool raw);

  void initServer(v8::Isolate*, v8::Handle<v8::Context> context, ClientFeature*);

 private:
  void createConnection();

  v8::Local<v8::Value> requestData(v8::Isolate* isolate, fuerte::RestVerb verb,
                                   StringRef const& location,
                                   v8::Local<v8::Value> const& body,
                                   std::unordered_map<std::string, std::string> const& headerFields,
                                   bool isFile = false);

  v8::Local<v8::Value> requestDataRaw(v8::Isolate* isolate, fuerte::RestVerb verb,
                                      StringRef const& location,
                                      v8::Local<v8::Value> const& body,
                                      std::unordered_map<std::string, std::string> const& headerFields);

  v8::Local<v8::Value> handleResult(v8::Isolate* isolate,
                                    std::unique_ptr<fuerte::Response> response,
                                    fuerte::ErrorCondition ec);

  /// @brief shuts down the connection _connection and resets the pointer
  /// to a nullptr
  void shutdownConnection();

 private:
  std::string _databaseName;
  std::chrono::duration<double> _requestTimeout;

  std::mutex _lock;
  int _lastHttpReturnCode;
  std::string _lastErrorMessage;
  std::string _version;
  std::string _mode;

  fuerte::EventLoopService _loop;
  fuerte::ConnectionBuilder _builder;
  std::shared_ptr<fuerte::Connection> _connection;
  velocypack::Options _vpackOptions;
};
}  // namespace arangodb

#endif
