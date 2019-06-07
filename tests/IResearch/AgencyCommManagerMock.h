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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_AGENCY_COMM_MANAGER_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_AGENCY_COMM_MANAGER_MOCK_H 1

#include <map>
#include "utils/file_utils.hpp"
#include "Agency/AgencyComm.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief mock of AgencyCommManager for use with tests
////////////////////////////////////////////////////////////////////////////////
class AgencyCommManagerMock: public arangodb::AgencyCommManager {
public:
 explicit AgencyCommManagerMock(std::string const& prefix = "");

 void addConnection(
   std::unique_ptr<arangodb::httpclient::GeneralClientConnection>&& connection
 );

 template <typename T, typename... Args>
 T* addConnection(Args&&... args) {
   auto* con = new T(std::forward<Args>(args)...);

   addConnection(
     std::unique_ptr<arangodb::httpclient::GeneralClientConnection>(con)
   );

   return con;
 }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief mock of Endpoint for use with GeneralClientConnectionMock
////////////////////////////////////////////////////////////////////////////////
class EndpointMock: public arangodb::Endpoint {
 public:
  EndpointMock();

  virtual TRI_socket_t connect(
    double connectTimeout, double requestTimeout
  ) override;
  virtual void disconnect() override;
  virtual int domain() const override;
  virtual std::string host() const override;
  virtual std::string hostAndPort() const override;
  virtual bool initIncoming(TRI_socket_t socket) override;
  virtual int port() const override;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mock of GeneralClientConnection for use with AgencyCommManagerMock
////////////////////////////////////////////////////////////////////////////////
class GeneralClientConnectionMock
  : public arangodb::httpclient::GeneralClientConnection {
 public:
  EndpointMock endpoint;

  #ifndef _WIN32
    irs::file_utils::handle_t nil;
  #endif

  GeneralClientConnectionMock();
  ~GeneralClientConnectionMock();
  virtual bool connectSocket() override;
  virtual void disconnectSocket() override;
  virtual bool readable() override;
  virtual bool readClientConnection(
    arangodb::basics::StringBuffer& buffer, bool& connectionClosed
  ) override;
  virtual bool writeClientConnection(
    void const* buffer, size_t length, size_t* bytesWritten
  ) override;

 protected:
  virtual void request(char const* data, size_t length); // override by specializations
  virtual void response(arangodb::basics::StringBuffer& buffer); // override by specializations
};

////////////////////////////////////////////////////////////////////////////////
/// @brief specialization of GeneralClientConnectionMock returning results from
///        a list
////////////////////////////////////////////////////////////////////////////////
class GeneralClientConnectionListMock: public GeneralClientConnectionMock {
 public:
  std::deque<std::string> responses;

  virtual void response(arangodb::basics::StringBuffer& buffer) override;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief specialization of GeneralClientConnectionMock returning results from
///        a map keyed by request
////////////////////////////////////////////////////////////////////////////////
class GeneralClientConnectionMapMock: public GeneralClientConnectionMock {
 public:
  std::string lastKey;
  std::map<std::string, std::string> responses;

  virtual void request(char const* data, size_t length) override;
  virtual void response(arangodb::basics::StringBuffer& buffer) override;
};

#endif
