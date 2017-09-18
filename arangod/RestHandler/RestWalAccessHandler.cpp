////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RestWalAccessHandler.h"

#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/WalAccess.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Rest/HttpResponse.h"

#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"

#include "VocBase/replication-common.h"

#include <velocypack/Iterator.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestWalAccessHandler::RestWalAccessHandler(GeneralRequest* request,
                                      GeneralResponse* response)
  : RestBaseHandler(request, response) {}

RestStatus RestWalAccessHandler::execute() {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET _api/wal/[tail|range|lastTick]>");
    return RestStatus::DONE;;
  }
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
  WalAccess const* wal = engine->walAccess();
  TRI_ASSERT(wal != nullptr);

  
  if (suffixes[0] == "range") {
    std::pair<TRI_voc_tick_t, TRI_voc_tick_t> minMax;
    Result r = wal->tickRange(minMax);
    if (!r.ok()) {
      /// {"tickMin":"123", "tickMax":"456", "version":"3.2", "serverId":"abc"}
      VPackBuilder result;
      result.openObject();
      result.add("time", VPackValue(utilities::timeString()));
      // "state" part
      result.add("tickMin", VPackValue(std::to_string(minMax.first)));
      result.add("tickMax", VPackValue(std::to_string(minMax.second)));
      { // "server" part
        VPackObjectBuilder server(&result, "server", true);
        server->add("version", VPackValue(ARANGODB_VERSION));
        server->add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
      }
      result.close();
      generateResult(rest::ResponseCode::OK, result.slice());
    } else {
      generateError(r);
    }
  } else if (suffixes[0] == "lastTick") {
    VPackBuilder result;
    result.openObject();
    result.add("time", VPackValue(utilities::timeString()));
    // "state" part
    result.add("tick", VPackValue(std::to_string(wal->lastTick())));
    { // "server" part
      VPackObjectBuilder server(&result, "server", true);
      server->add("version", VPackValue(ARANGODB_VERSION));
      server->add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
    }
    result.close();
    generateResult(rest::ResponseCode::OK, result.slice());
    
  } else if (suffixes[0] == "tail") {
    
    bool useVst = false;
    if (_request->transportType() == Endpoint::TransportType::VST) {
      useVst = true;
    }
    
    // determine start and end tick
    TRI_voc_tick_t tickStart = 0;
    TRI_voc_tick_t tickEnd = UINT64_MAX;
    
    bool found;
    std::string const& value1 = _request->value("from", found);
    
    if (found) {
      tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
    }
    
    // determine end tick for dump
    std::string const& value2 = _request->value("to", found);
    
    if (found) {
      tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
    }
    
    if (found && (tickStart > tickEnd || tickEnd == 0)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid from/to values");
      return RestStatus::DONE;
    }
    
    bool includeSystem = true;
    std::string const& value4 = _request->value("includeSystem", found);
    
    if (found) {
      includeSystem = StringUtils::boolean(value4);
    }
    
    size_t chunkSize = 1024 * 1024;  // TODO: determine good default value?
    std::string const& value5 = _request->value("chunkSize", found);
    if (found) {
      chunkSize = static_cast<size_t>(StringUtils::uint64(value5));
    }
    
    // extract collection filer
    /*TRI_voc_cid_t cid = 0;
    std::string const& value6 = _request->value("collection", found);
    if (found) {
      arangodb::LogicalCollection* c = _vocbase->lookupCollection(value6);
      
      if (c == nullptr) {
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
        return;
      }
      
      cid = c->cid();
    }*/
    // FIXME
    WalAccess::WalFilter filter;
    
    TRI_vocbase_t *vocbase;
    
    WalTailingResult result;
    VPackBuffer<uint8_t> vpackbuffer;
    auto trxContext = transaction::StandaloneContext::Create(vocbase);
    VPackBuilder builder(vpackbuffer, trxContext->getVPackOptions());
    if (useVst) {
      result = wal->tail(tickStart, tickEnd, chunkSize, includeSystem, filter, builder);
    } else {
      builder.openArray();
      result = wal->tail(tickStart, tickEnd, chunkSize,
                         includeSystem, filter, builder);
      builder.close();
    }
    
    if (result.fail()) {
      generateError(result);
      return RestStatus::DONE;
    }
    
    TRI_voc_tick_t const latest = wal->lastTick();
    bool const checkMore = (result.maxTick() > 0 && result.maxTick() < latest);
    size_t length = vpackbuffer.length();
    // transfer ownership of the buffer contents
    _response->setContentType(rest::ContentType::DUMP);
    
    // set headers
    _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                           checkMore ? "true" : "false");
    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                           StringUtils::itoa((length == 0) ? 0 : result.maxTick()));
    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                           StringUtils::itoa(latest));
    _response->setHeaderNC(TRI_REPLICATION_HEADER_ACTIVE, "true");
    _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                           result.minTick() <= tickStart ? "true" : "false");
    
    if (length > 0) {
      resetResponse(rest::ResponseCode::OK);
      if (useVst) {
        _response->addPayload(std::move(vpackbuffer),
                              trxContext->getVPackOptions(), false);
      } else {
        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
        if (httpResponse == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "invalid response type");
        }
        
        basics::StringBuffer& buffer = httpResponse->body();
        basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
        // note: we need the CustomTypeHandler here
        VPackDumper dumper(&adapter, trxContext->getVPackOptions());
        for (auto marker : VPackArrayIterator(builder.slice())) {
          dumper.dump(marker);
          buffer.appendChar('\n');
        }
      }
      // add client
      bool found;
      std::string const& value = _request->value("serverId", found);
      
      TRI_server_id_t serverId = 0;
      if (found) {
        serverId = (TRI_server_id_t)StringUtils::uint64(value);
      }
      vocbase->updateReplicationClient(serverId, result.maxTick());
    }
  } else {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  }
  
  return RestStatus::DONE;
}
