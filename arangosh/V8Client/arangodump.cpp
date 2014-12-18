////////////////////////////////////////////////////////////////////////////////
/// @brief arango dump tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/terminal-utils.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"


using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangodump");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::GeneralClientConnection* Connection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP client
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::SimpleHttpClient* Client = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections
////////////////////////////////////////////////////////////////////////////////

static vector<string> Collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief include system collections
////////////////////////////////////////////////////////////////////////////////

static bool IncludeSystemCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief output directory
////////////////////////////////////////////////////////////////////////////////

static string OutputDirectory;

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
}
Stats;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description
    ("collection", &Collections, "restrict to collection name (can be specified multiple times)")
    ("batch-size", &ChunkSize, "maximum size for individual data batches (in bytes)")
    ("dump-data", &DumpData, "dump collection data")
    ("force", &Force, "continue dumping even in the face of some server-side errors")
    ("include-system-collections", &IncludeSystemCollections, "include system collections")
    ("output-directory", &OutputDirectory, "output directory")
    ("overwrite", &Overwrite, "overwrite data in output directory")
    ("progress", &Progress, "show progress")
    ("tick-start", &TickStart, "only include data after this tick")
    ("tick-end", &TickEnd, "last tick to be included in data dump")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "", argc, argv, "arangodump.conf");

  if (1 == arguments.size()) {
    OutputDirectory = arguments[0];
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void arangodumpEntryFunction ();
static void arangodumpExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialisations for windows only
// .............................................................................

void arangodumpEntryFunction () {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangodumpExitFunction);
}

static void arangodumpExitFunction (int exitCode, void* data) {
  int res = 0;
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangodumpEntryFunction () {
}

static void arangodumpExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

static string GetHttpErrorMessage (SimpleHttpResult* result) {
  const StringBuffer& body = result->getBody();
  string details;

  TRI_json_t* json = JsonHelper::fromString(body.c_str(), body.length());

  if (json != 0) {
    const string& errorMessage = JsonHelper::getStringValue(json, "errorMessage", "");
    const int errorNum = JsonHelper::getNumericValue<int>(json, "errorNum", 0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    if (errorMessage != "" && errorNum > 0) {
      details = ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
    }
  }

  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) +
         " (" + result->getHttpReturnMessage() + ")" +
         details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

static string GetArangoVersion () {
  map<string, string> headers;

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                               "/_api/version",
                                               0,
                                               0,
                                               headers);

  if (response == 0 || ! response->isComplete()) {
    if (response != 0) {
      delete response;
    }

    return "";
  }

  string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";

    // convert response body to json
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                      response->getBody().c_str());

    if (json != 0) {
      // look up "server" value
      const string server = JsonHelper::getStringValue(json, "server", "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = JsonHelper::getStringValue(json, "version", "");
      }

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }
  else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response), false);
    }

    Connection->disconnect();
  }

  delete response;

  return version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if server is a coordinator of a cluster
////////////////////////////////////////////////////////////////////////////////

static bool GetArangoIsCluster () {
  map<string, string> headers;
  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                        "/_admin/server/role",
                                        "",
                                        0,
                                        headers);

  if (response == 0 || ! response->isComplete()) {
    if (response != 0) {
      delete response;
    }

    return false;
  }

  string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // convert response body to json
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                      response->getBody().c_str());

    if (json != 0) {
      // look up "server" value
      role = JsonHelper::getStringValue(json, "role", "UNDEFINED");

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }
  else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response), false);
    }

    Connection->disconnect();
  }

  delete response;

  return role == "COORDINATOR";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a batch
////////////////////////////////////////////////////////////////////////////////

static int StartBatch (string DBserver, string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/batch";
  const string body = "{\"ttl\":300}";
  string urlExt;
  if (! DBserver.empty()) {
    urlExt = "?DBserver="+DBserver;
  }

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_POST,
                                               url + urlExt,
                                               body.c_str(),
                                               body.size(),
                                               headers);

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

    if (Force) {
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  // convert response body to json
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str());
  delete response;

  if (json == 0) {
    errorMsg = "got malformed JSON";

    return TRI_ERROR_INTERNAL;
  }

  // look up "id" value
  const string id = JsonHelper::getStringValue(json, "id", "");

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  BatchId = StringUtils::uint64(id);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prolongs a batch
////////////////////////////////////////////////////////////////////////////////

static void ExtendBatch (string DBserver) {
  TRI_ASSERT(BatchId > 0);

  map<string, string> headers;
  const string url = "/_api/replication/batch/" + StringUtils::itoa(BatchId);
  const string body = "{\"ttl\":300}";
  string urlExt;
  if (! DBserver.empty()) {
    urlExt = "?DBserver="+DBserver;
  }

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url + urlExt,
                                               body.c_str(),
                                               body.size(),
                                               headers);

  // ignore any return value
  if (response != 0) {
    delete response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a batch
////////////////////////////////////////////////////////////////////////////////

static void EndBatch (string DBserver) {
  TRI_ASSERT(BatchId > 0);

  map<string, string> headers;
  const string url = "/_api/replication/batch/" + StringUtils::itoa(BatchId);
  string urlExt;
  if (! DBserver.empty()) {
    urlExt = "?DBserver="+DBserver;
  }

  BatchId = 0;

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_DELETE,
                                               url + urlExt,
                                               0,
                                               0,
                                               headers);

  // ignore any return value
  if (response != 0) {
    delete response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single collection
////////////////////////////////////////////////////////////////////////////////

static int DumpCollection (int fd,
                           const string& cid,
                           const string& name,
                           TRI_json_t const* parameters,
                           const uint64_t maxTick,
                           string& errorMsg) {

  const string baseUrl = "/_api/replication/dump?collection=" + cid +
                         "&chunkSize=" + StringUtils::itoa(ChunkSize) +
                         "&ticks=false&translateIds=true&flush=false";

  map<string, string> headers;

  uint64_t fromTick = TickStart;

  while (1) {
    string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    Stats._totalBatches++;

    SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                                 url,
                                                 0,
                                                 0,
                                                 headers);

    if (response == 0 || ! response->isComplete()) {
      errorMsg = "got invalid response from server: " + Client->getErrorMessage();

      if (response != 0) {
        delete response;
      }

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = GetHttpErrorMessage(response);
      delete response;

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;  // just to please the compiler
    bool checkMore = false;
    bool found;
    uint64_t tick;

    // TODO: fix hard-coded headers
    string header = response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded", found);

        if (found) {
          tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          }
          else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (! found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer& body = response->getBody();

      if (! TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      }
      else {
        Stats._totalWritten += (uint64_t) body.length();
      }
    }

    delete response;

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
      // done
      return res;
    }
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a WAL flush request
////////////////////////////////////////////////////////////////////////////////

static void FlushWal () {
  map<string, string> headers;
  const string url = "/_admin/wal/flush?waitForSync=true&waitForCollector=true";

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               nullptr,
                                               0,
                                               headers);

  if (response == nullptr || ! response->isComplete() || response->wasHttpError()) {
    cerr << "got invalid response from server: " + Client->getErrorMessage() << endl;
  }

  if (response != nullptr) {
    delete response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from server
////////////////////////////////////////////////////////////////////////////////

static int RunDump (string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/inventory?includeSystem=" +
                     string(IncludeSystemCollections ? "true" : "false");

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                               url,
                                               0,
                                               0,
                                               headers);

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  FlushWal();

  const StringBuffer& data = response->getBody();


  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str());

  delete response;

  if (! JsonHelper::isObject(json)) {
    if (json != 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }

    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  TRI_json_t const* collections = JsonHelper::getObjectElement(json, "collections");

  if (! JsonHelper::isArray(collections)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  const string tickString = JsonHelper::getStringValue(json, "tick", "");

  if (tickString == "") {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  cout << "Last tick provided by server is: " << tickString << endl;

  // read the server's max tick value
  uint64_t maxTick = StringUtils::uint64(tickString);

  // check if the user specific a max tick value
  if (TickEnd > 0 && maxTick > TickEnd) {
    maxTick = TickEnd;
  }

  // create a lookup table for collections
  map<string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(pair<string, bool>(Collections[i], true));
  }

  // iterate over collections
  const size_t n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);

    if (! JsonHelper::isObject(collection)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

    if (! JsonHelper::isObject(parameters)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    const string cid   = JsonHelper::getStringValue(parameters, "cid", "");
    const string name  = JsonHelper::getStringValue(parameters, "name", "");
    const bool deleted = JsonHelper::getBooleanValue(parameters, "deleted", false);

    if (cid == "" || name == "") {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && ! IncludeSystemCollections) {
      continue;
    }

    if (restrictList.size() > 0 &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    // found a collection!
    if (Progress) {
      cout << "dumping collection '" << name << "'..." << endl;
    }

    // now save the collection meta data and/or the actual data
    Stats._totalCollections++;

    {
      // save meta data
      string fileName;
      fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name + ".structure.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      const string collectionInfo = JsonHelper::toString(collection);

      if (! TRI_WritePointer(fd, collectionInfo.c_str(), collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }


    if (DumpData) {
      // save the actual data
      string fileName;
      fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name + ".data.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      ExtendBatch("");
      int res = DumpCollection(fd, cid, name, parameters, maxTick, errorMsg);

      TRI_CLOSE(fd);

      if (res != TRI_ERROR_NO_ERROR) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return res;
      }
    }
  }


  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single shard, that is a collection on a DBserver
////////////////////////////////////////////////////////////////////////////////

static int DumpShard (int fd,
                      const string& DBserver,
                      const string& name,
                      string& errorMsg) {

  const string baseUrl = "/_api/replication/dump?DBserver=" + DBserver +
                         "&collection=" + name +
                         "&chunkSize=" + StringUtils::itoa(ChunkSize) +
                         "&ticks=false&translateIds=true";

  map<string, string> headers;

  uint64_t fromTick = 0;
  uint64_t maxTick = UINT64_MAX;

  while (1) {
    string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    Stats._totalBatches++;

    SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                                 url,
                                                 0,
                                                 0,
                                                 headers);

    if (response == 0 || ! response->isComplete()) {
      errorMsg = "got invalid response from server: " + Client->getErrorMessage();

      if (response != 0) {
        delete response;
      }

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = GetHttpErrorMessage(response);
      delete response;

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;   // just to please the compiler
    bool checkMore = false;
    bool found;
    uint64_t tick;

    // TODO: fix hard-coded headers
    string header = response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded", found);

        if (found) {
          tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          }
          else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (! found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer& body = response->getBody();

      if (! TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      }
      else {
        Stats._totalWritten += (uint64_t) body.length();
      }
    }

    delete response;

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
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

static int RunClusterDump (string& errorMsg) {
  int res;

  map<string, string> headers;

  const string url = "/_api/replication/clusterInventory?includeSystem=" +
                     string(IncludeSystemCollections ? "true" : "false");

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                               url,
                                               0,
                                               0,
                                               headers);

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
    delete response;

    return TRI_ERROR_INTERNAL;
  }


  const StringBuffer& data = response->getBody();


  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str());

  delete response;

  if (! JsonHelper::isObject(json)) {
    if (json != 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }

    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  TRI_json_t const* collections = JsonHelper::getObjectElement(json, "collections");

  if (! JsonHelper::isArray(collections)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  // create a lookup table for collections
  map<string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(pair<string, bool>(Collections[i], true));
  }

  // iterate over collections
  const size_t n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);

    if (! JsonHelper::isObject(collection)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

    if (! JsonHelper::isObject(parameters)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    const string id    = JsonHelper::getStringValue(parameters, "id", "");
    const string name  = JsonHelper::getStringValue(parameters, "name", "");
    const bool deleted = JsonHelper::getBooleanValue(parameters, "deleted", false);

    if (id == "" || name == "") {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && ! IncludeSystemCollections) {
      continue;
    }

    if (restrictList.size() > 0 &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    // found a collection!
    if (Progress) {
      cout << "dumping collection '" << name << "'..." << endl;
    }

    // now save the collection meta data and/or the actual data
    Stats._totalCollections++;

    {
      // save meta data
      string fileName;
      fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name + ".structure.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      const string collectionInfo = JsonHelper::toString(collection);

      if (! TRI_WritePointer(fd, collectionInfo.c_str(), collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }


    if (DumpData) {
      // save the actual data

      // First we have to go through all the shards, what are they?
      TRI_json_t const* shards = JsonHelper::getObjectElement(parameters,
                                                             "shards");
      map<string, string> shardTab = JsonHelper::stringObject(shards);
      // This is now a map from shardIDs to DBservers

      // Now set up the output file:
      string fileName;
      fileName = OutputDirectory + TRI_DIR_SEPARATOR_STR + name + ".data.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      map<string, string>::iterator it;
      for (it = shardTab.begin(); it != shardTab.end(); it++) {
        string shardName = it->first;
        string DBserver = it->second;
        if (Progress) {
          cout << "dumping shard '" << shardName << "' from DBserver '"
               << DBserver << "' ..." << endl;
        }
        res = StartBatch(DBserver, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          TRI_CLOSE(fd);
          return res;
        }
        res = DumpShard(fd, DBserver, shardName, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          TRI_CLOSE(fd);
          return res;
        }
        EndBatch(DBserver);
      }

      res = TRI_CLOSE(fd);

      if (res != 0) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return res;
      }
    }
  }


  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

static string rewriteLocation (void* data, const string& location) {
  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + BaseClient.databaseName() + location;
  }
  else {
    return "/_db/" + BaseClient.databaseName() + "/" + location;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  arangodumpEntryFunction();

  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

  // .............................................................................
  // set defaults
  // .............................................................................

  int err = 0;
  OutputDirectory = FileUtils::currentDirectory(&err).append(TRI_DIR_SEPARATOR_STR).append("dump");
  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // use a minimum value for batches
  if (ChunkSize < 1024 * 128) {
    ChunkSize = 1024 * 128;
  }

  if (TickStart < TickEnd) {
    cerr << "invalid values for --tick-start or --tick-end" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // create output directory
  // .............................................................................

  bool isDirectory = false;
  if (OutputDirectory != "") {
    isDirectory = TRI_IsDirectory(OutputDirectory.c_str());
  }

  if (OutputDirectory == "" ||
      (TRI_ExistsFile(OutputDirectory.c_str()) && ! isDirectory)) {
    cerr << "cannot write to output directory '" << OutputDirectory << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (isDirectory && ! Overwrite) {
    cerr << "output directory '" << OutputDirectory << "' already exists. use --overwrite to overwrite data in in it" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Connection = GeneralClientConnection::factory(BaseClient.endpointServer(),
                                                BaseClient.requestTimeout(),
                                                BaseClient.connectTimeout(),
                                                ArangoClient::DEFAULT_RETRIES,
                                                BaseClient.sslProtocol());

  if (Connection == nullptr) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  if (Client == nullptr) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Client->setLocationRewriter(0, &rewriteLocation);
  Client->setUserNamePassword("/", BaseClient.username(), BaseClient.password());

  const string versionString = GetArangoVersion();

  if (! Connection->isConnected()) {
    cerr << "Could not connect to endpoint '" << BaseClient.endpointString()
         << "', database: '" << BaseClient.databaseName() << "', username: '" << BaseClient.username() << "'" << endl;
    cerr << "Error message: '" << Client->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // successfully connected
  cout << "Server version: " << versionString << endl;

  // validate server version
  int major = 0;
  int minor = 0;

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    cerr << "invalid server version '" << versionString << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (major < 1 ||
      major > 2 ||
      (major == 1 && minor < 4)) {
    // we can connect to 1.4, 2.0 and higher only
    cerr << "got incompatible server version '" << versionString << "'" << endl;
    if (! Force) {
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    clusterMode = GetArangoIsCluster();
    if (clusterMode) {
      if (TickStart != 0 || TickEnd != 0) {
        cerr << "cannot use tick-start or tick-end on a cluster" << endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }
    }
  }

  if (! Connection->isConnected()) {
    cerr << "Lost connection to endpoint '" << BaseClient.endpointString()
         << "', database: '" << BaseClient.databaseName() << "', username: '" << BaseClient.username() << "'" << endl;
    cerr << "Error message: '" << Client->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (! isDirectory) {
    int res = TRI_CreateDirectory(OutputDirectory.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      cerr << "unable to create output directory '" << OutputDirectory << "': " << string(TRI_errno_string(res)) << endl;
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (Progress) {
    cout << "Connected to ArangoDB '" << BaseClient.endpointString()
          << "', database: '" << BaseClient.databaseName() << "', username: '"
          << BaseClient.username() << "'" << endl;

    cout << "Writing dump to output directory '" << OutputDirectory << "'" << endl;
  }

  memset(&Stats, 0, sizeof(Stats));

  string errorMsg = "";

  int res;

  try {
    if (! clusterMode) {
      res = StartBatch("",errorMsg);
      if (res != TRI_ERROR_NO_ERROR && Force) {
        res = TRI_ERROR_NO_ERROR;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        res = RunDump(errorMsg);
      }

      if (BatchId > 0) {
        EndBatch("");
      }
    }
    else {   // clusterMode == true
      res = RunClusterDump(errorMsg);
    }
  }
  catch (std::exception const& ex) {
    cerr << "caught exception " << ex.what() << endl;
    res = TRI_ERROR_INTERNAL;
  }
  catch (...) {
    cerr << "caught unknown exception" << endl;
    res = TRI_ERROR_INTERNAL;
  }

  if (Progress) {
    if (DumpData) {
      cout << "Processed " << Stats._totalCollections << " collection(s), " <<
              "wrote " << Stats._totalWritten << " byte(s) into datafiles, " <<
              "sent " << Stats._totalBatches << " batch(es)" << endl;
    }
    else {
      cout << "Processed " << Stats._totalCollections << " collection(s)" << endl;
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    cerr << errorMsg << endl;
    ret = EXIT_FAILURE;
  }

  if (Client != nullptr) {
    delete Client;
  }

  TRIAGENS_REST_SHUTDOWN;

  arangodumpExitFunction(ret, nullptr);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
