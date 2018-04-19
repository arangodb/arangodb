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

#include "AgencyMock.h"
#include "Agency/Store.h"
#include "lib/Rest/HttpResponse.h"

#include <velocypack/velocypack-aliases.h>

// -----------------------------------------------------------------------------
// --SECTION--                                 GeneralClientConnectionAgencyMock
// -----------------------------------------------------------------------------

void GeneralClientConnectionAgencyMock::handleRead(
    arangodb::basics::StringBuffer& buffer
) {
  auto const query = arangodb::velocypack::Parser::fromJson(_body);

  auto result = std::make_shared<arangodb::velocypack::Builder>();
  auto const success = _store->read(query, result);
  auto const code = std::find(success.begin(), success.end(), false) == success.end()
    ? arangodb::rest::ResponseCode::OK
    : arangodb::rest::ResponseCode::BAD;

  arangodb::HttpResponse resp(code);

  std::string body;
  if (arangodb::rest::ResponseCode::OK == code && !result->isEmpty()) {
    body = result->slice().toString();
    resp.setContentType(arangodb::ContentType::VPACK);
    resp.headResponse(body.size());
  }

  resp.writeHeader(&buffer);

  if (!body.empty()) {
    buffer.appendText(body);
  }
}

void GeneralClientConnectionAgencyMock::handleWrite(
    arangodb::basics::StringBuffer& buffer
) {
  auto const query = arangodb::velocypack::Parser::fromJson(_body);

  auto const success = _store->applyTransactions(query);
  auto const code = std::find(success.begin(), success.end(), false) == success.end()
    ? arangodb::rest::ResponseCode::OK
    : arangodb::rest::ResponseCode::BAD;

  VPackBuilder bodyObj;
  bodyObj.openObject();
  bodyObj.add("results", VPackValue(VPackValueType::Array));
  bodyObj.close();
  bodyObj.close();
  auto body = bodyObj.slice().toString();

  arangodb::HttpResponse resp(code);
  resp.setContentType(arangodb::ContentType::VPACK);
  resp.headResponse(body.size());

  resp.writeHeader(&buffer);
  buffer.appendText(body);
}

void GeneralClientConnectionAgencyMock::getValue(
    arangodb::basics::StringBuffer& buffer
) {
  if (_path.size() != 4) {
    // invalid message format
    throw std::exception();
  }

  auto const& op = action();

  if (op == "write") {
    handleWrite(buffer);
  } else if (op == "read") {
    handleRead(buffer);
  }
}

void GeneralClientConnectionAgencyMock::setKey(char const* data, size_t length) {
  static const std::string bodyDelimiter("\r\n\r\n");

  std::string request(data, length);

  auto pos = request.find("\r\n");

  if (pos == std::string::npos) {
    return; // use full string as key
  }

  // <HTTP-method> <path> HTTP/1.1
  auto const requestLineParts = arangodb::basics::StringUtils::split(
    std::string(data, pos), ' '
  );

  if (3 != requestLineParts.size()
      || requestLineParts[0] != "POST" // agency works with POST requests only
      || requestLineParts[2] != "HTTP/1.1") {
    // invalid message format
    throw std::exception();
  }

  pos = request.find(bodyDelimiter, pos);

  if (pos == std::string::npos) {
    return; // use full string as key
  }

  _url = requestLineParts[1];
  _path = arangodb::basics::StringUtils::split(_url, '/');
  _body.assign(data + pos + bodyDelimiter.size());
}

bool initializeAgencyStore(arangodb::consensus::Store& store) {
  store.clear();

  auto addEmptyVPackObject = [](
      std::string const& name,
      VPackBuilder& builder
  ) {
    builder.add(VPackValue(name));
    VPackObjectBuilder c(&builder);
  };

  VPackBuilder builder;

  {
    VPackObjectBuilder b(&builder);

    builder.add(                       // Cluster Id --------------------------
      "Cluster", VPackValue(to_string(boost::uuids::random_generator()())));

    builder.add(VPackValue("Agency")); // Agency ------------------------------
    {
      VPackObjectBuilder a(&builder);
      builder.add("Definition", VPackValue(1));
    }

    builder.add(VPackValue("Current")); // Current ----------------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("AsyncReplication", builder);
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add("Version", VPackValue(1));
      addEmptyVPackObject("ShardsCopied", builder);
      addEmptyVPackObject("NewServers", builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      addEmptyVPackObject("Singles", builder);
      builder.add(VPackValue("ServersRegistered"));
      {
        VPackObjectBuilder c(&builder);
        builder.add("Version", VPackValue(1));
      }
      addEmptyVPackObject("Databases", builder);
    }

    builder.add("InitDone", VPackValue(true)); // InitDone

    builder.add(VPackValue("Plan")); // Plan ----------------------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("AsyncReplication", builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add(VPackValue("Databases"));
      {
        VPackObjectBuilder d(&builder);
        builder.add(VPackValue("_system"));
        {
          VPackObjectBuilder d2(&builder);
          builder.add("name", VPackValue("_system"));
          builder.add("id", VPackValue("1"));
        }
      }
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      addEmptyVPackObject("Singles", builder);
      builder.add("Version", VPackValue(1));
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add(VPackValue("Views"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
    }

    builder.add(VPackValue("Sync")); // Sync ----------------------------------
    {
      VPackObjectBuilder c(&builder);
      builder.add("LatestID", VPackValue(1));
      addEmptyVPackObject("Problems", builder);
      builder.add("UserVersion", VPackValue(1));
      addEmptyVPackObject("ServerStates", builder);
      builder.add("HeartbeatIntervalMs", VPackValue(1000));
      addEmptyVPackObject("Commands", builder);
    }

    builder.add(VPackValue("Supervision")); // Supervision --------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("Health", builder);
      addEmptyVPackObject("Shards", builder);
      addEmptyVPackObject("DBServers", builder);
    }

    builder.add(VPackValue("Target")); // Target ------------------------------
    {
      VPackObjectBuilder c(&builder);
      builder.add("NumberOfCoordinators", VPackSlice::nullSlice());
      builder.add("NumberOfDBServers", VPackSlice::nullSlice());
      builder.add(VPackValue("CleanedServers"));
      { VPackArrayBuilder dd(&builder); }
      builder.add(VPackValue("FailedServers"));
      { VPackObjectBuilder dd(&builder); }
      builder.add("Lock", VPackValue("UNLOCKED"));
      // MapLocalToID is not used for anything since 3.4. It was used in previous
      // versions to store server ids from --cluster.my-local-info that were mapped
      // to server UUIDs
      addEmptyVPackObject("MapLocalToID", builder);
      addEmptyVPackObject("Failed", builder);
      addEmptyVPackObject("Finished", builder);
      addEmptyVPackObject("Pending", builder);
      addEmptyVPackObject("ToDo", builder);
      builder.add("Version", VPackValue(1));
    }
  }

  arangodb::AgencyOperation initOperation(
    "", arangodb::AgencyValueOperationType::SET, builder.slice()
  );

  arangodb::AgencyWriteTransaction initTransaction;
  initTransaction.operations.push_back(initOperation);

  VPackBuilder transactionBuilder;
  initTransaction.toVelocyPack(transactionBuilder);

  return store.applyTransaction(transactionBuilder.slice()).success;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
