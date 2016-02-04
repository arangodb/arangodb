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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include <iostream>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangodump");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::GeneralClientConnection* Connection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP client
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::SimpleHttpClient* Client = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief max chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t MaxChunkSize = 1024 * 1024 * 12;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> Collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief include system collections
////////////////////////////////////////////////////////////////////////////////

static bool IncludeSystemCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief output directory
////////////////////////////////////////////////////////////////////////////////

static std::string OutputDirectory;

////////////////////////////////////////////////////////////////////////////////
/// @brief overwrite output directory
////////////////////////////////////////////////////////////////////////////////

static bool Overwrite = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief go on even in the face of errors
////////////////////////////////////////////////////////////////////////////////

static bool Force = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief save data
////////////////////////////////////////////////////////////////////////////////

static bool DumpData = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief first tick to be included in data dump
////////////////////////////////////////////////////////////////////////////////

static uint64_t TickStart = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief last tick to be included in data dump
////////////////////////////////////////////////////////////////////////////////

static uint64_t TickEnd = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief our batch id
////////////////////////////////////////////////////////////////////////////////

static uint64_t BatchId = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster mode flag
////////////////////////////////////////////////////////////////////////////////

static bool clusterMode = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics
////////////////////////////////////////////////////////////////////////////////

static struct {
  uint64_t _totalBatches;
  uint64_t _totalCollections;
  uint64_t _totalWritten;
} Stats;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions(int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description("collection", &Collections,
              "restrict to collection name (can be specified multiple times)")(
      "initial-batch-size", &ChunkSize,
      "initial size for individual data batches (in bytes)")(
      "batch-size", &MaxChunkSize,
      "maximum size for individual data batches (in bytes)")(
      "dump-data", &DumpData, "dump collection data")(
      "force", &Force,
      "continue dumping even in the face of some server-side errors")(
      "include-system-collections", &IncludeSystemCollections,
      "include system collections")("output-directory", &OutputDirectory,
                                    "output directory")(
      "overwrite", &Overwrite, "overwrite data in output directory")(
      "progress", &Progress, "show progress")(
      "tick-start", &TickStart, "only include data after this tick")(
      "tick-end", &TickEnd, "last tick to be included in data dump");

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  std::vector<std::string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "", argc, argv, "arangodump.conf");

  if (1 == arguments.size()) {
    OutputDirectory = arguments[0];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void LocalEntryFunction();
static void LocalExitFunction(int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
// .............................................................................

static void LocalEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(LocalExitFunction);
}

static void LocalExitFunction(int exitCode, void* data) {
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void LocalEntryFunction() {}

static void LocalExitFunction(int exitCode, void* data) {}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

static std::string GetHttpErrorMessage(SimpleHttpResult* result) {
  std::string details;
  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    std::string const& errorMessage =
        arangodb::basics::VelocyPackHelper::getStringValue(body, "errorMessage",
                                                           "");
    int errorNum = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        body, "errorNum", 0);

    if (errorMessage != "" && errorNum > 0) {
      details =
          ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
    }
  } catch (...) {
    // No action, fallthrough for error
  }
  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) + " (" +
         result->getHttpReturnMessage() + ")" + details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

static std::string GetArangoVersion() {
  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_GET, "/_api/version", nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return "";
  }

  std::string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();

      // look up "server" value
      std::string const server =
          arangodb::basics::VelocyPackHelper::getStringValue(body, "server",
                                                             "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = arangodb::basics::VelocyPackHelper::getStringValue(
            body, "version", "");
      }

    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
    }

    Connection->disconnect();
  }

  return version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if server is a coordinator of a cluster
////////////////////////////////////////////////////////////////////////////////

static bool GetArangoIsCluster() {
  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    return false;
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = arangodb::basics::VelocyPackHelper::getStringValue(body, "role",
                                                                "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
    }

    Connection->disconnect();
  }

  return role == "COORDINATOR";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a batch
////////////////////////////////////////////////////////////////////////////////

static int StartBatch(std::string DBserver, std::string& errorMsg) {
  std::string const url = "/_api/replication/batch";
  std::string const body = "{\"ttl\":300}";

  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_POST, url + urlExt, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (Force) {
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_INTERNAL;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON";
    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const resBody = parsedBody->slice();

  // look up "id" value
  std::string const id =
      arangodb::basics::VelocyPackHelper::getStringValue(resBody, "id", "");

  BatchId = StringUtils::uint64(id);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prolongs a batch
////////////////////////////////////////////////////////////////////////////////

static void ExtendBatch(std::string DBserver) {
  TRI_ASSERT(BatchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(BatchId);
  std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_PUT, url + urlExt, body.c_str(), body.size()));

  // ignore any return value
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a batch
////////////////////////////////////////////////////////////////////////////////

static void EndBatch(std::string DBserver) {
  TRI_ASSERT(BatchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(BatchId);
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  BatchId = 0;

  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_DELETE, url + urlExt, nullptr, 0));

  // ignore any return value
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single collection
////////////////////////////////////////////////////////////////////////////////

static int DumpCollection(int fd, std::string const& cid,
                          std::string const& name, uint64_t maxTick,
                          std::string& errorMsg) {
  uint64_t chunkSize = ChunkSize;

  std::string const baseUrl = "/_api/replication/dump?collection=" + cid +
                              "&ticks=false&translateIds=true&flush=false";

  uint64_t fromTick = TickStart;

  while (true) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick) +
                      "&chunkSize=" + StringUtils::itoa(chunkSize);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    if (Force) {
      url += "&failOnUnknown=false";
    } else {
      url += "&failOnUnknown=true";
    }

    Stats._totalBatches++;

    std::unique_ptr<SimpleHttpResult> response(
        Client->request(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg =
          "got invalid response from server: " + Client->getErrorMessage();

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = GetHttpErrorMessage(response.get());

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;  // just to please the compiler
    bool checkMore = false;
    bool found;

    // TODO: fix hard-coded headers
    std::string header =
        response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded",
                                          found);

        if (found) {
          uint64_t tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (!found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer const& body = response->getBody();

      if (!TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      } else {
        Stats._totalWritten += (uint64_t)body.length();
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      return res;
    }

    if (chunkSize < MaxChunkSize) {
      // adaptively increase chunksize
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);

      if (chunkSize > MaxChunkSize) {
        chunkSize = MaxChunkSize;
      }
    }
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a WAL flush request
////////////////////////////////////////////////////////////////////////////////

static void FlushWal() {
  std::string const url =
      "/_admin/wal/flush?waitForSync=true&waitForCollector=true";

  std::unique_ptr<SimpleHttpResult> response(
      Client->request(HttpRequest::HTTP_REQUEST_PUT, url, nullptr, 0));

  if (response == nullptr || !response->isComplete() ||
      response->wasHttpError()) {
    std::cerr << "got invalid response from server: " +
                     Client->getErrorMessage() << std::endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from server
////////////////////////////////////////////////////////////////////////////////

static int RunDump(std::string& errorMsg) {
  std::string const url =
      "/_api/replication/inventory?includeSystem=" +
      std::string(IncludeSystemCollections ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      Client->request(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
    return TRI_ERROR_INTERNAL;
  }

  FlushWal();
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const body = parsedBody->slice();

  if (!body.isObject()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  // read the server's max tick value
  std::string const tickString =
      arangodb::basics::VelocyPackHelper::getStringValue(body, "tick", "");

  if (tickString == "") {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  std::cout << "Last tick provided by server is: " << tickString << std::endl;

  uint64_t maxTick = StringUtils::uint64(tickString);
  // check if the user specified a max tick value
  if (TickEnd > 0 && maxTick > TickEnd) {
    maxTick = TickEnd;
  }

  try {
    VPackBuilder meta;
    meta.openObject();
    meta.add("database", VPackValue(BaseClient.databaseName()));
    meta.add("lastTickAtDumpStart", VPackValue(tickString));

    // save last tick in file
    std::string fileName =
        OutputDirectory + TRI_DIR_SEPARATOR_STR + "dump.json";

    int fd;

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                    S_IRUSR | S_IWUSR);

    if (fd < 0) {
      errorMsg = "cannot write to file '" + fileName + "'";

      return TRI_ERROR_CANNOT_WRITE_FILE;
    }
    meta.close();

    std::string const metaString = meta.slice().toJson();
    if (!TRI_WritePointer(fd, metaString.c_str(), metaString.size())) {
      TRI_CLOSE(fd);
      errorMsg = "cannot write to file '" + fileName + "'";

      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    TRI_CLOSE(fd);
  } catch (...) {
    errorMsg = "out of memory";

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(Collections[i], true));
  }

  // iterate over collections
  for (VPackSlice const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    std::string const cid = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "cid", "");
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        parameters, "type", 2);
    std::string const collectionType(type == 2 ? "document" : "edge");

    if (cid == "" || name == "") {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && !IncludeSystemCollections) {
      continue;
    }

    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    std::string const hexString(arangodb::rest::SslInterface::sslMD5(name));

    // found a collection!
    if (Progress) {
      std::cout << "# Dumping " << collectionType << " collection '" << name
                << "'..." << std::endl;
    }

    // now save the collection meta data and/or the actual data
    Stats._totalCollections++;

    {
      // save meta data
      std::string fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name +
                             "_" + hexString + ".structure.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(),
                      O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      std::string const collectionInfo = collection.toJson();

      if (!TRI_WritePointer(fd, collectionInfo.c_str(),
                            collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }

    if (DumpData) {
      // save the actual data
      std::string fileName;
      fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name + "_" +
                 hexString + ".data.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(),
                      O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      ExtendBatch("");
      int res = DumpCollection(fd, cid, name, maxTick, errorMsg);

      TRI_CLOSE(fd);

      if (res != TRI_ERROR_NO_ERROR) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }

        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single shard, that is a collection on a DBserver
////////////////////////////////////////////////////////////////////////////////

static int DumpShard(int fd, std::string const& DBserver,
                     std::string const& name, std::string& errorMsg) {
  std::string const baseUrl = "/_api/replication/dump?DBserver=" + DBserver +
                              "&collection=" + name + "&chunkSize=" +
                              StringUtils::itoa(ChunkSize) +
                              "&ticks=false&translateIds=true";

  uint64_t fromTick = 0;
  uint64_t maxTick = UINT64_MAX;

  while (1) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    Stats._totalBatches++;

    std::unique_ptr<SimpleHttpResult> response(
        Client->request(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg =
          "got invalid response from server: " + Client->getErrorMessage();

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = GetHttpErrorMessage(response.get());

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;  // just to please the compiler
    bool checkMore = false;
    bool found;

    // TODO: fix hard-coded headers
    std::string header =
        response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded",
                                          found);

        if (found) {
          uint64_t tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (!found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer const& body = response->getBody();

      if (!TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      } else {
        Stats._totalWritten += (uint64_t)body.length();
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      return res;
    }
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from cluster via a coordinator
////////////////////////////////////////////////////////////////////////////////

static int RunClusterDump(std::string& errorMsg) {
  int res;

  std::string const url =
      "/_api/replication/clusterInventory?includeSystem=" +
      std::string(IncludeSystemCollections ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      Client->request(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_INTERNAL;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const body = parsedBody->slice();

  if (!body.isObject()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(Collections[i], true));
  }

  // iterate over collections
  for (auto const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }
    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    std::string const id = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "id", "");
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);

    if (id == "" || name == "") {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && !IncludeSystemCollections) {
      continue;
    }

    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    // found a collection!
    if (Progress) {
      std::cout << "# Dumping collection '" << name << "'..." << std::endl;
    }

    // now save the collection meta data and/or the actual data
    Stats._totalCollections++;

    {
      // save meta data
      std::string fileName =
          OutputDirectory + TRI_DIR_SEPARATOR_STR + name + ".structure.json";

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      int fd = TRI_CREATE(fileName.c_str(),
                          O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      std::string const collectionInfo = collection.toJson();

      if (!TRI_WritePointer(fd, collectionInfo.c_str(),
                            collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }

    if (DumpData) {
      // save the actual data

      // Now set up the output file:
      std::string const hexString(arangodb::rest::SslInterface::sslMD5(name));
      std::string fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name +
                             "_" + hexString + ".data.json";

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      int fd = TRI_CREATE(fileName.c_str(),
                          O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      // First we have to go through all the shards, what are they?
      VPackSlice const shards = parameters.get("shards");

      // Iterate over the Map of shardId to server list
      for (auto const it : VPackObjectIterator(shards)) {
        TRI_ASSERT(it.key.isString());
        TRI_ASSERT(it.value.isArray());
        TRI_ASSERT(it.value[0].isString());
        std::string shardName = it.key.copyString();
        std::string DBserver = it.value[0].copyString();

        if (Progress) {
          std::cout << "# Dumping shard '" << shardName << "' from DBserver '"
                    << DBserver << "' ..." << std::endl;
        }
        res = StartBatch(DBserver, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_CLOSE(fd);
          return res;
        }
        res = DumpShard(fd, DBserver, shardName, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_CLOSE(fd);
          return res;
        }
        EndBatch(DBserver);
      }

      res = TRI_CLOSE(fd);

      if (res != TRI_ERROR_NO_ERROR) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }

        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

static std::string rewriteLocation(void* data, std::string const& location) {
  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + BaseClient.databaseName() + location;
  } else {
    return "/_db/" + BaseClient.databaseName() + "/" + location;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  LocalEntryFunction();

  TRIAGENS_C_INITIALIZE(argc, argv);
  TRIAGENS_REST_INITIALIZE(argc, argv);

  Logger::initialize(false);

  // .............................................................................
  // set defaults
  // .............................................................................

  int err = 0;
  OutputDirectory = FileUtils::currentDirectory(&err)
                        .append(TRI_DIR_SEPARATOR_STR)
                        .append("dump");
  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // use a minimum value for batches
  if (ChunkSize < 1024 * 128) {
    ChunkSize = 1024 * 128;
  }
  if (MaxChunkSize < ChunkSize) {
    MaxChunkSize = ChunkSize;
  }

  if (TickStart < TickEnd) {
    std::cerr << "Error: invalid values for --tick-start or --tick-end"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (!OutputDirectory.empty() &&
      OutputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(OutputDirectory.size() > 0);
    OutputDirectory.pop_back();
  }

  // .............................................................................
  // create output directory
  // .............................................................................

  bool isDirectory = false;
  bool isEmptyDirectory = false;

  if (!OutputDirectory.empty()) {
    isDirectory = TRI_IsDirectory(OutputDirectory.c_str());

    if (isDirectory) {
      TRI_vector_string_t files =
          TRI_FullTreeDirectory(OutputDirectory.c_str());
      // we don't care if the target directory is empty
      isEmptyDirectory = (files._length == 0);
      TRI_DestroyVectorString(&files);
    }
  }

  if (OutputDirectory.empty() ||
      (TRI_ExistsFile(OutputDirectory.c_str()) && !isDirectory)) {
    std::cerr << "Error: cannot write to output directory '" << OutputDirectory
              << "'" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (isDirectory && !isEmptyDirectory && !Overwrite) {
    std::cerr
        << "Error: output directory '" << OutputDirectory
        << "' already exists. use \"--overwrite true\" to overwrite data in it"
        << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    std::cerr << "invalid value for --server.endpoint ('"
              << BaseClient.endpointString() << "')" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Connection = GeneralClientConnection::factory(
      BaseClient.endpointServer(), BaseClient.requestTimeout(),
      BaseClient.connectTimeout(), ArangoClient::DEFAULT_RETRIES,
      BaseClient.sslProtocol());

  if (Connection == nullptr) {
    std::cerr << "out of memory" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  Client->setLocationRewriter(0, &rewriteLocation);
  Client->setUserNamePassword("/", BaseClient.username(),
                              BaseClient.password());

  std::string const versionString = GetArangoVersion();

  if (!Connection->isConnected()) {
    std::cerr << "Could not connect to endpoint '"
              << BaseClient.endpointString() << "', database: '"
              << BaseClient.databaseName() << "', username: '"
              << BaseClient.username() << "'" << std::endl;
    std::cerr << "Error message: '" << Client->getErrorMessage() << "'"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  int major = 0;
  int minor = 0;

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    std::cerr << "Error: invalid server version '" << versionString << "'"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (major < 1 || major > 2 || (major == 1 && minor < 4)) {
    // we can connect to 1.4, 2.0 and higher only
    std::cerr << "Error: got incompatible server version '" << versionString
              << "'" << std::endl;
    if (!Force) {
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    clusterMode = GetArangoIsCluster();
    if (clusterMode) {
      if (TickStart != 0 || TickEnd != 0) {
        std::cerr << "Error: cannot use tick-start or tick-end on a cluster"
                  << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }
    }
  }

  if (!Connection->isConnected()) {
    std::cerr << "Lost connection to endpoint '" << BaseClient.endpointString()
              << "', database: '" << BaseClient.databaseName()
              << "', username: '" << BaseClient.username() << "'" << std::endl;
    std::cerr << "Error message: '" << Client->getErrorMessage() << "'"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (!isDirectory) {
    long systemError;
    std::string errorMessage;
    int res =
        TRI_CreateDirectory(OutputDirectory.c_str(), systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      std::cerr << "Error: unable to create output directory '"
                << OutputDirectory << "': " << errorMessage << std::endl;
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (Progress) {
    std::cout << "Connected to ArangoDB '" << BaseClient.endpointString()
              << "', database: '" << BaseClient.databaseName()
              << "', username: '" << BaseClient.username() << "'" << std::endl;

    std::cout << "Writing dump to output directory '" << OutputDirectory << "'"
              << std::endl;
  }

  memset(&Stats, 0, sizeof(Stats));

  std::string errorMsg = "";

  int res;

  try {
    if (!clusterMode) {
      res = StartBatch("", errorMsg);
      if (res != TRI_ERROR_NO_ERROR && Force) {
        res = TRI_ERROR_NO_ERROR;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        res = RunDump(errorMsg);
      }

      if (BatchId > 0) {
        EndBatch("");
      }
    } else {  // clusterMode == true
      res = RunClusterDump(errorMsg);
    }
  } catch (std::exception const& ex) {
    std::cerr << "Error: caught exception " << ex.what() << std::endl;
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    std::cerr << "Error: caught unknown exception" << std::endl;
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      std::cerr << "Error: " << errorMsg << std::endl;
    } else {
      std::cerr << "An error occurred" << std::endl;
    }
    ret = EXIT_FAILURE;
  }

  if (Progress) {
    if (DumpData) {
      std::cout << "Processed " << Stats._totalCollections << " collection(s), "
                << "wrote " << Stats._totalWritten
                << " byte(s) into datafiles, "
                << "sent " << Stats._totalBatches << " batch(es)" << std::endl;
    } else {
      std::cout << "Processed " << Stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }

  delete Client;

  TRIAGENS_REST_SHUTDOWN;

  LocalExitFunction(ret, nullptr);

  return ret;
}
