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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AGENCY_APPLICATION_AGENCY_H
#define ARANGOD_AGENCY_APPLICATION_AGENCY_H 1

#include "Basics/Common.h"

#include "HttpServer/ApplicationEndpointServer.h"

#include "ApplicationServer/ApplicationFeature.h"
#include "Agency/Agent.h"

struct TRI_server_t;

namespace arangodb {
class ApplicationV8;
namespace aql {
class QueryRegistry;
}
namespace rest {
class Task;


////////////////////////////////////////////////////////////////////////////////
/// @brief application server with agency
////////////////////////////////////////////////////////////////////////////////
using agent_t = consensus::Agent;

  
class ApplicationAgency : virtual public arangodb::rest::ApplicationFeature {
 private:
  ApplicationAgency(ApplicationAgency const&);
  ApplicationAgency& operator=(ApplicationAgency const&);

  
 public:

  ApplicationAgency(TRI_server_t*, ApplicationEndpointServer*,
                    ApplicationV8* applicationV8, 
                    aql::QueryRegistry* queryRegistry);

  ~ApplicationAgency();

  
 public:

  /// @brief builds the dispatcher queue
  void buildStandardQueue(size_t nrThreads, size_t maxSize);

  /// @brief builds the additional AQL dispatcher queue
  void buildAQLQueue(size_t nrThreads, size_t maxSize);

  /// @brief builds an additional dispatcher queue
  void buildExtraQueue(size_t name, size_t nrThreads, size_t maxSize);

  /// @brief returns the number of used threads
  size_t size();

  /// @brief sets the processor affinity
  void setProcessorAffinity(std::vector<size_t> const& cores);

  /// @brief stop feature
  void stop () override;

  
 public:

  /// @brief setup options
  void setupOptions(std::map<std::string,
                    arangodb::basics::ProgramOptionsDescription>&) override final;

  bool prepare() override final;
  bool start() override final;
  bool open() override final;
  void close() override final;

  agent_t* agent() const;
  
 private:

  TRI_server_t* _server;

  uint64_t _size; /**< @brief: agency size (default: 5)*/
  double   _min_election_timeout; /**< @brief: min election timeout */
  double   _max_election_timeout; /**< @brief: max election timeout */
  double   _election_call_rate_mul; /**< @brief: */
  bool     _notify;
  bool     _sanity_check;
               /**< @brief interval between retry to slaves*/
  std::vector<std::string> _agency_endpoints; /**< @brief agency adresses */
  std::unique_ptr<agent_t> _agent;
  
  uint32_t _agent_id;

  ApplicationEndpointServer* _endpointServer;
  ApplicationV8* _applicationV8;
  aql::QueryRegistry* _queryRegistry;

};
}
}

#endif


